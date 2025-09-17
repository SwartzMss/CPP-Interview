---
title: Qt 连接类型详解
tags:
  - qt
---

## 问题

Qt 中有哪些连接类型？它们分别在哪个线程调用槽？如何正确选型？

## 回答

### 1) 行为定义（在哪个线程运行槽）

- Qt::AutoConnection（默认）
  - 同线程时等同 Direct；不同线程时等同 Queued。
- Qt::DirectConnection（直接）
  - 槽在“发射信号的线程”里同步执行。Direct 不等于“同线程”。若接收者属于其他线程，则会出现“跨线程 Direct”，容易越线程访问接收者对象。
- Qt::QueuedConnection（队列）
  - 调用被封装为事件，投递到“接收者所属线程”的事件循环；槽在接收者线程异步执行。
- Qt::BlockingQueuedConnection（阻塞队列）
  - 与 Queued 相同，但发射线程会阻塞等待槽执行完成。仅用于不同线程之间；同线程会死锁。
- Qt::UniqueConnection（去重标志）
  - 与上述按位或使用（如 `Qt::AutoConnection | Qt::UniqueConnection`），避免重复连接。

线程亲和性：`QObject::thread()` 指明对象归属线程。Queued/BlockingQueued 会把调用搬到接收者线程；Direct 则不会搬运，始终在发射线程执行。

### 2) 参数与类型要求

- 对于队列连接，信号参数会被拷贝并跨线程传递；自定义类型需要 `Q_DECLARE_METATYPE(T)` 并在运行时 `qRegisterMetaType<T>("T")`。
- 大对象参数建议传共享指针或轻量数据以减少拷贝成本。

### 3) 选型建议

- 默认：Auto。
- 跨线程切回 UI：Queued。
- 跨线程且需要等待结果：BlockingQueued（谨慎、避免死锁）。
- 防重复连接：UniqueConnection。
- 重负载槽：不要 Direct；放到 Worker 线程或使用 Queued。

### 4) 示例

跨线程 Direct（反例，不推荐）：

```cpp
QThread th; Worker w; w.moveToThread(&th); th.start();
// 槽会在 sender 所在线程执行，而 w 归 th 线程 => 越线程访问
QObject::connect(&sender, &Sender::sig, &w, &Worker::slot, Qt::DirectConnection);
```

正确：使用 Auto/Queued，槽在接收者线程：

```cpp
QObject::connect(&sender, &Sender::sig, &w, &Worker::slot); // Auto => Queued
```

阻塞等待工作线程返回（谨慎）：

```cpp
QObject::connect(&sender, &Sender::sig, &w, &Worker::slot, Qt::BlockingQueuedConnection);
```

### 5) 关联主题

- 机制与语法：参见《[Qt 信号槽机制（原理与语法）](signals_and_slots.md)》
- 跨线程 UI 安全更新：参见《[跨线程更新 UI 的安全方式？](cross_thread_ui_update.md)》
- 坑位清单：参见《[信号槽常见坑与最佳实践](pitfalls_best_practices.md)》
