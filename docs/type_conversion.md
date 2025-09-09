---
title: C++ 四种类型转换
tags:
  - language
---

# C++ 四种类型转换

一图速览：C++ 四种类型转换

| 转换 | 用途 | 检查时机 | 限制 | 风险 |
| --- | --- | --- | --- | --- |
| `static_cast` | 基本转换 / 上行转型 | 编译期 | 不改 `const` / 无运行时检查 | 中 |
| `dynamic_cast` | 多态下行转型 | 运行期 | 需虚函数；失败返回空/抛异常 | 低 |
| `const_cast` | 改 `const`/`volatile` | 编译期 | 真常量不可改 | 视用法 |
| `reinterpret_cast` | 底层重解释 | 编译期 | 不做检查 | 高 |

## 关键要点

- `static_cast`：基本类型转换；向上转型安全；不能去掉 `const/volatile`。
- `dynamic_cast`：需多态，安全下行；失败时指针为 `nullptr` 或抛异常。
- `const_cast`：只改限定符，修改真常量未定义。
- `reinterpret_cast`：位级重解释，风险最高，慎用。

## 实战建议

1. 能不用就不用，避免不必要的转换。
2. 首选 `static_cast`；向下转型需 `dynamic_cast`。
3. 仅改限定符用 `const_cast`。
4. `reinterpret_cast` 只在底层需求且明确后果时使用。

## 代码模式记忆卡

```cpp
if (auto p = dynamic_cast<Derived*>(base)) {
    // use p
}

try {
    auto& d = dynamic_cast<Derived&>(base);
} catch (const std::bad_cast&) {
    // 失败处理
}

const int* p = /* ... */;
int* q = const_cast<int*>(p); // 去 const（谨慎）
int x = 0;
const int* r = const_cast<const int*>(&x); // 加 const

std::uintptr_t u = reinterpret_cast<std::uintptr_t>(ptr);
auto* p2 = reinterpret_cast<Foo*>(u);
```

## `static_cast`

### 基本类型转换

```cpp
int i = 42;
double d = static_cast<double>(i);      // 42 -> 42.0

double pi = 3.14;
int truncated = static_cast<int>(pi);   // 结果: 3
```

> `static_cast` 会遵循既定的转换规则，例如将浮点的小数部分舍弃。

### 类层次之间的转换

```cpp
struct Animal {
    virtual ~Animal() = default;
};
struct Dog : Animal {
    void bark();
};

Dog dog;
Animal* a1 = static_cast<Animal*>(&dog);    // 上行转换，安全

Animal* base = &dog;
Dog* d1 = static_cast<Dog*>(base);          // 下行转换，需确保 base 的真实对象确实是 Dog
```

> 如果 `base` 实际指向 `Animal` 而非 `Dog`，使用 `d1->bark()` 会导致未定义行为。若无法保证，考虑 `dynamic_cast`。

## `dynamic_cast`

### 指针的安全下行转换

```cpp
Animal* base = new Dog{};
if (auto* d = dynamic_cast<Dog*>(base)) {
    d->bark();      // 转换成功
} else {
    // base 不是 Dog*，d 为 nullptr
}
```

### 引用的安全下行转换

```cpp
Animal& ref = dog;  // dog 与上文一致
try {
    Dog& dref = dynamic_cast<Dog&>(ref);
    dref.bark();
} catch (const std::bad_cast&) {
    // 转换失败，抛出异常
}
```

> `dynamic_cast` 依赖多态类型的 RTTI，仅适用于带虚函数的类。

## `const_cast`

### 去除常量性

```cpp
void takes_int(int*);

const int value = 10;
int* writable = const_cast<int*>(&value); // value 是真常量，修改将导致 UB

int n = 20;
takes_int(const_cast<int*>(&n));          // n 可写，可安全修改
```

### 添加常量性

```cpp
void print(const int*);

int x = 5;
print(const_cast<const int*>(&x));        // 将非常量指针视为 const
```

## `reinterpret_cast`

### 指针与整数之间

```cpp
int data = 0x12345678;
std::uintptr_t raw = reinterpret_cast<std::uintptr_t>(&data);
int* p = reinterpret_cast<int*>(raw); // raw 再转回指针
```

### 读取对象的字节表示

```cpp
unsigned char* bytes = reinterpret_cast<unsigned char*>(&data);
for (std::size_t i = 0; i < sizeof(int); ++i) {
    std::printf("%02x ", bytes[i]);
}
```

> `reinterpret_cast` 不改变位模式，只是从新的角度解读内存。使用前务必确认对齐、字节序等问题。
