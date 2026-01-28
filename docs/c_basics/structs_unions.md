---
title: 结构体与联合体要点
tags:
  - c
  - 结构体
  - 联合体
---

## 结构体

- 内存对齐按最大成员对齐，必要时补齐填充；示例 `char + int + short` 得到 12 字节。
- 调整成员顺序可减填充：`int + short + char` 压缩到 8 字节。
- 嵌套结构体继续对齐：含前述结构的包裹体可达 20 字节。
- 结构体赋值默认逐成员拷贝（浅拷贝）。
- 含柔性数组（flexible array member）时，`sizeof` 仅含固定部分，例如 `int len; char data[];` 结果等于 `sizeof(int)`。

### 示例代码

```c
#include <stdio.h>

struct A1 { char c; int i; short s; };
struct A2 { int i; short s; char c; };

int main(void) {
    printf("sizeof(A1)=%zu\n", sizeof(struct A1)); // 12
    printf("sizeof(A2)=%zu\n", sizeof(struct A2)); // 8（成员重排减少填充）

    struct A2 a = {1, 2, 'x'}, b = {3, 4, 'y'};
    a = b; // ✅ 按成员拷贝
    printf("a: %d %d %c\n", a.i, a.s, a.c);

    struct FA { int len; char data[]; };
    printf("sizeof(FA)=%zu\n", sizeof(struct FA)); // 4，仅含固定部分
    return 0;
}
```

## 联合体

- 联合体大小等于最大成员大小并按最大对齐取整，例如 `int` 与 `char[4]` 联合为 4 字节。
- 写入一个成员后读另一个成员会暴露字节序：`u.i = 0x12345678; u.c[0]` 小端得 `0x78`，大端得 `0x12`。
- 典型用途：节省内存、协议/寄存器解析、类型复用。

### 示例代码

```c
#include <stdio.h>

union U {
    int i;
    char c[4];
};

int main(void) {
    union U u;
    u.i = 0x12345678;
    printf("sizeof(U)=%zu\n", sizeof(union U)); // 4
    printf("lowest byte=0x%02x\n", (unsigned char)u.c[0]); // 小端输出 0x78

    // 同一存储复用：写 char 读 int
    u.c[0] = 1; u.c[1] = 0; u.c[2] = 0; u.c[3] = 0;
    printf("i=%d\n", u.i); // 在小端体系输出 1，演示类型复用
    return 0;
}
```
