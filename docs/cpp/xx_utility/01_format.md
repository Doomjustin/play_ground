---
description: C++20引入了std::format作为格式化工具，但是用户自定义类型的格式化的API却十分难用。我们想办法让他变得用户友好一点。
---

# std::format增强

### 1. 问题定义

`std::format` 对标准类型支持完整，但对用户自定义类型，通常需要显式特化 `std::formatter<T>`。这会带来两个问题：

1. 每个类型都要重复写样板代码
2. 真正有语义的逻辑只占很小一部分

先看一个简单类型：

```cpp
struct Color {
    int r;
    int g;
    int b;
};
```

标准做法是特化 `std::formatter<Color>`：

```cpp
template<>
struct std::formatter<Color> : std::formatter<std::string_view> {
    auto format(const Color& color, auto& ctx) const
    {
        using Formatter = std::formatter<std::string_view>;
        return Formatter::format(std::format("({}, {}, {})", color.r, color.g, color.b), ctx);
    }
};
```

从结构上看，`Formatter::format(..., ctx)` 几乎固定，唯一与业务有关的是这句：

`std::format("({}, {}, {})", color.r, color.g, color.b)`

因此，这篇实现要解决的核心是：

- 把样板代码统一收敛到模板层
- 让类型只暴露自己的格式化语义

一个自然的目标接口就是 `format_as`：

```cpp
auto format_as(const Color& color) -> std::string
{
    return std::format("({}, {}, {})", color.r, color.g, color.b);
}
```

当类型提供 `format_as` 时，模板层自动把它接入 `std::format`。

```cpp
template<>
struct std::formatter<Color> : std::formatter<std::string_view> {
    auto format(const Color& color, auto& ctx) const
    {
        using Formatter = std::formatter<std::string_view>;
        return Formatter::format(format_as(color), ctx);
    }
};
```

最终调用方式保持不变：

```cpp
Color color{ .r = 255, .g = 125, .b = 123 };
std::cout << std::format("Color: {}\n", color);
// 输出：Color: (255, 125, 123)
```

{% hint style="info" %}
新版本 fmt 已经提供了类似思路；但进入标准库时并没有完整带上这套体验，这也是 `std::format` 在自定义类型场景里显得偏重的原因。
{% endhint %}

### 2. 分发机制

这套实现的基础是 **编译期能力探测**。先定义一个 `concept`，判断某个类型是否存在 `format_as`：

```cpp
template<typename T>
concept has_format_as = requires(const T& t) { format_as(t); };
```

然后基于 `has_format_as<T>` 做 `std::formatter` 的模板分支：

```cpp
template<typename T>
    requires has_format_as<T>
struct std::formatter<T> : formatter<decltype(format_as(std::declval<T>()))> {
    auto format(const T& value, auto& ctx) const
    {
        using Result = decltype(format_as(value));
        using Formatter = std::formatter<Result>;

        return Formatter::format(format_as(value), ctx);
    }
};
```

这个分支的语义是：

- 如果 `T` 有 `format_as`，则把 `T` 映射到 `Result`
- `Result` 交给现成的 `std::formatter<Result>` 处理
- 格式字符串里的格式选项自然透传到 `Result` 的 formatter

### 3. `decltype` 在这里的作用

`decltype` 用来在 **编译期提取表达式类型**。

最常见写法：

```cpp
int x = 0;
decltype(x) a = 1;   // a 的类型是 int

const int& ref = x;
decltype(ref) b = x; // b 的类型是 const int&
```

在本实现里，`decltype` 出现了两次，分别承担两个职责：

1. `decltype(format_as(std::declval<T>()))`

- 用在基类位置
- 目的是在不构造 `T` 对象的前提下，获得 `format_as(T)` 的返回类型

2. `decltype(format_as(value))`

- 用在 `format` 函数体内
- 作用同样是获取返回类型，只是这里已有 `value` 实参可用

为什么要配合 `std::declval<T>()`？

- `declval` 能在仅类型上下文里生成一个 `T` 的右值引用表达式
- 它不会真正构造对象
- 非常适合模板元编程场景下做类型推导

这也是下列声明能成立的原因：

```cpp
template<typename T>
    requires has_format_as<T>
struct std::formatter<T> : formatter<decltype(format_as(std::declval<T>()))> {
    auto format(const T& value, auto& ctx) const
    {
        using Result = decltype(format_as(value));
        using Formatter = std::formatter<Result>;

        return Formatter::format(format_as(value), ctx);
    }
};
```

如果 `format_as` 返回 `std::string`，这里就继承 `formatter<std::string>`；如果返回 `int`，就继承 `formatter<int>`。因此格式说明符会遵循返回类型自身的规则。

### 4. 回退策略

仅支持 `format_as` 还不够。很多历史类型只实现了 `operator<<`，所以可以增加一条 **流输出回退**。

先定义能力探测：

```cpp
template<typename T>
concept has_ostream = requires(const T& t, std::ostream& os) { os << t; };
```

再定义次优先级分支：

```cpp
template<typename T>
    requires(!has_format_as<T> && has_ostream<T>)
struct std::formatter<T> : formatter<std::string> {
    auto format(const T& value, auto& ctx) const
    {
        std::ostringstream os;
        os << value;

        using Formatter = std::formatter<std::string>;
        return Formatter::format(os.str(), ctx);
    }
};
```

这里的关键约束是 `!has_format_as<T>`：

- 明确优先级：`format_as` 高于 `operator<<`
- 避免两个模板分支同时可行导致二义性

{% hint style="warning" %}
如果去掉 `!has_format_as<T>`，当类型同时提供 `format_as` 与 `operator<<` 时，模板匹配会冲突。
{% endhint %}

{% hint style="success" %}
建议把 `format_as` 作为主语义接口，`operator<<` 作为兼容旧类型的兜底机制。
{% endhint %}

### 5. 完整示例

```cpp
#include <cstdlib>
#include <format>
#include <iostream>

template<typename T>
concept has_format_as = requires(const T& t) { format_as(t); };

template<typename T>
    requires has_format_as<T>
struct std::formatter<T> : formatter<decltype(format_as(std::declval<T>()))> {
    auto format(const T& value, auto& ctx) const
    {
        using Result = decltype(format_as(value));
        using Formatter = std::formatter<Result>;

        return Formatter::format(format_as(value), ctx);
    }
};

struct Color {
    int r;
    int g;
    int b;
};

auto format_as(const Color& color) -> std::string
{
    return std::format("({}, {}, {})", color.r, color.g, color.b);
}

int main(int argc, char* argv[])
{
    Color color{ .r = 255, .g = 125, .b = 123 };
    std::cout << std::format("Color: {}\n", color);
    // 输出: Color: (255, 125, 123)
    return EXIT_SUCCESS;
}
```

### 6. 小结

这套设计的原理可以概括为三点：

1. 用 `concept` 做编译期能力探测
2. 用 `decltype` + `declval` 建立返回类型到 formatter 的映射
3. 用约束表达优先级，形成可扩展的回退链

[完整实现](../../../src/utility/format.h)
