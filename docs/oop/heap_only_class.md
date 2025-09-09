---
title: 如何设计一个只能在堆上创建对象的类？
tags:
  - oop
---
## 问题

如何设计一个只能在堆上创建对象的类？

## 回答

要让类只能在堆上创建，关键是阻止编译器在栈上完成构造或析构。常见做法有三种：

### 方案 A：私有/受保护析构 + 工厂 + 自定义 `deleter`

将析构函数设为 `private` 或 `protected`，栈对象离开作用域时编译器无法访问析构器，于是禁止在栈上定义。工厂函数内部使用 `new` 创建并返回带自定义 `deleter` 的智能指针，使 `deleter` 拥有访问析构器的权限。

```cpp
#include <memory>

class HeapOnly {
public:
    static std::unique_ptr<HeapOnly, struct Deleter> create(int x) {
        return std::unique_ptr<HeapOnly, Deleter>(new HeapOnly(x));
    }
    int value() const { return x_; }

private:
    explicit HeapOnly(int x) : x_(x) {}
    ~HeapOnly() = default;              // 析构不可见 -> 禁栈

    struct Deleter {
        void operator()(HeapOnly* p) const { delete p; }
    };
    friend struct Deleter;

    int x_;
};
```

优点：彻底禁止栈对象，释放统一交给智能指针。  
注意：`std::make_shared` 不能用于这种类型，需要自定义 `deleter`。

### 方案 B：私有/受保护构造 + 静态工厂函数

构造函数设为 `private` 或 `protected`，外部既不能在栈上，也不能直接 `new`。工厂函数内部 `new` 出对象并返回 `std::unique_ptr`；析构函数可以设为 `public`。

```cpp
#include <memory>

class HeapOnly2 final {
public:
    static std::unique_ptr<HeapOnly2> create(int x) {
        return std::unique_ptr<HeapOnly2>(new HeapOnly2(x));
    }

    int value() const { return x_; }
    ~HeapOnly2() = default;

private:
    explicit HeapOnly2(int x) : x_(x) {}
    int x_;
};
```

优点：实现简洁，默认删除器可用。  
注意：最好将类标记为 `final`，避免派生类暴露构造函数绕过限制。

### 方案 C：私有析构 + `destroy()`（传统做法）

提供静态 `create()` 返回裸指针，并由成员函数 `destroy()` 释放。

```cpp
class HeapOnly3 {
public:
    static HeapOnly3* create(int x) { return new HeapOnly3(x); }
    void destroy() { delete this; }

private:
    explicit HeapOnly3(int x) : x_(x) {}
    ~HeapOnly3() = default;            // 禁栈
    int x_;
};
```

优点：实现简单。  
缺点：不符合 RAII，调用方必须手动 `destroy()`，容易泄漏或重复释放，现代 C++ 不推荐。

### 小结

- 私有/受保护析构可以禁止栈对象，但需要自定义 `deleter` 或显式 `destroy()`。
- 私有/受保护构造配合工厂函数是更常见的现代方式，建议返回智能指针并将类标记为 `final`。
- 无论哪种方式，都应使用 RAII 管理生命周期，避免手动 `delete`。
