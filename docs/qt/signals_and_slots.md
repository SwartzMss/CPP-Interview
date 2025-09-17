---
title: Qt 信号槽机制
tags:
  - qt
---

## 问题

Qt 的信号-槽机制是如何工作的？常用语法有哪些？

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

### 2) 常用语法与技巧

- 新语法（函数/成员指针）：编译期类型检查、安全重命名。
- 连接到 lambda：便于就地处理、捕获上下文。
- 指定上下文对象：`connect(sender, signal, context, functor)` 让连接随上下文析构自动断开。
- 断开连接：保存 `QMetaObject::Connection` 并在需要时 `disconnect(conn)`。
- 重载信号/槽：使用 `QOverload<Args...>::of(&Class::signal)` 辅助选择。
- 连接类型选择：详见《[Qt 连接类型详解](connection_types.md)》。

提示：Direct 不会把调用“搬运”到接收者线程；它始终在发射线程执行。如果接收者属于其他线程，这样的 Direct 就是跨线程 Direct，容易导致线程亲和性违规（例如 UI 对象被非 GUI 线程访问）。

### 3) 选型建议

- 同线程、快速槽：`Auto`（= Direct）。
- 跨线程、UI 更新：`Queued`（把更新切回主线程）。
- 跨线程且需等待返回：`BlockingQueued`，谨慎使用并注意死锁。
- 防重复连接：在 `connect` 时加 `Qt::UniqueConnection`。
- 自定义参数用于队列连接：`Q_DECLARE_METATYPE(T)` + `qRegisterMetaType<T>("T")`。

### 4) 示例

跨线程调用，切换到工作线程执行（连接类型详见连接类型文档）：

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

阻塞等待工作线程完成（谨慎使用）：

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

### 3) 关联主题

- 连接类型详解：参见《[Qt 连接类型详解](connection_types.md)》
- 常见坑与最佳实践：参见《[Qt 信号槽常见坑与最佳实践](pitfalls_best_practices.md)》
