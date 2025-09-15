---
title: 为什么不推荐把业务对象/槽放在 QThread 子类里？
tags:
  - qt
---

## 问题

很多教程示例里在继承 `QThread` 的类里写槽和业务逻辑，这样可以吗？

## 回答

### 不推荐的原因

- 线程亲和性混淆：`QThread` 是“线程句柄”，其自身的 `QObject` 部分属于创建它的线程（通常是 GUI 线程），不是子线程。
- 业务放在 `QThread` 子类里，容易误以为槽会自动在子线程执行，导致跨线程访问、越线程更新 UI 等问题。
- 覆盖 `run()` 后常见长阻塞，队列事件/定时器无法处理，和 Qt 事件驱动模型冲突。

### 推荐做法

- 使用 Worker + `moveToThread()`：
  - 业务对象是 `QObject`，移动到目标线程；通过信号槽通信，队列连接保证在所属线程运行。
  - `QThread` 只负责线程生命周期（启动/退出/等待）。

### 示例对比

- 反例（把业务放进 QThread）：
```cpp
class MyThread : public QThread {
public slots:
    void doWork(); // 槽默认属于创建线程，不会自动在子线程执行
};
```

- 正例：
```cpp
class Worker : public QObject { Q_OBJECT public slots: void doWork(); };
QThread th; auto* w = new Worker; w->moveToThread(&th);
QObject::connect(&th, &QThread::started, w, &Worker::doWork);
th.start();
```

### 何时需要继承 QThread

- 需要自定义 `run()`/自定义事件循环的极少数场景，例如内部管理专用 loop 或集成非 Qt 事件源。
- 即使继承，也应将业务对象仍放在独立 `QObject` 中，避免在 `QThread` 子类混入业务。

### 关联

- 《[QThread 正确使用姿势与常见误区](qthread_usage.md)》
- 《[Qt 信号槽常见坑与最佳实践](pitfalls_best_practices.md)》

