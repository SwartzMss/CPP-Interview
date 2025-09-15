---
title: QtConcurrent 和 QThreadPool 的区别与选择？
tags:
  - qt
---

## 问题

`QtConcurrent` 和 `QThreadPool` 有什么区别？在什么场景下选择哪一个？

## 回答

### 1) 定位与关系

- QtConcurrent：高层“声明式并发”接口（run/map/mapReduced/filter 等），返回 `QFuture`；默认基于全局 `QThreadPool` 实现。
- QThreadPool：底层“线程池执行器”，提交 `QRunnable` 或可调用对象，负责线程与任务队列管理。

结论：QtConcurrent 通常用线程池跑任务；QThreadPool 是执行器，QtConcurrent 是易用的封装。

### 2) 编程模型对比

- QtConcurrent：
  - 直接传函数/容器，自动切分与并行调度。
  - 产出 `QFuture`，配合 `QFutureWatcher` 获取 `finished/progress/canceled` 等信号。
  - 适合一次性短任务、并行 map/reduce、结果聚合。
- QThreadPool：
  - 手动封装任务（`QRunnable`/可调用），可设置最大线程数、优先级。
  - 无内置 future，需要自建信号/回调/同步来传结果与进度。
  - 适合自定义调度策略、任务分片和生命周期控制。

共同点：线程池线程默认无事件循环；如需事件驱动/定时器，应改用 `QThread + Worker`。

### 3) 取消、进度与结果

- QtConcurrent：
  - Qt6 支持 `QFuture::cancel()/isCanceled()`（任务需协作检查）。
  - 自动聚合结果（如 `mapped()`/`filtered()`）。
  - 通过 `QFutureWatcher` 在 UI 线程处理完成与进度。
- QThreadPool：
  - 取消需自定义协议（原子标志、条件变量等）。
  - 结果与进度需自建信号/回调。

### 4) 选型建议

- 声明式并发、要结果/进度/取消：优先 QtConcurrent（+ QFutureWatcher）。
- 特别小而多的任务、需要精细控制分片与队列：用 QThreadPool 手工批处理更灵活。
- CPU 密集批处理：两者都可以；控制并发度（池大小≈核心数）。
- 需要事件循环/长期驻留对象：都不合适，改用 `QThread + Worker::moveToThread()`。

### 5) 示例

- QtConcurrent：
```cpp
auto fut = QtConcurrent::run([](){ /* work */ return 42; });
auto* watcher = new QFutureWatcher<int>(parent);
QObject::connect(watcher, &QFutureWatcher<int>::finished, ui, [=]{
    ui->showValue(fut.result());
    watcher->deleteLater();
});
watcher->setFuture(fut);
```

- QThreadPool：
```cpp
class Task : public QRunnable {
public:
    void run() override { /* work */ /* emit via QObject bridge if needed */ }
};
QThreadPool::globalInstance()->start(new Task());
// 或设置自定义池：pool.setMaxThreadCount(n); pool.start(new Task());
```

### 6) 关联

- 《[QThread 和线程池如何选择？](when_use_qthread_vs_threadpool.md)》
- 《[QThread 正确使用姿势与常见误区](qthread_usage.md)》
- 《[跨线程更新 UI 的安全方式？](cross_thread_ui_update.md)》

