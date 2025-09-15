---
title: QThread 正确使用姿势与常见误区
tags:
  - qt
---

## 问题

如何正确使用 QThread？推荐架构是什么？有哪些常见误区与规避方式？

## 回答

### 1) 两种用法与推荐

#### 推荐：Worker 对象 + `moveToThread()`

- 将纯业务的 `QObject`（Worker）移动到子线程；通过信号把任务投递给 Worker，通过信号把结果发回 UI。
- 优点：复用事件循环、便于跨线程通信、资源生命周期清晰。

#### 少数场景：继承 QThread（自定义 `run()`/专用线程）

- 在 `run()` 中执行阻塞循环或设置自定义事件循环等特殊需求。
- 不要在 `QThread` 子类中放业务槽/对象；业务对象应为独立 `QObject` 并放入目标线程。

结论：绝大多数场景使用 Worker + `moveToThread()` 即可。

### 2) 标准模板（Worker + moveToThread）

```cpp
class Worker : public QObject {
    Q_OBJECT
public:
    explicit Worker(QObject* parent = nullptr) : QObject(parent) {}

signals:
    void progress(int p);
    void finished();

public slots:
    void doWork(QString path) {
        for (int i = 0; i < 100; ++i) {
            if (QThread::currentThread()->isInterruptionRequested()) break;
            // ...heavy work...
            emit progress(i);
        }
        emit finished();
    }
};

// 使用
auto* th = new QThread;                // 无父对象，避免跨线程父子关系
auto* w  = new Worker;                 // 同上
w->moveToThread(th);

QObject::connect(th, &QThread::started,  w,  [w]{ w->doWork("/tmp"); });
QObject::connect(w,  &Worker::progress, ui,  [ui](int p){ ui->bar->setValue(p); });
QObject::connect(w,  &Worker::finished, th, &QThread::quit);
QObject::connect(th, &QThread::finished, w,  &QObject::deleteLater);
QObject::connect(th, &QThread::finished, th, &QObject::deleteLater);

th->start();

// 请求结束（例如窗口关闭）
th->requestInterruption();
th->quit();
th->wait();
```

要点：
- 子线程对象用 `deleteLater()` 在其所属线程安全销毁；线程用 `quit()+wait()` 收尾。
- 使用 `requestInterruption()`/`isInterruptionRequested()` 协作式取消；或自定义原子标志。
- UI 更新在 UI 线程完成（跨线程信号槽默认 Queued）。

### 3) 事件循环与定时器

- `QThread` 默认 `start()` 后进入内部事件循环（除非自定义 `run()` 覆盖了它）。
- `QTimer` 等依赖事件循环的类必须在有事件循环的线程中使用；否则定时器/事件不会触发。
- 若自定义 `run()` 执行长阻塞循环，则队列事件无法处理；尽量让 Worker 使用默认事件循环，避免在 `run()` 中阻塞。

### 4) 线程亲和性与对象生命周期

- `QObject::thread()` 表示对象归属线程；只能在该线程直接访问其非线程安全状态。
- 不要跨线程建立父子关系（父在 UI 线程、子在 Worker 线程等）；常见做法是无父创建对象，移动到目标线程后再设置父子。
- 销毁对象用 `deleteLater()` 让其在所属线程的事件循环中完成。

### 5) 常见误区

- 将 UI 对象移到子线程或在子线程直接触碰 UI：未定义行为。
- 跨线程使用 `Qt::DirectConnection`：槽在发射线程执行，越线程访问接收者；跨线程使用 `Queued`。
- 子线程未进入事件循环却使用队列连接：槽不执行；需要 `start()` 后进入 `exec()`。
- 在 `run()` 中执行长阻塞导致事件无法处理：改为 Worker + 默认事件循环。
- 粗暴终止线程（如平台 API 强杀）：易导致资源泄漏或不一致；优先协作式取消。
- 线程不收尾：忘记 `quit()`/`wait()`；会泄漏或崩溃。
- 在错误线程 delete 对象：使用 `deleteLater()`。

### 6) 停止/取消与中断

- `QThread::requestInterruption()` + 在循环中检查 `isInterruptionRequested()`。
- 自定义 `std::atomic_bool stop{false};` 并在热点循环检查。
- I/O 或阻塞等待：使用可中断 API（带超时）或在外部关闭句柄以唤醒阻塞。

### 7) QThreadPool / QtConcurrent

- 零散一次性任务：优先使用 `QThreadPool` 或 `QtConcurrent::run()`，避免大量 `QThread` 管理开销。
- 批量 CPU 密集：控制并发度（线程池大小 = 核心数或合适的倍数）。
- 结合 `QFutureWatcher` 在 UI 线程处理完成/进度信号。

### 8) 示例：继承 QThread 的适用场景（少见）

```cpp
class LooperThread : public QThread {
    void run() override {
        QEventLoop loop;           // 自定义事件循环
        QTimer timer;              // 绑定到当前线程
        QObject::connect(&timer, &QTimer::timeout, []{ /* ... */ });
        timer.start(100);
        loop.exec();               // 退出 loop 即线程结束
    }
};
```

注意：不要把业务槽/对象塞进 `QThread` 子类里并期望自动跨线程；业务仍应放在 Worker 中。

### 9) 关联主题

- 《[跨线程更新 UI 的安全方式？](cross_thread_ui_update.md)》
- 《[Qt 连接类型详解](connection_types.md)》
- 《[信号槽常见坑与最佳实践](pitfalls_best_practices.md)》
