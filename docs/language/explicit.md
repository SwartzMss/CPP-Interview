---
title: explicit 关键字的作用
tags:
  - language
---

# `explicit` 关键字的作用

`explicit` 用于修饰构造函数或类型转换运算符，**禁止编译器进行隐式类型转换**，让“自动”变为“手动”。

## 何时需要 `explicit`

- 会被当作“隐式转换入口”的构造函数
  - 单参数构造函数：`struct T { explicit T(int); };`
  - 多参数但除一个以外都有默认值：`explicit T(int x, int y = 0);`
  - `std::initializer_list` 构造函数（本质是单参数）
- 类型转换运算符
  - 例如 `explicit operator bool() const;`，避免对象被当作整数参与算术或比较
- 强类型/语义约束
  - 如 `Id{int}`、单位/货币、句柄等场景，禁止“看起来能转其实不对”的隐式转换
- 存在信息丢失或代价高昂的转换
  - 如 `Big(Tiny)` 可能截断，或构造成本较高

## 行为规律（记忆卡）

- 禁止隐式转换与复制初始化：`T t = 1;`、`f(1)` 都会失败
- 允许直接初始化：`T t(1);`、`T t{1};`、`f(T{1})` 可用
- 列表初始化注意：`T t = {1};` 属于“复制列表初始化”，同样会受 `explicit` 限制
- C++20 起支持“条件 explicit”：`explicit(constant-expression)`

```cpp
template<class U>
struct Box {
    // 非整型参数时要求显式构造
    explicit(!std::is_integral_v<U>) Box(U);
};
```

## 典型示例

```cpp
struct Foo {
    explicit Foo(int x, int y = 0); // 看似双参，但可被单实参调用
};

void bar(Foo);

int main() {
    // bar(10);        // ❌ 禁止隐式：需要显式构造
    bar(Foo(10));      // ✅ 直接初始化
    Foo a{10};         // ✅ 直接列表初始化
    // Foo b = {10};   // ❌ 复制列表初始化同样被禁止
}
```

类型转换运算符配合 `explicit`：

```cpp
struct Socket {
    int fd;
    explicit operator bool() const noexcept { return fd >= 0; }
};

void f(const Socket& s) {
    if (s) { /* ✅ 合法的条件判断 */ }
    // int x = s + 1;  // ❌ 不允许被隐式转换为整数参与算术
}
```

## 常见陷阱与注意

- `std::initializer_list` 重载与花括号：当存在 `initializer_list` 构造时，`T{...}` 更倾向匹配它；若它被标记为 `explicit`，`T t = { ... }` 将被禁用，需要使用 `T t{ ... }` 或显式构造。
- 不要将复制/移动构造函数标记为 `explicit`：会破坏常见用法（例如容器的值语义）。
- 仅在明确希望禁止“隐式转换”的 API 上使用：有些轻量适配器类就是为了“隐式”而生，应统一权衡团队风格。

## 何时不必（或不宜）用

- 你确实希望存在隐式从“轻量类型”到“重类型”的便捷过渡，并且转换安全清晰。
- 历史惯例或生态依赖隐式转换的类型（需与团队一致性保持一致）。

## 面试总结

- `explicit` 不仅仅用于“单参数”，凡是可能被单实参调用的构造、以及类型转换运算符都应考虑。
- 规则：禁止隐式/复制初始化，允许直接初始化；列表初始化用 `=` 也会被禁止。
- C++20 支持条件 `explicit`，便于模板化 API 的语义精细化。
