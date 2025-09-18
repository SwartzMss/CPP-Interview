---
title: C++ 零拷贝详解与示例
tags:
  - 性能
  - 调试
  - I/O
  - 网络
---

## 核心概念

- “零拷贝”旨在在应用与设备/内核间传输数据时尽量避免多余的内存复制与上下文切换，降低 CPU 与缓存带宽占用，提高吞吐并降低延迟。
- 常见边界：
  - 进程内零拷贝：组件之间仅传“视图/引用”，不复制底层缓冲。
  - 进程间/文件：多个进程共享同一物理页（`mmap`/共享内存）。
  - 内核 I/O：用页映射/引用传递/DMA，避免“用户态←→内核态”的重复拷贝。
- 实务上常是“少拷贝”，而非绝对零。目标是减少不必要复制与系统调用。

## 常见手段与场景

- 进程内
  - `std::string_view`/`std::span`：只传视图不复制；必须保证被引用缓冲生命周期足够长。
  - 移动语义 `std::move`：转移所有权避免复制（不跨内核边界）。
  - 引用计数缓冲：`std::shared_ptr<uint8_t[]>` 或自定义 buffer pool，降低复制与分配成本。
- 进程间/文件
  - `mmap` 文件或共享内存（`shm_open`/`memfd_create`）：多进程共享页，避免 read 复制到用户态。
- 网络/内核 I/O（Linux）
  - `sendfile(out_sock, in_fd, ...)`：文件→套接字零拷贝（页缓存→NIC DMA）。
  - `splice`/`tee`/`vmsplice`：在 fd 间移动页引用（管道/套接字/文件）。
  - `readv`/`writev`：分散/聚集 I/O，减少系统调用次数（仍可能一次内核复制）。
  - `MSG_ZEROCOPY`（`SO_ZEROCOPY` + `sendmsg`）：大报文由 DMA 直接从用户页发送，内核通过错误队列通知完成；适合大吞吐发送。
  - `io_uring`：注册固定缓冲与部分 `*_ZC` 操作，进一步减少拷贝与系统调用。
- 高性能用户态栈（延伸）
  - DPDK、SPDK；RDMA/verbs、AF_XDP；Folly `IOBuf`、Cap’n Proto（in‑place）。

## 代码示例

说明：以下示例为讲解重点，省略了常规头文件与完整错误处理，仅保留关键调用与结构；实际工程中请补齐必要的 include、检查与事件循环。

### 进程内：只传视图（不复制）

适用：模块间传递只读数据；注意保证原缓冲存活。

```cpp
void handle_text(std::string_view sv) {/* 只读使用 */}
void handle_bytes(std::span<const uint8_t> sp) {/* 只读使用 */}

std::string data = "hello zero-copy";
handle_text(std::string_view{data});          // 无拷贝；data 需存活

std::vector<uint8_t> buf{1,2,3,4};
handle_bytes(std::span<const uint8_t>(buf));  // 无拷贝；buf 需存活
```

### 文件映射：mmap（跨进程/只读共享）

适用：大文件只读处理，多进程共享页缓存，避免 read→用户态复制。

```cpp
const char* path = "big.bin";
int fd = open(path, O_RDONLY);
struct stat st{}; fstat(fd, &st);
void* p = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
close(fd);

std::string_view sv(static_cast<const char*>(p), st.st_size);
// 直接在 sv 上解析，不产生复制

munmap(p, st.st_size);
```

### 文件→socket：sendfile（经典零拷贝）

适用：静态文件分发（HTTP）；避免用户态读写。

```cpp
// 将文件 path 发送到套接字 sockfd（省略错误处理与等待可写）
int send_file_zerocopy(int sockfd, const char* path) {
  int fd = open(path, O_RDONLY);
  struct stat st{}; fstat(fd, &st);
  off_t off = 0;
  while (off < st.st_size) {
    ssize_t n = sendfile(sockfd, fd, &off, st.st_size - off);
    if (n <= 0) break; // 实际工程：处理 EINTR/EAGAIN
  }
  close(fd);
  return 0;
}
```

