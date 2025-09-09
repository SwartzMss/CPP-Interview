---
title: vector中 push_back 和 emplace_back 的区别？
tags:
  - stl
---

## 问题

vector中 `push_back` 和 `emplace_back` 有什么区别？

## 回答

`push_back` 接受一个已构造好的对象并把它拷贝或移动到容器尾部；  
`emplace_back` 则直接在容器尾部原地构造对象，避免创建临时对象和额外的拷贝/移动开销。

```cpp
struct Foo {
    Foo(int a, int b) { /* ... */ }
};

std::vector<Foo> v;

// push_back：对象先构造，再拷贝/移动到容器
Foo f1(1, 2);         // 构造一次
v.push_back(f1);      // 拷贝一次
v.push_back(Foo(3,4));// 临时对象构造一次，再移动或拷贝一次

// emplace_back：直接在容器尾部构造
v.emplace_back(5, 6); // 原地构造一次，无额外拷贝/移动
```

### 补充

- 如果已有对象，可以使用 `push_back(std::move(obj))` 或 `emplace_back(std::move(obj))`，两者开销相近。
- `emplace_back` 通过完美转发参数，如果构造函数存在多义性，需要注意调用是否正确。
