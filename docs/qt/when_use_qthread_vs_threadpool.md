---
title: QThread 和线程池（QtConcurrent/QThreadPool）如何选择？
tags:
  - qt
---

## 问题

什么时候用 `QThread`（长期驻留 Worker），什么时候用 `QThreadPool`/`QtConcurrent`（线程池）？

## 回答

### 选择建议

- 一次性、短任务、多批并发：优先 `QThreadPool` 或 `QtConcurrent::run()`。
  - 线程资源复用、创建销毁成本低，API 简洁；配合 `QFutureWatcher` 在 UI 线程处理完成/进度。
- 长期驻留、需要事件循环/状态机/信号交互的后台对象：`QThread + Worker::moveToThread()`。
  - 例如：持续监听、长连接、设备读写、需要接收定时器/事件等。
- CPU 密集批处理：优先线程池并控制并发度（池大小≈核心数）。
- I/O 阻塞型长期任务：`QThread + Worker` 更适配，可细粒度控制生命周期与取消。

### 对比要点

- 线程管理：线程池自动调度；QThread 需手动 `start/quit/wait` 与对象清理。
- 通信模式：线程池更偏函数式（future），QThread 更偏对象/事件式（signal/slot）。
- 事件循环：线程池任务默认无事件循环；QThread 线程有事件循环（未覆盖 run）。

### 示例

- 线程池：
```cpp
auto fut = QtConcurrent::run([]{ /* work */ return 42; });
auto* w = new QFutureWatcher<int>(parent);
QObject::connect(w, &QFutureWatcher<int>::finished, ui, [=]{ ui->show(fut.result()); w->deleteLater(); });
w->setFuture(fut);
```

- QThread + Worker：
```cpp
QThread th; auto* worker = new Worker; worker->moveToThread(&th);
QObject::connect(&th, &QThread::started, worker, &Worker::start);
th.start();
```

### 关联

- 《[QThread 正确使用姿势与常见误区](qthread_usage.md)》
- 《[跨线程更新 UI 的安全方式？](cross_thread_ui_update.md)》
- 《[Qt 连接类型详解](connection_types.md)》

