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

## 代码示例

```c
// 1) 字符串字面量不可改写（未定义行为/运行期可能崩溃）
char *p = "hello";
p[0] = 'H';   // ❌ UB，很多平台会段错误

// 合法写法
char s1[] = "hello"; // 栈上可写副本
s1[0] = 'H';          // ✅
char *s2 = malloc(6);
strcpy(s2, "hello"); // ✅ 记得 free
```

```c
// 2) sizeof vs strlen
char a[] = "abc";    // sizeof(a) == 4 (含'\0')
char *p = a;          // sizeof(p) == 8 (64-bit)
printf("%zu %zu %zu\n", sizeof(a), sizeof(p), strlen(a));
```

```c
// 3) const 指针位置决定限制
const char *p1 = "hi";   // 内容不可改
// p1[0] = 'H';           // ❌ 编译错误：read-only location
p1 = "ok";               // ✅ 指针可改

char * const p2 = malloc(3); // 指针常量，不可改指向
// p2 = NULL;                 // ❌ 编译错误：assignment of read-only variable
p2[0] = 'a';                 // ✅ 内容可改

const char * const p3 = "yo"; // 两者都不可改
// p3 = NULL;                 // ❌
// p3[0] = 'Y';               // ❌
```

```c
// 4) const 形参指针可以移动
void foo(const char *p) {
    p++;   // ✅ 合法：仅改变形参副本
    // *p = 'x';             // ❌ 通过 const 指针写会编译错误
}
```
