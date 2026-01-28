---
title: C 语言基础速览
tags:
  - c
  - 基础
---

## 速记要点

- 字符串字面量不可改写，`char *p = "hello"; p[0] = 'H';` 属于未定义行为；若需修改请使用可写数组或拷贝到堆内存。
- `sizeof` 作用于数组得到整体大小（含末尾 `\0`），作用于指针得到指针本身大小；`strlen` 仅计算首个 `\0` 前的字符数。
- `const` 修饰位置决定“谁不能改”：`const char *p` 禁改内容；`char * const p` 禁改指针；全 `const` 两者都不能改。
- `const` 形参指针可以移动（如 `p++`），只要不通过它修改所指内容。

## 关联示例

```c
char a[] = "abc";    // sizeof(a)=4, strlen=3
char *p = a;          // sizeof(p)=8 (64-bit)
```

```c
void foo(const char *p) {
    p++;   // 合法：仅移动指针
}
```
