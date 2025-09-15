---
title: CPP-Interview
---

# CPP-Interview
欢迎来到C++面试指南。本指南涵盖了C++的重要特性以及常见面试问题。

## Qt 专栏（新）

- 《[Qt 信号槽机制（原理与语法）](qt/signals_and_slots.md)》：moc/activate、新语法、上下文与断连、重载处理
- 《[Qt 连接类型详解](qt/connection_types.md)》：Auto/Direct/Queued/BlockingQueued/Unique、跨线程 Direct 风险与选型
- 《[跨线程更新 UI 的安全方式？](qt/cross_thread_ui_update.md)》：Queued/invokeMethod/singleShot、QThread+Worker、QtConcurrent/QFutureWatcher
- 《[信号槽常见坑与最佳实践](qt/pitfalls_best_practices.md)》：易错点与修正、死锁规避、参数注册、重复连接等
- 《[QThread 正确使用姿势与常见误区](qt/qthread_usage.md)》：Worker+moveToThread 模板、事件循环与亲和性、取消与收尾、何时继承 QThread
 - 《[QThread 和线程池如何选择？](qt/when_use_qthread_vs_threadpool.md)》：一次性短任务/CPU 密集用线程池，事件驱动/长期驻留用 QThread+Worker
 - 《[为何不把业务放进 QThread 子类？](qt/why_not_put_business_in_qthread.md)》：线程亲和性与事件模型原因、推荐 Worker+moveToThread
 - 《[如何安全地停止线程任务？](qt/how_to_stop_thread_safely.md)》：协作式取消、可中断等待、资源回收与 UI 善后
