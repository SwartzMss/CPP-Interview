---
title: Qt 专栏
tags:
  - qt
---

# Qt 专栏概览

收录常见 Qt 面试/实践要点与示例，聚焦信号槽与跨线程 UI 安全更新。

- 信号槽机制与连接类型：
  - 基础原理（moc、QMetaObject、事件循环）
  - 连接类型 Auto/Direct/Queued/BlockingQueued、UniqueConnection
  - Direct 不等于同线程；跨线程 Direct 的风险与替代方案
  - 示例与常见坑位
  - 详见《[Qt 信号槽机制与连接类型？](signals_and_slots.md)》

- 跨线程更新 UI 的安全方式：
  - 信号槽（Queued）、invokeMethod、singleShot、postEvent
  - QThread + Worker 架构、QtConcurrent + QFutureWatcher
  - 重负载槽处理与切片更新、避免死锁
  - 详见《[跨线程更新 UI 的安全方式？](cross_thread_ui_update.md)》

