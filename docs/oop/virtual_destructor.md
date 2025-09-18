---
title: 为什么析构函数必须是虚函数？（两个例子看懂）
tags:
  - oop
---

## 问题

为什么在多态场景下析构函数必须声明为虚函数？如果是通过派生类指针删除对象，还需要虚析构吗？

## 回答（两个例子）

### 例子 1：多态删除（经由基类指针）— 必须虚析构

```cpp
struct Base {
    virtual void foo();
    ~Base() {}                 // 非虚析构（错误做法）
};
struct Derived : Base {
    ~Derived() { /* 释放派生资源 */ }
};

Base* p = new Derived;
delete p;      // 未定义行为：只调用 Base::~Base()，Derived 资源未释放
```

正确做法：

```cpp
struct Base {
    virtual ~Base() = default; // 通过基类指针删除会动态绑定到最末层析构
    virtual void foo();
};
```

结论：只要对象可能“经由基类指针/引用”被销毁（多态使用），基类析构必须是 `virtual`。

### 例子 2：通过派生类指针删除—不依赖虚析构

```cpp
struct Base { ~Base() {} };      // 非虚
struct Derived : Base { ~Derived() {} };

Derived* p = new Derived;
delete p;   // 安全：调用顺序为 Derived::~Derived() → Base::~Base()
```

结论：当且仅当用“确切类型指针/引用”销毁同类型对象本身时，是否虚析构不影响正确性；问题出在“经由基类（发生多态擦除）销毁”。

