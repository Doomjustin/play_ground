# std::format 强化器

## 引入
C++20 引入了 `std::format`，它把字符串拼接和格式化输出带到了标准库里，语法也比传统的 `operator<<` 更清爽。

但它有一个现实问题：当你面对的是一堆项目里的自定义类型时，标准库并不会自动知道“该怎么把它们格式化成字符串”。如果每个类型都去手写 `std::formatter` 特化，成本会很高，而且维护起来也很碎。

所以这里做了一个小而实用的“强化器”：让 `std::format` 尽量自动识别几种常见的输出方式，并按固定优先级选择最合适的格式化结果。

## 设计目标

这套实现的目标很简单：

- 尽量少写模板代码
- 尽量复用类型本身已经提供的字符串化能力
- 对常见场景提供默认回退
- 让 `std::format("{}", value)` 在项目里更接近“开箱即用”

## 整体思路

当前实现放在 [src/utility/format.h](../../../src/utility/format.h) 和 [src/utility/format.cpp](../../../src/utility/format.cpp) 中。

核心做法是两层：

1. 先为个别标准类型提供 `format_as`
2. 再通过一组 `std::formatter` 特化，为用户类型建立统一的回退链路

它的判断顺序是固定的：

| 优先级 | 规则 | 结果 |
| --- | --- | --- |
| 1 | 存在 `format_as(value)` | 直接复用 `format_as` 的结果 |
| 2 | 存在 `to_string()` | 使用 `to_string()` |
| 3 | 存在 `to_repr()` | 使用 `to_repr()` |
| 4 | 枚举类型 | 使用 `magic_enum::enum_name(value)` |
| 5 | 支持 `operator<<` 的用户定义类型 | 通过流输出到 `std::ostringstream` |

这意味着，只要你的类型满足上面任意一种能力，它就可以直接参与 `std::format`。

## 代码结构

### `format_as`

在 [src/utility/format.cpp](../../../src/utility/format.cpp) 中，只为 `std::error_code` 提供了一个 `format_as`：

```cpp
namespace pg {

auto format_as(const std::error_code& ec) -> std::string
{
	return ec.message();
}

} // namespace pg
```

这代表 `std::error_code` 会被格式化成它的错误信息文本，而不是默认的原始对象表现。

### `has_*` 概念

在 [src/utility/format.h](../../../src/utility/format.h) 中，先定义了一组检测概念：

```cpp
template<typename T>
concept has_format_as = requires(const T& t) { format_as(t); };

template<typename T>
concept has_to_string = requires(const T& t) { t.to_string(); };

template<typename T>
concept has_to_repr = requires(const T& t) { t.to_repr(); };

template<typename T>
concept has_ostream = requires(const T& t, std::ostream& os) { os << t; };
```

这些概念的作用不是“做业务判断”，而是帮助 `std::formatter` 在编译期选路。

### `user_defined_type`

还有一个辅助概念，用来限制流输出回退只作用于用户定义类型：

```cpp
template<typename T>
concept user_defined_type =
	std::is_class_v<std::remove_cvref_t<T>> || std::is_union_v<std::remove_cvref_t<T>>;
```

这样可以避免把过于宽泛的流输出逻辑扩散到不该处理的类型上。

## 格式化优先级

真正的核心在 `std::formatter` 的一组偏特化上。它们按照“最明确的格式化语义优先”的原则排列。

### 1. `format_as` 优先

如果类型能够通过 `format_as` 转换成别的值，就直接格式化转换后的结果。

这是最灵活的一层，适合把一个类型映射到另一个已经成熟可格式化的表示。

### 2. `to_string()` 回退

如果没有 `format_as`，但类型自己提供了 `to_string()`，就直接使用它。

这很适合“对象本来就知道如何转成字符串”的类型。

### 3. `to_repr()` 回退

如果连 `to_string()` 都没有，但有 `to_repr()`，就使用 `to_repr()`。

这通常更偏调试风格：它不一定是面向用户的文案，但足够表达对象内容。

### 4. 枚举名回退

如果是枚举类型，并且前面几层都不满足，就用 `magic_enum::enum_name(value)` 取枚举名。

这一步的好处是，枚举不再只能打印底层整数值，而是直接输出语义更强的名字。

### 5. `operator<<` 回退

最后，如果类型是用户定义类型，并且支持 `operator<<`，就把它流式输出到 `std::ostringstream`，再把结果交给 `std::format`。

这能兼容大量已经在项目里写过流输出的旧类型。

## 使用示例

### `std::error_code`

```cpp
const auto ec = std::make_error_code(std::errc::invalid_argument);

REQUIRE(std::format("{}", ec) == ec.message());
```

这说明 `std::error_code` 最终会输出它的错误文本，例如 `Invalid argument`。

### `to_string()`

```cpp
struct sample_with_to_string {
	auto to_string() const -> std::string
	{
		return "sample";
	}
};

REQUIRE(std::format("{}", sample_with_to_string{}) == "sample");
```

### `to_repr()`

```cpp
struct sample_with_to_repr {
	auto to_repr() const -> std::string
	{
		return "repr";
	}
};

REQUIRE(std::format("{}", sample_with_to_repr{}) == "repr");
```

### `operator<<`

```cpp
struct sample_with_ostream {};

auto operator<<(std::ostream& os, const sample_with_ostream& value) -> std::ostream&
{
	return os << "stream";
}

REQUIRE(std::format("{}", sample_with_ostream{}) == "stream");
```

### 枚举类型

```cpp
enum class sample_enum : std::uint8_t {
	alpha,
};

REQUIRE(std::format("{}", sample_enum::alpha) == "alpha");
```

## 适用场景

这套封装特别适合下面几类场景：

- 项目里有很多轻量类型，不想逐个写 `std::formatter`
- 已经有 `to_string()` / `to_repr()` 的代码风格
- 大量使用枚举，需要稳定、可读的名字输出
- 旧代码里已经写了 `operator<<`，想平滑接入 `std::format`

## 需要注意的点

### 这是对 `std` 的定制

这里通过 `std::formatter` 特化把行为接进了标准格式化流程。它很实用，但也意味着你要对“格式化规则的全局影响”保持清醒认识。

### 优先级会影响最终输出

如果一个类型同时满足多个条件，例如既有 `to_string()` 又有 `operator<<`，最终只会走更靠前的那条路径。

### `operator<<` 回退只是兼容层

它解决的是“能格式化”，不是“最优格式化”。如果类型本身能提供更直接的字符串语义，优先实现 `format_as`、`to_string()` 或 `to_repr()` 会更清晰。

## 小结

这份实现的本质不是“重写 `std::format`”，而是给它补一层项目级别的自动适配：

- 标准错误码走 `format_as`
- 普通对象优先用自己的字符串接口
- 枚举输出名字
- 旧类型通过流输出兜底

这样一来，`std::format` 在项目里就不只是一个标准库工具，而是一个更贴近业务对象的统一格式化入口。
