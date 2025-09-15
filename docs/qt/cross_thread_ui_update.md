---
title: 跨线程更新 UI 的安全方式？
tags:
  - qt
---

## 问题

在 Qt 中如何安全地从工作线程更新 UI（QWidget/QML）？有哪些推荐手段与常见坑？

## 回答

### 1) 核心原则

- 仅在 GUI 线程访问/修改 UI 对象（`QWidget`、`QWindow`、`QQuickItem` 等）。
- 跨线程时，将更新请求“投递”到 GUI 线程处理（消息/队列调用），而不是直接调用 UI 接口。

### 2) 推荐方式

- 信号槽（`Qt::AutoConnection`/`Qt::QueuedConnection`）
  - 跨线程自动变为队列连接，槽在接收者（UI）线程异步执行。
- `QMetaObject::invokeMethod`
  - 指定 `Qt::QueuedConnection`，把调用排队到接收者所属线程。
- `QTimer::singleShot(0, ...)`
  - 将一个 0ms 定时任务加入接收者线程的事件队列，常用于切回主线程。
- `QtConcurrent` + `QFutureWatcher`
  - 计算在线程池执行，通过 `finished()`、`progressValueChanged()` 等信号在 UI 线程处理结果。
- 自定义事件
  - `QCoreApplication::postEvent(target, ...)` + 覆盖 `event()`/`customEvent()`。

### 3) 安全架构（QThread + Worker 对象）

- 不要将 UI 对象移到子线程；仅移动“工作对象”到子线程：`worker->moveToThread(&thread)`。
- 连接 Worker 的信号到 UI 槽使用默认 `Auto` 或显式 `Queued`，保证在 UI 线程运行。
- 确保接收者线程有事件循环（GUI 线程天然有；子线程需要调用 `exec()` 进入循环）。

### 4) 代码示例

工作线程计算，UI 线程更新：

```cpp
class Worker : public QObject {
    Q_OBJECT
signals:
    void progress(QString msg);
public slots:
    void doWork() {
        // ...耗时任务...
        emit progress("50%");
    }
};

QThread th;
auto* worker = new Worker;
worker->moveToThread(&th);
QObject::connect(&th, &QThread::started, worker, &Worker::doWork);

// 跨线程：Auto => Queued，槽在 GUI 线程执行
QObject::connect(worker, &Worker::progress, label, [label](const QString& s){
    label->setText(s);
});

th.start();
```

直接把调用排队到 GUI 线程：

```cpp
QMetaObject::invokeMethod(
    label,                                // 接收者决定目标线程
    [label]{ label->setText("done"); },   // Qt 5.10+ functor 语法
    Qt::QueuedConnection
);
```

0ms 定时切回主线程：

```cpp
QTimer::singleShot(0, QApplication::instance(), [w]{ w->updateUI(); });
```

旧版字符串槽写法：

```cpp
QMetaObject::invokeMethod(window, "updateStatus",
                          Qt::QueuedConnection,
                          Q_ARG(QString, text));
```

### 5) 常见误用

- 在子线程直接调用 `label->setText()`：未定义行为/崩溃。
- 将 `QWidget`/`QQuickItem` 调用 `moveToThread()`：禁止，UI 必须留在 GUI 线程。
- 子线程没有事件循环却使用队列连接：槽不会执行（需要 `QThread::exec()`）。
- `Qt::BlockingQueuedConnection` 在同线程或锁顺序不当导致死锁：谨慎使用。
- 队列连接传递自定义类型未注册 `QMetaType`：需 `Q_DECLARE_METATYPE` + `qRegisterMetaType<T>()`。
- lambda 捕获悬垂指针：将接收者对象作为 `connect` 的上下文，或使用 `QPointer`。

### 6) 选型小贴士

- 默认 `Qt::AutoConnection` 足够，跨线程自动排队，性能/正确性平衡。
- 大对象结果避免频繁拷贝：传 `QSharedPointer`、轻量结构，或仅传索引/句柄。
- QML/Qt Quick 同理：只在 GUI 线程触碰 QML 对象；用信号/`invokeMethod` 切回。

### 7) 与“信号槽连接类型”的关系与重负载槽处理

- 交叉阅读：更系统的连接类型说明与坑位，可参见《[Qt 信号槽机制与连接类型？](signals_and_slots.md)》。
- 重负载槽不要用 `DirectConnection`（或同线程下的 `Auto` 会退化为 Direct），否则会在发射点同步执行并阻塞（可能是 UI 线程）。
- 建议：
  - 把重活放到 `Worker`（子线程）里执行；结果再通过信号回到 UI 线程。
  - 或显式使用 `Qt::QueuedConnection`，让调用异步化，避免阻塞发射线程。
  - 需要等待结果时再考虑 `Qt::BlockingQueuedConnection`，谨慎避免死锁。

示例：`QtConcurrent` + `QFutureWatcher` 下放计算，完成后在 UI 线程更新。

```cpp
#include <QtConcurrent>
#include <QFutureWatcher>

auto task = [] {
    // heavy work...
    return QString("result");
};

QFuture<QString> future = QtConcurrent::run(task);
auto* watcher = new QFutureWatcher<QString>(parent);

QObject::connect(watcher, &QFutureWatcher<QString>::finished, ui, [=]{
    // QFutureWatcher 的信号在其所在线程（通常是 GUI 线程）发出
    ui->label->setText(watcher->future().result());
    watcher->deleteLater();
});

watcher->setFuture(future);
```

示例：同线程但希望避免一次长阻塞，可用队列连接或 0ms 定时切片处理。

```cpp
QObject::connect(obj, &Obj::sig, obj, &Obj::slot, Qt::QueuedConnection);
// 或
QTimer::singleShot(0, obj, [obj]{ obj->slot(); });
```
