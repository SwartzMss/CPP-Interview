---
title: vector对象到底是在堆上还是栈上？
tags:
  - stl
---

## 问题

vector对象到底是在堆上还是栈上？

## 回答

`std::vector` 由两部分组成：

1. **控制块**：包括三个指针（`begin` / `end` / `capacity_end`）、尺寸和 allocator 等元信息。
2. **元素存储区**：真正放元素的那片连续内存。

控制块的存放位置取决于对象的声明方式：

- 在函数体中直接定义 `std::vector<int> v;` 时，控制块随对象一起出现在栈上。
- 通过 `new std::vector<int>` 动态创建时，控制块位于堆上。

不论控制块在哪里，元素存储区默认都由 `new` 申请在堆上。扩容时，`vector` 会重新在堆上申请更大的空间，将原有元素移动过去并释放旧空间。

### 为什么元素必须在堆上？

栈空间大小固定，且其生命周期由作用域决定。`std::vector` 需要支持运行时扩容和元素个数未知的场景，因此只能把元素存放在堆（或由分配器提供的"动态存储区"）中。

### 例子：观察地址

```cpp
void demo() {
    std::vector<int> v{1, 2, 3};

    std::cout << "address of v: " << &v << '\n';
    std::cout << "address of elements: " << v.data() << '\n';
}
```

在大多数 64 位平台上，打印结果类似：

```
address of v: 0x7ffd23c8e720   // 控制块，在栈上
address of elements: 0x55b8c8d0eeb0  // 元素，在堆上
```

如果把 `v` 改为 `auto v = new std::vector<int>{1,2,3};`，则 `&v` 是栈上的指针，而 `v`（控制块）和 `v->data()`（元素）都在堆上。

### 分配器的影响

`std::vector` 通过模板参数中的分配器管理元素存储区。使用 `std::pmr::monotonic_buffer_resource` 等自定义分配器可以让元素来自内存池、共享内存甚至显存，但本质上仍是动态获取的内存，而非栈空间。

### 与 std::array 对比

`std::array<int,3>` 的元素和控制块是一个整体，通常位于栈上，大小在编译期已知，不会扩容。

## 小结

- 控制块（`std::vector` 对象本身）在栈还是堆取决于创建方式。
- 元素存储区默认在堆上，通过分配器管理。
- 这种设计使 `std::vector` 支持运行时扩容和资源自动管理（RAII）。

