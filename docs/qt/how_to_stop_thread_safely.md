---
title: 如何安全地停止正在运行的线程任务？
tags:
  - qt
---

## 问题

Qt 中如何安全停止一个正在运行的后台任务？

## 回答

### 核心原则

- 协作式取消：任务周期性检查“是否请求中断/停止”，在安全点有序退出。
- 避免强杀：硬终止可能造成资源泄漏、死锁或状态不一致。

### 常见手段

- `QThread::requestInterruption()` + `isInterruptionRequested()`：
```cpp
void Worker::doWork() {
    while (...) {
        if (QThread::currentThread()->isInterruptionRequested()) break;
        // ... 执行一小步 ...
    }
    emit finished();
}

// 请求停止
thread->requestInterruption();
thread->quit();
thread->wait();
```

- 原子标志：
```cpp
std::atomic_bool stop{false};
// 循环中检查 stop.load()；外部设置 stop.store(true)
```

- 可中断等待：
  - 使用带超时的等待函数（如 `waitFor...`），周期性返回检查中断。
  - I/O 或阻塞读：关闭套接字/管道唤醒阻塞，再走正常清理路径。

### UI 收尾与资源释放

- 在 `finished` 槽中回收资源、更新 UI 状态。
- 使用 `deleteLater()` 销毁归属于子线程的对象；线程用 `quit()+wait()` 收尾。

### 线程池/QtConcurrent 任务

- 使用 `QFuture`：
  - Qt6：`QFuture::cancel()`/`isCanceled()`；Qt5 需自管标志。
  - `QFutureWatcher::finished()` 在 UI 线程通知，做善后。

### 反例与风险

- 直接 `terminate()`：不可取，可能中断在关键区，导致数据破坏。
- 在 UI 线程等待未可中断的长任务：界面卡死；改用后台执行 + 协作式取消。

### 关联

- 《[QThread 正确使用姿势与常见误区](qthread_usage.md)》
- 《[跨线程更新 UI 的安全方式？](cross_thread_ui_update.md)》

