---
title: 条件变量使用规范（单等待者/点对点）
tags:
  - concurrency
  - condition_variable
---

## 问题

只有一个等待线程与一个通知线程时，条件变量应如何正确使用，既要抗虚假唤醒、防丢唤醒，又保持实现简洁？

## 回答

核心规范（单等待者/点对点）：同锁保护的可重算谓词 + while（或 `wait(lock, pred)`）等待 + 先改状态后通知（`notify_one`）。

### 适用范围

- 一个等待线程与一个通知线程，无“惊群”（thundering herd）问题。
- 典型：点对点交付、单生产者-单消费者。

### 核心规则

- 同锁保护：谓词只依赖同一把互斥锁保护的数据。
- 抗虚假唤醒：`while(!pred()) cv.wait(lock);` 或使用 `cv.wait(lock, pred)`。
- 防丢唤醒：通知方在同锁内先把状态改到“谓词为真”，再解锁并 `notify_one()`。
- 通知策略：只有一个等待者，默认 `notify_one`；停机/关闭需要所有等待者前进时用 `notify_all`。

### 谓词设计（可重算）

- 单槽：`has_item == true`
- 队列：消费者 `!queue.empty()`；生产者 `queue.size() < cap`
- 停止：`stop/closed` 并入谓词，统一退出通道

### 等待/通知流程

- 等待方：锁 → `cv.wait(lk, [&]{ return stop || pred(); });` → 处理 → 改状态（使谓词变假）
- 通知方：锁内改状态（使谓词变真）→ 解锁 → `cv.notify_one()`

### 停机与超时

- 停机：`{ lock; stop=true; } cv.notify_all();`（确保尽快唤醒退出）
- 超时：`wait_for/until` 返回后仍需 while 重检谓词（返回值仅作辅助）

### 常见误区

- 用 `if` 替代 `while` 检查谓词。
- 等待与通知使用不同的锁，或谓词未受该锁保护。
- 不维护可重算状态，仅依赖“被通知过”。
- 先通知后改状态，或状态未改变却调用 `notify_*`。

### 示例（单槽信箱，点对点交付）

说明：仅展示核心逻辑，省略头文件；一个线程 `send`，一个线程 `recv`。

```cpp
template <class T>
class SingleSlotMailbox {
public:
  bool send(T v) {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&]{ return stop_ || !slot_.has_value(); });
    if (stop_) return false;
    slot_.emplace(std::move(v));   // 改状态：槽变非空
    lk.unlock();
    cv_.notify_one();              // 唤醒接收者
    return true;
  }

  bool recv(T& out) {
    std::unique_lock<std::mutex> lk(m_);
    cv_.wait(lk, [&]{ return stop_ || slot_.has_value(); });
    if (!slot_) return false;      // stop_ 且无值
    out = std::move(*slot_);
    slot_.reset();                 // 改状态：槽变空
    lk.unlock();
    cv_.notify_one();              // 唤醒发送者
    return true;
  }

  void stop() {
    { std::lock_guard<std::mutex> lk(m_); stop_ = true; }
    cv_.notify_all();              // 广播退出
  }

private:
  std::mutex m_;
  std::condition_variable cv_;
  std::optional<T> slot_;
  bool stop_ = false;
};
```

要点回顾：

- 谓词（可重算）：发送侧“槽为空”，接收侧“槽非空”，两者都并入 `stop_`。
- `cv.wait(lock, pred)` 等价于 while+wait，抗虚假唤醒。
- 改状态在锁内完成，先改后通知，避免丢唤醒。

延伸阅读：

- 惊群问题与避免策略（条件变量/IO）：language/thundering_herd.md
