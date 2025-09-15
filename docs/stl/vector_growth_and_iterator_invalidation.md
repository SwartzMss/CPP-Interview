title: vector 的增长策略与迭代器失效？
tags:
  - stl
---

## 要点

- `std::vector` 必须保持元素连续存储；当 `size()` 需要超过 `capacity()` 时，会重新分配更大的连续内存并搬移元素。
- 一旦发生重分配（reallocate），所有指向旧元素的迭代器/指针/引用全部失效；未重分配时，影响局部：`end()` 通常失效，某些操作会使插入点及之后的迭代器失效。
- 标准未规定具体增长倍数（实现相关，常见 ~1.5x-2x），但保证 `push_back` 摊还 O(1)。

## 最小示例：观察重分配导致地址变化

```cpp
#include <vector>
#include <iostream>
int main() {
    std::vector<int> v;
    v.push_back(1);
    auto p = v.data(); // 保存旧的底层指针，也可保存迭代器/引用
    std::cout << "cap=" << v.capacity() << " data=" << (void*)v.data() << "\n";

    v.push_back(2); // 可能触发扩容（从 1 到 2）
    std::cout << "cap=" << v.capacity() << " data=" << (void*)v.data() << "\n";
    std::cout << "moved? " << std::boolalpha << (p != v.data()) << "\n";

    // 若 moved 为 true，p/迭代器/引用均已失效，不能再解引用！
}
```

## 预留容量避免重分配

```cpp
#include <vector>
#include <iostream>
int main() {
    std::vector<int> v;
    v.reserve(100);   // 预留容量，避免后续扩容
    v.push_back(1);
    auto p = v.data();
    v.push_back(2);   // 未触发扩容
    std::cout << std::boolalpha << (p == v.data()) << "\n"; // true → 迭代器/指针仍有效（end() 除外）
}
```

## 常见操作的失效规则（不考虑重分配时）

- push_back/emplace_back：已有元素的迭代器/引用有效；`end()` 失效。
- insert(pos, ...)：插入点之前的迭代器/引用有效；插入点及之后的迭代器失效；`end()` 失效。
- erase(pos, ...)：被删位置起及之后的迭代器失效；返回值是下一个有效迭代器；`end()` 失效。
- reserve(n)：若 n 大于当前 capacity，立刻重分配，全部失效；否则不变。
- resize(n)：若导致重分配则全部失效；否则仅 `end()` 变化，新增/被销毁元素的引用不可用。

## 小结

- “是否重分配”是判断是否全体失效的关键；能预估大小时用 `reserve` 降低风险。
- 需要稳定地址时，考虑存索引而不是迭代器/指针，或改用其他结构/容器。

