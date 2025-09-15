---
title: Qt 信号槽机制与连接类型？
tags:
  - qt
---

## 问题

Qt 的信号-槽机制是如何工作的？连接类型有哪些，各自适用场景是什么？

## 回答

### 1) 基本机制（Meta-Object + 事件循环）

- 元对象系统：含有 `Q_OBJECT` 的类会经由 `moc` 生成元信息（信号列表、属性、调用表等）。
- 连接建立：`QObject::connect` 建立信号与槽的关联，返回 `QMetaObject::Connection`，对象析构会自动断开相关连接。
- 发射信号：发射本质是一个普通成员函数调用，内部通过 `QMetaObject::activate` 遍历连接，并按连接类型决定调用方式。
- 跨线程与事件循环：队列（Queued）连接会把调用封装成事件投递到接收者线程的事件队列，由该线程的事件循环异步处理。
- 参数类型要求：对于队列连接，参数需要可拷贝并可通过 `QMetaType` 识别（自定义类型需 `Q_DECLARE_METATYPE` 并在运行时 `qRegisterMetaType<T>()`）。

常用的“新语法”更安全（编译期检查）：

```cpp
connect(sender, &Sender::sigName, receiver, &Receiver::slotName);     // 成员函数
connect(sender, &Sender::sigName, receiver, [=](Args... a){ ... });   // lambda 槽
```

### 2) 连接类型（`Qt::ConnectionType`）

- Qt::AutoConnection（默认）
  - 如果发射信号的线程与接收者所属线程相同，则等同 Direct；否则等同 Queued。
- Qt::DirectConnection（直接连接）
  - 槽函数在发射信号的线程里同步调用。注意：Direct 不等于“同线程”，即使接收者属于其他线程，也会在发射线程里执行槽（可能越线程访问接收者）。适合同线程、对实时性有要求且槽执行很快且线程安全的场景。
- Qt::QueuedConnection（队列连接）
  - 把调用投递到接收者线程的事件队列，槽在接收者线程异步执行。跨线程/切到 UI 线程更新时常用。
- Qt::BlockingQueuedConnection（阻塞队列）
  - 类似队列连接，但会阻塞发射线程直到槽执行完毕。仅用于不同线程之间；同线程使用会造成死锁风险。
- Qt::UniqueConnection（去重标志）
  - 作为标志与上述类型按位或使用（如 `Qt::AutoConnection | Qt::UniqueConnection`），避免重复建立同一对信号-槽连接。

要点：

- Auto 是首选；跨线程自动退化为 Queued；同线程为 Direct。
- 队列连接要求接收者线程有事件循环在跑，否则不会处理该调用（常见于子线程未进入 `exec()`）。
- 队列连接会拷贝参数；大对象可考虑共享指针或轻量结构以减小开销。
- 阻塞队列仅在明确需要等待结果的跨线程场景使用，并确保不会形成互相等待。

提示：Direct 不会把调用“搬运”到接收者线程；它始终在发射线程执行。如果接收者属于其他线程，这样的 Direct 就是跨线程 Direct，容易导致线程亲和性违规（例如 UI 对象被非 GUI 线程访问）。

### 3) 选型建议

- 同线程、快速槽：`Auto`（= Direct）。
- 跨线程、UI 更新：`Queued`（把更新切回主线程）。
- 跨线程且需等待返回：`BlockingQueued`，谨慎使用并注意死锁。
- 防重复连接：在 `connect` 时加 `Qt::UniqueConnection`。
- 自定义参数用于队列连接：`Q_DECLARE_METATYPE(T)` + `qRegisterMetaType<T>("T")`。

### 4) 示例

跨线程调用，切换到工作线程执行：

```cpp
class Worker : public QObject {
    Q_OBJECT
public slots:
    void doWork(int v) { /* ...耗时任务... */ }
};

QThread th;
Worker w;
w.moveToThread(&th);
th.start();

// 默认 Auto：不同线程 -> 等同 Queued，在工作线程中执行 doWork
QObject::connect(&sender, &Sender::sigValue, &w, &Worker::doWork);

sender.emitSigValue(42); // 异步投递到工作线程
```

阻塞等待工作线程完成：

```cpp
QObject::connect(
    &sender, &Sender::sigValue,
    &w,      &Worker::doWork,
    Qt::BlockingQueuedConnection
);
// 发射线程在此连接下会等待 doWork 返回
```

防止重复连接：

```cpp
QObject::connect(
    &a, &A::sig,
    &b, &B::slot,
    Qt::AutoConnection | Qt::UniqueConnection
);
```

自定义类型用于队列连接：

```cpp
struct Foo { int x; };
Q_DECLARE_METATYPE(Foo)

int main(int argc, char** argv) {
    qRegisterMetaType<Foo>("Foo");
    // 之后即可在 Queued 连接中传递 Foo
}
```

### 5) 常见坑位

- 子线程未运行事件循环，导致队列连接的槽不执行。
- 槽执行耗时却使用 Direct，阻塞了发射线程（甚至 UI 线程）。
- 自定义参数未注册 `QMetaType`，导致队列连接运行时告警或崩溃。
- 误用 `BlockingQueued` 于同线程或形成锁顺序错误，造成死锁。

—— 熟悉线程亲和性（`QObject::thread()`）、事件循环与参数类型要求，是正确选用连接类型的关键。

### 6) 重负载槽与 Direct 的替代方案

问题：重负载槽使用 Direct 会让槽在发射处同步执行，若发射方是 UI 线程，界面会卡顿；跨线程 Direct 还可能越线程操作接收者，带来安全风险。

修正：

- 把重活放到工作线程（`QThread + Worker`），用信号把结果发回 UI。
- 对“可能很重”的槽，显式使用 `Qt::QueuedConnection`，避免在发射线程阻塞。
- 仅把轻量 UI 更新留在 GUI 线程；必要时用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 或 `QTimer::singleShot(0, ...)` 切回。
- 需要等待结果再考虑 `Qt::BlockingQueuedConnection`，但要严格避免同线程和死锁。

示例：重活在 Worker，结果回 UI。

```cpp
class Worker : public QObject {
    Q_OBJECT
signals:
    void done(QString r);
public slots:
    void doWork(Input in) { /* heavy */ emit done("ok"); }
};

QThread th; auto w = new Worker; w->moveToThread(&th); th.start();

// 发给 Worker：跨线程 Auto => Queued（异步，不阻塞 UI）
QObject::connect(&sender, &Sender::start, w, &Worker::doWork);

// 回到 UI：同线程 => Direct（快且安全），或显式 Queued 亦可
QObject::connect(w, &Worker::done, ui, [ui](const QString& r){ ui->setText(r); });
```

同线程但要避免长阻塞：

```cpp
// 把执行切片，排队到稍后执行
QObject::connect(obj, &Obj::sig, obj, &Obj::slot, Qt::QueuedConnection);
// 或：
QTimer::singleShot(0, obj, [obj]{ obj->slot(); });
```