### 内核页移动：splice（fd 间移动页引用）

适用：需要在多个 fd 之间“串接”（文件→管道→socket），保持零拷贝路径。

```cpp
int splice_file_to_sock(int sockfd, const char* path) {
  int fd = open(path, O_RDONLY | O_NONBLOCK);
  int pfd[2]; pipe(pfd);
  off_t off = 0; struct stat st{}; fstat(fd, &st);
  const size_t chunk = 64 * 1024;
  while (off < st.st_size) {
    ssize_t n = splice(fd, &off, pfd[1], nullptr, chunk, SPLICE_F_MORE);
    size_t left = (n > 0 ? (size_t)n : 0);
    while (left) {
      ssize_t m = splice(pfd[0], nullptr, sockfd, nullptr, left, SPLICE_F_MORE);
      if (m <= 0) break; left -= (size_t)m;
    }
    if (n <= 0) break; // 实际工程：处理 EINTR/EAGAIN
  }
  close(fd); close(pfd[0]); close(pfd[1]);
  return 0;
}
```

### 分散/聚集 I/O：readv/writev（少 syscall）

适用：小包/多段缓冲组合，减少系统调用与一次内核复制。

```cpp
ssize_t write_http_resp(int sockfd, const char* hdr, size_t hlen,
                        const void* body, size_t blen) {
  iovec iov[2];
  iov[0].iov_base = const_cast<char*>(hdr);
  iov[0].iov_len  = hlen;
  iov[1].iov_base = const_cast<void*>(body);
  iov[1].iov_len  = blen;
  return writev(sockfd, iov, 2);
}
```

### 网络发送：MSG_ZEROCOPY（大报文 DMA 直接取用户页）

适用：用户缓冲→网卡，大包/大吞吐；需 Linux ≥ 4.14 且驱动支持；要处理错误队列确认。

要点：

- 对 socket 调用 `setsockopt(SOL_SOCKET, SO_ZEROCOPY, ...)` 开启。
- 以 `sendmsg(..., MSG_ZEROCOPY)` 发送；内核在错误队列（`MSG_ERRQUEUE`）返回完成通知。
- 小报文不划算；需要轮询/事件读取错误队列来回收与统计完成。

```cpp
static int enable_zerocopy(int sockfd) {
  int one = 1;
  return setsockopt(sockfd, SOL_SOCKET, SO_ZEROCOPY, &one, sizeof(one));
}

// 读取错误队列，统计 zerocopy 完成个数（示意）
static int drain_errqueue(int sockfd) {
  int completions = 0;
  while (true) {
    char ctrl[256]; msghdr msg{}; iovec iov{}; char dummy;
    iov.iov_base = &dummy; iov.iov_len = sizeof(dummy);
    msg.msg_iov = &iov; msg.msg_iovlen = 1; msg.msg_control = ctrl; msg.msg_controllen = sizeof(ctrl);
    ssize_t n = recvmsg(sockfd, &msg, MSG_ERRQUEUE | MSG_DONTWAIT);
    if (n <= 0) break; // 实际工程：区分 EAGAIN/EWOULDBLOCK
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg); cmsg; cmsg = CMSG_NXTHDR(&msg, cmsg)) (void)cmsg, ++completions;
  }
  return completions;
}

static ssize_t send_big_zerocopy(int sockfd, const void* buf, size_t len) {
  msghdr msg{}; iovec iov{}; iov.iov_base = const_cast<void*>(buf); iov.iov_len = len;
  msg.msg_iov = &iov; msg.msg_iovlen = 1;
  return sendmsg(sockfd, &msg, MSG_ZEROCOPY);
}

// 连接、启用 SO_ZEROCOPY 后：
char* big = (char*)aligned_alloc(4096, 8u * 1024 * 1024);
memset(big, 'Z', 8u * 1024 * 1024);
send_big_zerocopy(sockfd, big, 8u * 1024 * 1024);
int done = drain_errqueue(sockfd);
free(big);
```

### io_uring（注册缓冲与低开销提交）

