---
description: C++20引入了std::format作为格式化工具，但是用户自定义类型的格式化的API却十分难用。我们想办法让他变得用户友好一点。
---

# std::format增强

### 1. 如何给用户自定义类型定义格式化输出

假如我们想要给下面这个自定义类型定义格式化

```cpp
struct Color {
    int r;
    int g;
    int b;
};
```

std::format提供给我们的方案是继承std::formatter\<T>，实现一个format方法，如下

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

然后就可以正常的格式化输出了

```cpp
Color color{ .r = 255, .g = 125, .b = 123 };
std::cout << std::format("Color: {}\n", color);
// 输出：Color: (255, 125, 123)
```

观察format的实现，可以发现，Formatter::format(xx, ctx)是一个完全样板式的代码，我们真正自定义的部分其实只有std::format("({}, {}, {})", color.r, color.g, color.b)。

特别是对于一些简单的格式化代码，如果我们不需要对格式有更精细的控制需求，其实就只需要写一句std::format("({}, {}, {})", color.r, color.g, color.b)就足够了。

因此，我们的目标是对上面的用法做出简化，通过模板，只要用户写了特定的方法，我们就调用它，替换这里的std::format()操作。类似下面这样

```cpp
// 1.假设用户自定义了这个方法
auto format_as(const Color& color) -> std::string
{
    return std::format("({}, {}, {})", color.r, color.g, color.b);
}

// 2.自动调用format方法格式化Color
// 也就是说这里会自动生成类似下面的代码
template<>
struct std::formatter<Color> : std::formatter<std::string_view> {
    auto format(const Color& color, auto& ctx) const
    {
        using Formatter = std::formatter<std::string_view>;
        return Formatter::format(format_as(color), ctx);
    }
};
```

这样一来，用户只需要写一个十分简单的format\_as方法就能完美融入std::format格式化输出了。

{% hint style="info" %}
其实在新版本的fmt库里已经提供了类似的自定义格式化方式，但是被引入标准库时还没有这个实现，所以标准库里的用户自定义方法才这么难用
{% endhint %}

### 2. 怎么做？

如果是在C++20以前，实现这里的代码会显得十分啰嗦，但是C++20引入了concep之后，模板分发就变得十分简单了。

具体而言，我们需要定义一个探测用户是否自定义了format\_as方法的concept

```cpp
template<typename T>
concept has_format_as = requires(const T& t) { format_as(t); };
```

对于返回值，没有任何要求，他可以返回任何能够被Formatter::format()接收的类型。

接着我们就可以基于这个concept实现模板分发了

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

由于requires has\_forma\_as\<T>，所以只有在有对应的format\_as方法时，才会匹配这个模板，实现了完美的静态分发。

由于我们不知道format\_as的返回值类型，所以用了decltype(format\_as(value))来获得其返回值类型。

看看效果

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

### 3. 更进一步

其实有了format\_as的方式之后，用户自定义类型的格式化已经友好很多了，不过我们可以更进一步，做一些更激进的优化。

其实很多时候，格式化输出都是用在打log或者开发的时候print出来看看值这种程度，所以相比于易用性，性能可以暂缓。由于C++早期都是通过定义operator<<的方式来做格式化，我们可以兼容这个操作。

具体而言，优先查找format\_as，如果没有，就继续探查是否支持operator<<操作，如果可以，就用operator<<来做格式化输出。

手法是一样的，先定义一个探查用的concept

```c++
template<typename T>
concept has_ostream = requires(const T& t, std::ostream& os) { os << t; };
```

这里需要注意优先级，只有不满足format\_as时才会查找operator<<操作，所以formatter实现如下

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

{% hint style="warning" %}
如果这里不加!has\_format\_as\<T>的话，就会发生冲突，如果用户同时定义了format\_as和operator<<，用谁？
{% endhint %}

[完整实现](../../../src/utility/format.h)
