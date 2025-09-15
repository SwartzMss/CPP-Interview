---
title: Qt 信号槽常见坑与最佳实践
tags:
  - qt
---

## 常见坑位

- 重负载槽配 Direct/同线程 Auto：
  - 卡住发射线程（可能是 UI）；改为 Queued 或下放到 Worker。
- 跨线程 Direct：
  - 槽在发射线程执行，可能越线程访问接收者；跨线程应用 Queued/BlockingQueued。
- 子线程无事件循环：
  - Queued 的槽不执行；子线程需进入 `QThread::exec()`。
- 自定义参数未注册：
  - 队列连接跨线程传参需 `Q_DECLARE_METATYPE` + `qRegisterMetaType<T>()`。
- 误用 BlockingQueued：
  - 同线程死锁；不同线程也可能因锁顺序错误死锁；仅在确需同步等待时使用。
- 重复连接：
  - 导致槽多次触发；用 `Qt::UniqueConnection` 或在连接前断开旧连接。
- lambda 捕获悬垂：
  - 捕获裸指针生命周期不受控；使用接收者作为上下文对象（自动断连）或 `QPointer`。
- 在子线程触碰 UI 对象：
  - 未定义行为；所有 UI 更新切回 GUI 线程。

## 最佳实践

- 连接类型选型：默认 Auto；跨线程显式 Queued；等待结果用 BlockingQueued（谨慎）。
- 线程架构：Worker 对象移到子线程；结果通过信号回 UI。
- 轻重分离：耗时任务放后台；UI 线程仅做轻量更新；需要时切片（`QTimer::singleShot(0, ...)`）。
- 参数传递：跨线程避免大对象拷贝，优先轻量数据或共享指针。
- 连接管理：保存 `QMetaObject::Connection` 以便断开；或用上下文对象自动断连。
- 语法选择：优先“新语法”（函数指针/成员指针）获得编译期检查；重载信号用 `QOverload<>` 辅助。

## 参考与关联

- 《[Qt 信号槽机制（原理与语法）](signals_and_slots.md)》
- 《[Qt 连接类型详解](connection_types.md)》
- 《[跨线程更新 UI 的安全方式？](cross_thread_ui_update.md)》

