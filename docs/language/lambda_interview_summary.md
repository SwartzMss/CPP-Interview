---
title: Lambda 常见考点总结
tags:
  - language
---

## 概览

- 捕获方式与区别：按值、按引用、隐式捕获、初始化捕获。
- 与函数对象（仿函数）的关系：闭包对象、本质是匿名类 + `operator()`。
- 与 `std::function` 的关系：类型擦除的代价与使用场景。
- 类型与声明：Lambda 的独特类型、`auto`、`decltype`、泛型 lambda。
- `mutable`：允许修改按值捕获的副本（仅影响闭包内部副本）。

## 捕获方式与区别

- 按值捕获 `[x]`：把外部变量按值拷贝进闭包对象的成员。
  - 修改需要 `mutable`；修改的只是副本，外部变量不变。
- 按引用捕获 `[&x]`：闭包保存引用/地址，读写影响外部变量；需确保生命周期安全。
- 隐式捕获 `[=]` / `[&]`：缺省按值 / 按引用捕获用到的外部变量。
  - `this` 在 `[=]` 下会隐式按引用（复制 this 指针）捕获，可能悬垂；可改用 `[*this]`（C++17）按值拷贝对象。
- 初始化捕获（C++14 起）：`[p = std::move(ptr), i = x + 1]`
  - 支持移动捕获、重命名、表达式初始化；常用于捕获 `unique_ptr`/socket/文件句柄等。

示例：

```cpp
int x = 0; 
auto f1 = [x]()      { /* 读 x 的副本 */ };
auto f2 = [&x]()     { x++; };                // 引用修改外部 x
auto f3 = [=]() mutable { /* 可修改副本 */ };
auto f4 = [p = std::make_unique<int>(42)]() { /* 拥有 p */ };
```

## 与函数对象的关系（底层实现）

- 编译器把 lambda 编译为“匿名类”的一个对象（闭包对象），其捕获成为该类的数据成员，函数体成为 `operator()`。
- 无捕获 lambda 可隐式转换为函数指针：`void(*)()`。
- 有捕获 lambda 的类型是独特的、不可书写；只能通过 `auto`/模板参数承接。

示意（非标准语法，仅说明本质）：

```cpp
// auto lam = [a](int x){ return x + a; };
struct __Lambda {
    int a;                       // 捕获
    int operator()(int x) const { return x + a; }
};
__Lambda lam{.a = a};
```

## `std::function` 与 Lambda 的结合

- `std::function<R(Args...)>` 通过类型擦除存放任意可调用对象（函数指针、lambda、仿函数、`bind` 结果等）。
- 代价：可能发生堆分配（小对象优化依赖实现），调用有间接开销；目标需可复制（C++23 起可用 `std::move_only_function` 支持仅移动）。

建议：
- 函数模板/成员模板参数使用“泛型可调用”：`template<class F> void run(F f);` 或 `auto` 形参（C++20）。
- 仅当需要类型擦除（跨模块存储、容器、虚接口替代）时再用 `std::function`。

示例：

```cpp
#include <functional>

void set_callback(std::function<int(int)> cb) { /* 存起来稍后调用 */ }

int main() {
    int a = 10;
    set_callback([a](int x){ return x + a; });
}
```

## 类型推断与声明方式

- 变量承接：`auto f = [](int x){ return x + 1; };`
- 不能直接写出 lambda 的类型；若需声明相同类型的变量，可用 `decltype`：
  - `auto f = []{}; decltype(f) g = f;`
- 泛型 lambda（C++14）：形参用 `auto`，相当于内置模板：
  - `auto g = [](auto&& x){ return x + x; };`
- 返回类型推断：单 return 推断；复杂分支用尾置返回类型：
  - `auto h = [](int x){ if (x>0) return x; else return 0; };`
  - `auto h2 = [](auto x, auto y) -> std::common_type_t<decltype(x), decltype(y)> { return x + y; };`

## `mutable`：修改按值捕获的副本

- 缺省 `operator()` 为 `const`，不能修改按值捕获的成员；`mutable` 使其变为非常量，可修改副本。
- 只影响闭包对象内部副本，不会回写到外部变量。

```cpp
int a = 0;
auto f = [a]() mutable { a = 42; return a; };
int r = f();        // r = 42
// a 仍为 0（外部变量未变）
```

## 常见陷阱与建议

- 避免 `[&]` 跨线程/异步持有引用；优先值捕获或初始化捕获移动资源。
- 注意 `[=]` 会隐式捕获 `this`（按引用）。对象可能析构导致悬垂；需要对象副本时用 `[*this]`（C++17）。
- 在回调长期存活的场景，避免捕获栈对象的引用；可捕获 `shared_ptr` 或使用所有权转移。
- 作为回调存放时若无需类型擦除，优先模板/`auto`；确需统一接口再用 `std::function`。

## 速记

- 值捕获=拷贝进闭包；引用捕获=指向外部；初始化捕获=可表达式/可移动。
- 无捕获可转函数指针；有捕获是“匿名类”。
- `std::function` 为类型擦除容器，有成本；C++23 有 `move_only_function`。
- `mutable` 仅改内部副本，不回写外部。
