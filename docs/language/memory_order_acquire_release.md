---
title: std::memory_order_release / acquire 的使用场景
tags:
  - language
---

# `std::memory_order_release / acquire` 的使用场景

## 核心结论

- 用于“发布数据/获取数据”的同步：写线程先写一批普通数据，然后对某个原子变量做 release 写；读线程对同一原子变量做 acquire 读，再读那批数据，从而保证可见性与顺序（建立 happens-before）。
- 仅适用于原子操作或内存栅栏：`std::atomic<T>` 的 `load/store/RMW/compare_exchange`，以及 `std::atomic_thread_fence`。
- 只关心原子变量自身的数值且与其他数据无依赖时，可用 `relaxed`。
- 需要全局线性化时用默认 `seq_cst`；复杂临界区直接用 `std::mutex`。

## 何时用 release/acquire（心法）

- 写线程：先写共享数据 → `flag.store(true, std::memory_order_release)`
- 读线程：`while (!flag.load(std::memory_order_acquire))` 等就绪 → 再读共享数据
- 必须是同一个“同步点”（同一个原子对象）跨线程配对，才能对前后普通读写生效。

## 典型模式与示例

### 1) 发布就绪标志（发布-订阅）

```cpp
#include <atomic>
#include <thread>
#include <string>
#include <iostream>

struct Payload { int x; std::string s; };

Payload data;                 // 非原子数据
std::atomic<bool> ready{false};

void producer() {
    data.x = 42;
    data.s = "hello";                               // 普通写
    ready.store(true, std::memory_order_release);    // 发布
}

void consumer() {
    while (!ready.load(std::memory_order_acquire)) {}// 获取
    // acquire 之后，能看见 producer 对 data 的写入
    std::cout << data.x << " " << data.s << "\n";   // 安全读取 42 hello
}
```

### 2) 交接所有权（发布指针/句柄）

```cpp
#include <atomic>
struct Node { int v; /* ... */ };
std::atomic<Node*> head{nullptr};

void make_and_publish() {
    Node* n = new Node{123};
    head.store(n, std::memory_order_release); // 发布：构造对读者可见
}

void use_after_acquire() {
    Node* n = nullptr;
    while (!(n = head.load(std::memory_order_acquire))) {}
    int x = n->v;  // 安全读取 123
}
```

### 3) 单生产者单消费者（SPSC）环形队列索引发布

```cpp
#include <atomic>
#include <array>
constexpr size_t N = 1024;
std::array<int, N> buf;
std::atomic<size_t> w{0}, r{0};

void producer(int x) {
    auto wi = w.load(std::memory_order_relaxed);
    auto ri = r.load(std::memory_order_acquire);            // 看到最新 r 防越界
    while (((wi + 1) % N) == ri) ri = r.load(std::memory_order_acquire); // 满
    buf[wi] = x;                                            // 普通写
    w.store((wi + 1) % N, std::memory_order_release);       // 发布写入完成
}

int consumer() {
    auto ri = r.load(std::memory_order_relaxed);
    auto wi = w.load(std::memory_order_acquire);            // 获取以看到 buf 写入
    while (ri == wi) wi = w.load(std::memory_order_acquire); // 空
    int x = buf[ri];                                        // 安全读
    r.store((ri + 1) % N, std::memory_order_release);       // 发布消费进度
    return x;
}
```

### 4) 轻量自旋锁（获取 acquire，释放 release）

```cpp
#include <atomic>
std::atomic_flag spin = ATOMIC_FLAG_INIT;

void lock()   { while (spin.test_and_set(std::memory_order_acquire)) {} }
void unlock() {        spin.clear(std::memory_order_release);            }
```

### 5) 仅计数/统计（可用 relaxed）

```cpp
#include <atomic>
std::atomic<int> counter{0};

void worker() {
    for (int i = 0; i < 100000; ++i)
        counter.fetch_add(1, std::memory_order_relaxed); // 只需数值正确
}
```

## 与 `std::atomic` 的关系

- 内存序参数仅用于：
  - `std::atomic<T>` 的原子操作（`load` 可 `acquire/relaxed/seq_cst`；`store` 可 `release/relaxed/seq_cst`；读改写类常用 `acq_rel`），以及
  - `std::atomic_thread_fence(order)`（跨多个原子建立更广义的屏障，少数场景使用）。
- 普通变量的读写不接受 `memory_order`；要受其保护，必须通过与之配对的原子操作建立同步关系。

## 何时不要用 acquire/release

- 仅关心原子自身值、不依赖其他数据可见性：用 `relaxed` 更快。
- 需要全局时序直观与调试方便：使用默认 `seq_cst`。
- 保护复杂不变量/大临界区：用 `std::mutex`/`std::lock_guard` 更安全、省心。

## 误区：`volatile` 不是并发同步

- `volatile` 只意味着“每次都真的访问内存”，不保证原子性/可见性/顺序；跨线程对同一对象的非原子读写（即便 `volatile`）是数据竞争，行为未定义。

反例与正确做法：

```cpp
#include <thread>
#include <atomic>

volatile bool stop_bad = false; // ❌ 非原子，存在数据竞争
std::atomic<bool> stop{false};  // ✅ 原子

int main() {
    std::thread t1([]{
        while (!stop.load(std::memory_order_acquire)) {}
    });
    stop.store(true, std::memory_order_release);
    t1.join();
}
```

## 面试速记

- “发布就绪标志用 release，等待就绪用 acquire”。
- 需要从写线程“带出”其他普通写入：`store(release)` + `load(acquire)` 成对使用。
- 只统计不依赖其他数据可见性：`relaxed`。
- 复杂同步别硬凹内存序：用锁或保持 `seq_cst`。
