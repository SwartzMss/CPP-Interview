---
title: CPP-Interview
---

# CPP-Interview
欢迎来到C++面试指南。本指南涵盖了C++的重要特性以及常见面试问题。

## Qt 专栏（新）

- 《[Qt 信号槽机制与连接类型？](qt/signals_and_slots.md)》：
  - 连接类型 Auto/Direct/Queued/BlockingQueued，UniqueConnection
  - Direct 在“发射线程”执行，警惕跨线程 Direct 风险与替代方案
- 《[跨线程更新 UI 的安全方式？](qt/cross_thread_ui_update.md)》：
  - 使用 Queued/invokeMethod/singleShot/QtConcurrent 等切回 GUI 线程
  - QThread + Worker 架构、QFutureWatcher 示例、避免死锁与卡顿
