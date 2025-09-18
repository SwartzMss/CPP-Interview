---
title: 惊群问题与避免策略（条件变量/IO）
tags:
  - concurrency
  - condition_variable
  - synchronization
  - OS
---

## 什么是“惊群”（Thundering Herd）？

当多个线程/进程在等待同一个事件（条件）时，一次事件到来导致大量等待者同时被唤醒；但真正能前进的往往只有极少数，其余很快又睡回去。这会造成 CPU 抢占、缓存抖动、上下文切换增加与延迟上升。

典型场景：

- 条件变量 `notify_all` 唤醒了所有等待者，但谓词只允许 1 个线程继续；其余被唤醒、争锁、发现谓词为假后再次睡眠。
- 多线程/多进程同时 `accept` 同一监听套接字，或多个线程对同一 fd 做 `poll/epoll` 竞争事件。

## 为什么发生？

- C++ 条件变量采用 Mesa 语义：被唤醒后需在锁内重新检查谓词。`notify_all` 会唤醒全部等待者；只有拿到锁且谓词为真的少数能继续，其余都会“空转一次”。
- 内核/IO 层同样存在类似现象：同一触发源上有多个等待者（进程/线程），内核一次事件到来会尝试唤醒多个等待者。

## 通用避免策略

- 首选 `notify_one` 而非 `notify_all`（除停机/广播退出等必须情形）。
- 设计“可重算谓词”，严格在锁内“先改状态→解锁→notify_one”。
- 事件与等待者分片（sharding）：按 key/队列/资源拆分，减少共享等待点。
- 用“计数”表达资源可用度：
  - 如果一次使得 N 个资源变为可用，应当 `notify_one` 重复 N 次，或使用“计数信号量”。
- 使用 C++20 同步原语：`std::counting_semaphore`/`std::binary_semaphore` 自然按配额唤醒单个等待者，适合资源计数型问题。

## 条件变量的最佳实践（多等待者）

- 边界情形才用 `notify_all`：仅用于停机/关闭、或谓词可能让“多数等待者都变真”的情形。
- 对于有界队列：
  - 两个谓词/两个条件变量：`not_empty`（消费者等待）、`not_full`（生产者等待）。
  - 每次生产完成后 `notify_one(not_empty)`；每次消费完成后 `notify_one(not_full)`。
  - 停机时广播：`notify_all` 唤醒所有等待者退出。

示例（MPMC 有界队列）

```cpp
template<class T>
class BoundedQueue {
public:
  explicit BoundedQueue(size_t cap): cap_(cap) {}

  bool push(T v) {
    std::unique_lock<std::mutex> lk(m_);
    not_full_.wait(lk, [&]{ return stop_ || q_.size() < cap_; });
    if (stop_) return false;
    q_.push(std::move(v));
    lk.unlock();
    not_empty_.notify_one(); // 唤醒一个消费者，避免惊群
    return true;
  }

  bool pop(T& out) {
    std::unique_lock<std::mutex> lk(m_);
    not_empty_.wait(lk, [&]{ return stop_ || !q_.empty(); });
    if (q_.empty()) return false; // stop
    out = std::move(q_.front());
    q_.pop();
    lk.unlock();
    not_full_.notify_one(); // 唤醒一个生产者，避免惊群
    return true;
  }

  void stop() {
    { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
    not_empty_.notify_all();
    not_full_.notify_all();
  }

private:
  std::mutex m_;
  std::condition_variable not_empty_, not_full_;
  std::queue<T> q_;
  size_t cap_;
  bool stop_ = false;
};
```

说明：每次只唤醒“需要前进的一个等待者”，把可用度的递增按单位转化为一次 `notify_one`。这能在保证正确性的同时显著减少无谓唤醒。

## IO 层面的惊群与缓解

- 多线程 `accept` 同一监听套接字：
  - 单线程 accept + 任务派发（队列/管道交给工作线程）。
  - Linux: `SO_REUSEPORT` 让内核做连接分流；或 epoll 使用 `EPOLLEXCLUSIVE`（4.5+）降低惊群。
- `epoll`/`kqueue` 等：优先使用“独占/公平”唤醒机制；避免多个循环实体竞争同一事件。

## 何时可以接受 `notify_all`？

- 停机/取消广播，必须唤醒所有等待者以尽快退出。
- 谓词变化使得“几乎所有等待者都满足条件”，且唤醒成本可接受。

## 快速要点

- 单等待者/点对点：`notify_one` + 先改状态后通知；见「条件变量使用规范（单等待者/点对点）」。
- 多等待者：按资源增量唤醒同等数量的等待者；优先信号量或重复 `notify_one`。
- IO：使用 `SO_REUSEPORT`/`EPOLLEXCLUSIVE` 或集中 accept 分发。

