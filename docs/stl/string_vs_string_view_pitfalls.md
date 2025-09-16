---
title: std::string 与 std::string_view 的常见坑
tags:
  - stl
---

## 标准与兼容性

- 引入版本：C++17（头文件 `<string_view>`，类型 `std::string_view`）。
- 实验性前身：C++14 Library Fundamentals TS（2015）的 `std::experimental::string_view`。
- 编译器支持：主流编译器自 2017 年起均已支持；旧代码中若见 `std::experimental::string_view`，升级到 C++17+ 可直接替换为标准版。
- 字面量后缀：`"abc"sv` 在命名空间 `std::literals::string_view_literals` 中，示例：
  
  ```cpp
  using namespace std::literals::string_view_literals;
  std::string_view sv = "hello"sv;
  ```

## 结论（安全用法）

- 只读入参：优先用 `std::string_view`（不持久化，不越过调用栈）。
- 需要持久化/跨线程/跨异步：存为 `std::string`（拥有所有权）。
- 与 C API 交互（需要以 `\0` 结尾）：要么复制到 `std::string`，要么确保末尾有 `\0` 且没有内嵌 `\0`。

## 高频坑点

1) 悬垂引用（最致命）

```cpp
std::string_view bad() {
    std::string s = "hello"; // 局部对象
    return std::string_view{s}; // 返回后 s 析构 → 悬垂
}

void store(std::string_view sv); // 若内部持久化保存 sv → 危险
store(std::string("tmp"));       // 临时 string 销毁后，sv 悬垂
```

不要把 `string_view` 存起来（成员变量/容器/异步回调），除非它引用的内存是静态存储期或你能严格保证其存续期更长（如引用“更大的 owning 字符串”生命周期相同）。

2) 非 `\0` 结尾与内嵌 `\0`

```cpp
void c_api(const char*); // 期望以 \0 结尾
std::string_view sv = "abc\0def"; // size() == 7，包含内嵌 \0
c_api(sv.data());        // 仅看到 "abc"，后半丢失
```

`string_view` 只是“指针+长度”，`data()` 不保证末尾 `\0`。传给 C API 前，需拷贝到 `std::string` 或手动补 `\0`（且确认无内嵌 `\0`）。

3) 底层存储变化导致视图失效

```cpp
std::string s = "hello";
std::string_view v = s;   // 指向 s 的缓冲区
s += " world";            // 可能触发 reallocate
// v 现在可能悬垂（指向旧缓冲）
```

修改 `std::string` 可能使所有指向它的 `string_view` 失效（特别是增长导致重分配时）。

4) 以为 `string_view`“更省内存所以 everywhere 用”

`string_view` 不拥有数据，不能延长生命周期；用错场景反而更危险。规则：
- 形参读-only → 用值传 `std::string_view`；
- 需要保存/跨边界 → 立即拷贝成 `std::string`。

5) 与字面量/缓冲区的生命周期

```cpp
constexpr std::string_view ok = "literal"; // OK，静态存储期
std::string_view v = std::to_string(42);   // 危险：临时 string 立刻销毁
```

对临时字符串、局部 `std::string`、`std::vector<char>` 的数据视图在对象销毁或重分配后都将失效。

6) 与 UTF-8/字符边界

`std::string`/`std::string_view` 都是“字节序列”，按字节切片可能破坏多字节字符边界；不要用 `substr/remove_prefix` 去“按字符”切多字节编码。

## 正确范式

- API 设计：读-only 文本参数用 `std::string_view`；需要持久化则在函数体内 `std::string owned(sv);` 拷贝一次。
- 与 C API 的桥接：

```cpp
void use_c_api(std::string_view sv) {
    std::string tmp(sv);     // 保证 \0 结尾且无内嵌 \0 影响
    c_api(tmp.c_str());
}
```

- 视图内切片不改原数据：`remove_prefix/remove_suffix/substr` 只移动“视窗”，不复制不修改原文。

```cpp
std::string_view sv = "  header: value  ";
sv.remove_prefix(2);           // "header: value  "
sv = sv.substr(0, sv.size()-2);// "header: value"
```

## 与容器/哈希联动（异构查找）

- `std::map<std::string, T, std::less<>> m;` 可用 `string_view` 直接查找：`m.find(std::string_view{"key"});`
- `std::unordered_map<std::string, T, /*透明 hasher*/, /*透明 equal*/>`（C++20）既保留 `std::string` 作为拥有键，又支持用 `string_view` 零拷贝查找，避免构造临时 `std::string`。

示例透明 hasher/equal：

```cpp
struct SvHash {
  using is_transparent = void;
  size_t operator()(std::string_view s) const noexcept {
    return std::hash<std::string_view>{}(s);
  }
  size_t operator()(const std::string& s) const noexcept { return (*this)(std::string_view{s}); }
  size_t operator()(const char* s) const noexcept { return (*this)(std::string_view{s}); }
};
struct SvEq {
  using is_transparent = void;
  bool operator()(std::string_view a, std::string_view b) const noexcept { return a == b; }
};
// std::unordered_map<std::string, T, SvHash, SvEq> um;
// um.find(std::string_view{"key"}); // 零拷贝查找
```

注意：键类型依然是 `std::string`（拥有内存），`string_view` 仅用于查找时的临时视图，避免了悬垂问题。

## 小贴士

- `string_view::data()` 不保证 `\0`；`string::c_str()` 才保证。
- `string_view::substr()` 若 `pos > size()` 会抛 `std::out_of_range`；`string::substr()` 也会，留意边界。
- 想要“廉价前缀/后缀去除”优先用 `remove_prefix/remove_suffix`，零拷贝。
- 需要线程安全的长期保存，务必拷贝成 `std::string`。