适用：降低系统调用开销，结合固定缓冲/文件描述符；部分内核和库提供 `SENDZC`/`*_ZC` 能力以进一步减少复制。

要点：

- 预注册固定缓冲（`IORING_REGISTER_BUFFERS`）和/或固定文件；提交 `send`/`recv`/`splice` 等 SQE；从 CQE 读取完成。
- 与 `MSG_ZEROCOPY` 并非互斥：发送大块用户缓冲可考虑二者配合（依内核版本与接口支持）。

```cpp
// 伪代码（liburing），演示关键步骤，省略错误处理
io_uring ring; io_uring_queue_init(256, &ring, 0);

// 注册固定缓冲（可选）
iovec bufs[1]; bufs[0].iov_base = big_buf; bufs[0].iov_len = big_len;
io_uring_register_buffers(&ring, bufs, 1);

// 建立连接（略），可对 socket 启用 SO_ZEROCOPY（可选，视内核/驱动而定）

// 提交 send
io_uring_sqe* sqe = io_uring_get_sqe(&ring);
io_uring_prep_send(sqe, sockfd, big_buf, big_len, 0);
// 若使用固定缓冲：io_uring_prep_send(sqe, sockfd, nullptr, big_len, 0);
// 并设置 sqe->buf_index = 0; sqe->flags |= IOSQE_BUFFER_SELECT;
io_uring_submit(&ring);

// 取完成
io_uring_cqe* cqe; io_uring_wait_cqe(&ring, &cqe);
int res = cqe->res; io_uring_cqe_seen(&ring, cqe);

io_uring_queue_exit(&ring);
```

提示：io_uring 需要较新的内核（≥5.10 更成熟），部署时关注权限、`RLIMIT_MEMLOCK`、注册缓冲数量限制以及回退路径（出错时退回常规 `send`）。

## 选择建议

- 静态文件/大对象下发：优先 `sendfile`；若需管道编排，考虑 `splice`。
- 高吞吐网络（≥10GbE）且数据来自用户缓冲：`MSG_ZEROCOPY` 或 `io_uring` + 注册缓冲。
- 多进程共享大块只读数据：`mmap` 文件或 `shm_open` 共享内存。
- 小报文/繁杂控制流：`readv`/`writev` 降低 syscall 开销，收益稳定且实现简单。
- 进程内组件传参：`string_view`/`span`/引用计数缓冲，避免复制与重复分配。

## 坑与权衡

- 生命周期与并发：视图/共享页易悬挂；必须清晰约定所有权与存续期。
- 小包不划算：`MSG_ZEROCOPY` 对小数据可能更慢（管理开销、完成通知成本）。
- 页固定与内存压力：零拷贝常需 pin page，过量会影响系统回收与 NUMA 行为。
- 可观测性：需要统计零拷贝命中率、失败回退路径与重传成本。

## 性能与调试

- 度量指标
  - CPU 使用（系统态 vs 用户态）、每核吞吐（Gbps / Mbps）、P99/P999 延迟。
  - socket 层统计：发送队列长度、重传、`tcp_info`（`getsockopt(TCP_INFO)`）。
  - 零拷贝命中率：`MSG_ERRQUEUE` 完成数 / 发送数；sendfile/splice 成功返回与回退路径统计。
- 观察工具
  - `perf record/report`：采样热点、系统调用与 cache-miss。
  - `perf trace`/`strace -k`：跟踪 syscalls 与阻塞点。
  - `bpftool`/`bcc`：kprobe/tracepoint 观测 TCP、`sendfile`、`splice` 路径。
  - `sar`/`iostat`/`vmstat`：磁盘/CPU/内存总体压力。
- 常见定位思路
  - 吞吐不升：检查是否命中零拷贝（错误队列回执/统计）、是否被小包化/合并阻碍（Nagle、GSO/TSO）。
  - 延迟变差：检查页缺页/缺乏对齐、NUMA 远端访问、pin 过多导致回收压力。
  - CPU 居高：确认是否仍在用户态做 memcpy，或存在过多 syscalls（考虑聚合 I/O）。
