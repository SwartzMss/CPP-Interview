---
title: make_shared 和 shared_ptr 的区别？
tags:
  - stl
---

## 问题

`std::make_shared` 与 `std::shared_ptr<T>(new T)` 有什么区别？

## 回答

### 1. 内存布局

`std::make_shared<T>(args...)` **一次分配**：将控制块和对象本体放在同一块堆内存中。

```
[ 控制块 + T 对象 ]
├─ ref counts, deleter, allocator
└─ T 对象本体
```

`std::shared_ptr<T>(new T(args...))` **两次分配**：对象和控制块分别分配。

```
[ T 对象 ]            [ 控制块 ]
└─ 堆1                 └─ 堆2
```

### 2. 行为差异

- **性能与缓存**：`make_shared` 少一次堆分配，控制块与对象相邻，速度和缓存局部性更好；`new + shared_ptr` 需两次分配。
- **异常安全**：`make_shared` 天然强异常安全；使用 `new` 时若分两行写，可能在 `new` 成功后构造 `shared_ptr` 前抛异常导致泄漏。
- **大对象延迟释放**：`make_shared` 中对象和控制块同处一块内存，若存在长寿命 `weak_ptr`，对象析构后这块大内存仍需等待 `weak_ptr` 全部失效才释放；分离分配可在强计数归零时立刻释放对象内存。
- **自定义删除器/分配器**：`make_shared` 不能自定义删除器，可用 `allocate_shared` 指定分配器；`new + shared_ptr` 可传入自定义删除器，适合管理非 `new` 资源。

### 3. 选择指南

| 场景 | 推荐方式 |
|------|----------|
| 普通对象，追求性能与简洁 | `std::make_shared` |
| 需要自定义删除器，或对象很大且存在长期 `weak_ptr` | `std::shared_ptr<T>(new T(...))` |
| 需要自定义分配器 | `std::allocate_shared` |

### 4. 小实验代码

```cpp
#include <iostream>
#include <memory>

struct Foo {
    Foo()  { std::puts("Foo constructed"); }
    ~Foo() { std::puts("Foo destructed"); }
};

int main() {
    std::weak_ptr<Foo> w;

    {
        auto sp = std::make_shared<Foo>();
        w = sp;
        std::cout << "use_count: " << sp.use_count() << '\n';
    } // sp 析构

    std::cout << "expired? " << w.expired() << '\n';
    return 0;
}
```

这个程序可用于观察控制块和对象的释放顺序。

### 5. 面试总结

`make_shared` 将控制块与对象合并分配，性能好且异常安全；`new + shared_ptr` 支持自定义删除器，并能在大对象场景下更早释放对象内存。常规场景首选 `make_shared`，有特殊资源管理或内存占用时机需求时再选 `new + shared_ptr`（或 `allocate_shared`）。
