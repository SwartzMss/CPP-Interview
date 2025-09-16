---
title: std::map vs std::unordered_map：什么时候用？
tags:
  - stl
---

## 结论先行（怎么选）

- 只做高频点查/插入，顺序无要求：用 `std::unordered_map`
- 需要按键有序、范围查询、前驱/后继、最小/最大：用 `std::map`
- 需要稳定最坏复杂度或更稳迭代器（插入不让其它迭代器失效）：用 `std::map`
- 大批量导入且只点查：`std::unordered_map` 并提前 `reserve`

## 使用场景

### 什么时候用 std::unordered_map（哈希表）

- 高频“按键直查”或计数器、缓存（ID→对象、字符串→对象）
- 数据量大、追求吞吐与低延迟，键分布均匀
- 只需要存在性判断/插入/删除，不需要有序语义

特性与要点：
- 均摊 O(1) 查/插/删；无序
- 发生 rehash 时，所有迭代器失效（引用/指针仍有效）
- 大批量插入前 `reserve(N)`，必要时调 `max_load_factor`

示例：

```cpp
#include <unordered_map>

std::unordered_map<int, Info> u;
u.reserve(1'000'000);             // 批量插入前预留
u.max_load_factor(0.8f);          // 视内存/性能权衡

if (auto it = u.find(42); it != u.end()) {
    // 点查命中
}
```

### 什么时候用 std::map（红黑树，有序）

- 需要按键有序遍历、区间查询（[l, r)）、找前驱/后继
- 需要随时取最小/最大键、有序导出报表
- 需要稳定上界（始终 O(log N)）与更稳的迭代器（插入不使其他迭代器失效）

特性与要点：
- 查/插/删 O(log N)，有序；`lower_bound/upper_bound` 友好
- 插入不失效其他迭代器；擦除仅使被删元素失效

示例（区间导出）：

```cpp
#include <map>

std::map<int, Info> m;
// ... 插入若干
for (auto it = m.lower_bound(l); it != m.end() && it->first < r; ++it) {
    // 处理 [l, r) 区间
}
```

## 简明对比

- 有序能力：`map` 有；`unordered_map` 无
- 复杂度：`map` O(log N) 稳定；`unordered_map` 均摊 O(1)，最坏可退化
- 迭代器：`map` 插入不失效；`unordered_map` rehash 全失效
- 内存：`map` 节点开销大（多指针）；`unordered_map` 桶+节点，负载因子可调

## 自定义“索引”（排序/哈希）

两者都可定制：

- `std::map` 自定义比较器（定制排序规则，等价于“自定义有序索引”）

```cpp
struct ByLenThenLex {
    using is_transparent = void; // 支持 string_view 异构查找
    bool operator()(std::string_view a, std::string_view b) const noexcept {
        return a.size() == b.size() ? a < b : a.size() < b.size();
    }
};
std::map<std::string, int, ByLenThenLex> m;
// m.find(std::string_view{"abc"}); // 零拷贝查找
```

- `std::unordered_map` 自定义哈希与相等（定制“无序索引”）

```cpp
struct Key { int id; std::string tag; };
struct KeyHash {
    size_t operator()(Key const& k) const noexcept {
        size_t h1 = std::hash<int>{}(k.id);
        size_t h2 = std::hash<std::string>{}(k.tag);
        return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1<<6) + (h1>>2));
    }
};
struct KeyEq {
    bool operator()(Key const& a, Key const& b) const noexcept {
        return a.id == b.id && a.tag == b.tag;
    }
};
std::unordered_map<Key, int, KeyHash, KeyEq> u;
```

> 多索引需求（同一份数据按不同字段查）：标准容器不支持内建多索引，常见做法是维护多份容器（比如按 id 的 `unordered_map` + 按时间的 `map`）并在写入时双写；或使用 Boost.MultiIndex。

## 常见坑点与小技巧

- `unordered_map` 批量导入要一次性 `reserve(N)`，避免多次 rehash 抖动
- `unordered_map` rehash 会使所有迭代器失效；`map` 无 rehash 概念
- 字符串键想零拷贝查找：
  - `std::map<std::string, T, std::less<>>` → 可用 `string_view`/`char*` 查找
  - `std::unordered_map<std::string, T, /*透明 hasher*/, /*透明 equal*/>`（C++20）
- 只需“插入顺序遍历”？两者都不保证；可额外维护 `std::list` 记录顺序或用第三方“linked hash map”

## 快速决策清单

- 要“有序/范围/前驱后继”？→ `std::map`
- 只要快点查，数据量大？→ `std::unordered_map`（记得 `reserve`）
- 需要稳定上界或更稳迭代器？→ `std::map`
- 既要快点查又要有序导出？→ 点查用 `unordered_map`，导出维护一份 `map`（双索引）

