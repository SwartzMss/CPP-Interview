---
title: std::move 与 std::forward 的区别？
tags:
  - language
---

## 核心区别

- `std::move`：无条件把表达式转换为右值（`T&&`），表示“我不再需要它，可被移走”。它只是转换，不会真正移动。
- `std::forward<T>`：按原实参的值类别“条件转右值”。仅当原实参是右值且模板参数 `T` 不是左值引用时，才转成右值；否则保持为左值。用于“完美转发”。

## 何时使用

- 使用 `std::move`（你在当前处要“拿走资源”）
  - 容器插入/赋值：`vec.push_back(std::move(s));`
  - 成员初始化：`S(std::string s): s_(std::move(s)) {}`
  - 接口按值接收再落地：`void set(std::string v){ value_ = std::move(v); }`

- 使用 `std::forward`（你只是“中转参数”，保留它原本是左值/右值）
  - 仅在“转发引用”模板中：`template<class T> void f(T&& x) { g(std::forward<T>(x)); }`
  - 工厂/构造/`emplace` 参数传递：`Ctor(Args&&... a): base_(std::forward<Args>(a)...) {}`

## 返回值建议

- 返回局部对象按值：优先直接 `return x;`，让编译器做 NRVO/复制省略；不要强写 `std::move(x)`（可能抑制 NRVO）。
- 返回成员/参数时需要明确移动：`return std::move(member_);`

## 常见注意点

- `const` + move 通常退化为拷贝：`const T` 被转成 `const T&&`，多数类型的移动构造/赋值无法绑定到 `const`，最终会复制。
- 只在“转发引用”（`T&&` 且 `T` 参与类型推导）位置使用 `std::forward`；若形参是 `T&`/`const T&` 或非模板，`forward` 没意义。
- 移动后对象仍需保持“有效但未指定状态”；不要依赖其旧值，仅能析构或重新赋值。
- 有异常保证要求时，可考虑 `std::move_if_noexcept`：移动可能抛异常而拷贝不抛时选择拷贝。
- 右值一旦“起了名字”就成了左值表达式，若要继续按右值传递，需要 `std::move`/`std::forward` 维持右值性。

## 最小示例（仅保留关键头文件）

```cpp
#include <string>
#include <utility> // std::move, std::forward

// 用于区分左值/右值调用路径（空实现即可）
inline void f(const std::string&) {}
inline void f(std::string&&) {}

template<class T>
void forwarder(T&& x) {
    // 完美转发：左值仍是左值，右值仍是右值
    f(std::forward<T>(x));
}

struct Holder {
    std::string m;
    std::string take() { return std::move(m); } // 明确把成员移出
};

// 返回值：优先让编译器做 NRVO（不要强写 std::move）
inline std::string make() {
    std::string t = "xyz";
    return t; // NRVO/复制省略
}

inline void demo() {
    // std::forward：仅在“转发引用”模板中使用
    std::string a = "A";
    forwarder(a);                   // 左值 -> f(const&)
    forwarder(std::string("B"));    // 右值 -> f(&&)
    auto tmp = std::string("C");
    forwarder(std::move(tmp));      // 命名对象右值化 -> f(&&)

    // std::move：当下就要移交资源
    std::string name = "alice";
    f(std::move(name));             // name 进入“有效但未指定”状态

    // const + move：通常退化为拷贝（多数类型对 const 不提供移动）
    const std::string cs = "const";
    f(std::move(cs));               // 调用 f(const&)（等价于拷贝语义）
}
```

## 一句话总结

你自己在当前语境要“拿走资源”时用 `std::move`；你只是中转参数、希望保留它原本是左值还是右值时用 `std::forward<T>`。

