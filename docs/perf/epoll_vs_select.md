---
title: epoll vs. select：性能要点与示例
tags:
  - 性能
  - 调试
  - 网络
---

## 核心差异概览

- `select` 使用位图 `fd_set`，每次调用都要复制整块 fd 集合进内核并线性扫描，复杂度近似 O(监控 fd 数)。`FD_SETSIZE`（默认 1024）限制了可监控的 fd 数，且跨线程共享困难。
- `epoll` 维护内核态红黑树 + 就绪链表。`epoll_ctl` 只在增删改时调整红黑树，`epoll_wait` 直接返回就绪列表，复杂度接近 O(就绪 fd 数)。fd 几乎只受系统总句柄数限制。
- 事件语义：`select` 仅做水平触发；`epoll` 既可水平触发（默认，安全）也可边缘触发（`EPOLLET`，降低唤醒但需 drain buffer）。`EPOLLONESHOT`、`EPOLLEXCLUSIVE` 等标志进一步控制并发和惊群。
- 资源开销差异：`select` 每次调用都重复拷贝和扫描；`epoll` 注册一次后事件直接回调到 ready-list，内核空间按 fd 分配少量节点结构。

## 典型使用场景

- 连接规模 < 1K、需要快速原型或跨平台（例如 POSIX、老 Unix、Windows）时，`select`/`poll` 足够且 API 简单。
- Linux 上的高并发长连接（IM、推送、网关、反向代理、游戏服）应优先选择 `epoll`，配合非阻塞 fd 与线程池/Reactor 架构扩展到数万乃至更多活跃连接。
- 需要统一编程模型的跨平台库（libevent、boost.asio）通常在 Linux 下封装 `epoll`，在 BSD 下封装 `kqueue`，在 Windows 下封装 IOCP，通过抽象层隐藏差异。

## 性能与调试要点

- `select` 每次都重新构造 `fd_set`，忘记重新添加 fd 会直接漏事件。调试简单但扩展成本高。
- `epoll` 默认水平触发，若切换到边缘触发必须：
  - 将 fd 设为非阻塞；
  - 在收到事件后循环 `read`/`write` 直到命中 `EAGAIN`，否则会丢事件。
- 多线程同时 `epoll_wait` 同一 fd 可能出现惊群，可用 `EPOLLEXCLUSIVE`（Linux 4.5+）或每线程独立 `epoll` + 接入层负载均衡。
- ready-list 若持续堆积，说明用户态处理不过来：需要扩展线程池、分片多个 `epoll` 实例或限流上游连接。

## 最简单的示例代码

以下示例展示了用 `select` 和 `epoll` 监听标准输入/监听套接字的最小化写法，仅保留关键逻辑，省略了错误处理与 `close`/`cleanup`。

```cpp
// select 版本（水平触发，每次都要重建 fd 集合）
int main() {
  int listen_fd = setup_listen_socket(); // 伪函数：创建 + bind + listen
  while (true) {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(listen_fd, &readfds);
    int max_fd = std::max(STDIN_FILENO, listen_fd);

    int ready = select(max_fd + 1, &readfds, nullptr, nullptr, nullptr);
    if (ready <= 0) continue; // 实际工程要区分 EINTR

    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      std::string line;
      std::getline(std::cin, line);
      handle_cli_command(line); // 处理输入命令
    }

    if (FD_ISSET(listen_fd, &readfds)) {
      int conn_fd = accept(listen_fd, nullptr, nullptr);
      handle_new_conn(conn_fd);
    }
  }
}
```

```cpp
// epoll 版本（注册一次，wait 直接拿 ready-list）
int main() {
  int listen_fd = setup_listen_socket(); // 伪函数：创建 + bind + listen
  int epfd = epoll_create1(0);

  auto add_fd = [&](int fd, uint32_t events) {
    epoll_event ev{};
    ev.events = events;
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
  };

  add_fd(STDIN_FILENO, EPOLLIN); // 水平触发
  add_fd(listen_fd, EPOLLIN);

  std::array<epoll_event, 16> events; // 简化起见
  while (true) {
    int ready = epoll_wait(epfd, events.data(), events.size(), -1);
    for (int i = 0; i < ready; ++i) {
      int fd = events[i].data.fd;
      uint32_t ev = events[i].events;

      if (fd == STDIN_FILENO && (ev & EPOLLIN)) {
        std::string line;
        std::getline(std::cin, line);
        handle_cli_command(line);
      } else if (fd == listen_fd && (ev & EPOLLIN)) {
        while (true) {
          int conn_fd = accept4(listen_fd, nullptr, nullptr, SOCK_NONBLOCK);
          if (conn_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            // 真实工程：处理 EINTR 等
            break;
          }
          handle_new_conn(conn_fd);
        }
      }
    }
  }
}
```

> 小贴士
>
> - 若要切换 `EPOLLET`（边缘触发），需要在 `add_fd` 时添加 `EPOLLET` 并保证 `accept4`/`read`/`write` 循环读写到 `EAGAIN` 为止。
> - 示例中省略了资源释放与深入的错误处理，真实服务需使用 RAII、`unique_fd` 等封装保证异常安全。

## 面试答题思路（可复用）

1. 先描述多路复用问题：单线程要同时感知多个 fd 的可读可写。
2. 对比 `select`/`poll` 与 `epoll` 的数据结构、复杂度、限制和触发方式。
3. 补充调优点：非阻塞、边缘触发注意事项、惊群、防止 ready-list 堆积。
4. 给出一段实际项目案例（例如从 `select` 升级 `epoll` 后 CPU 降了多少、尾延迟改善等），突出工程经验与排障手段。
