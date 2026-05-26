# 01

## 引入

C++20 引入了 `std::format`，它把字符串拼接和格式化输出带到了标准库里，语法也比传统的 `operator<<` 更清爽。

但它有一个现实问题：当你面对的是一堆项目里的自定义类型时，标准库并不会自动知道“该怎么把它们格式化成字符串”。如果每个类型都去手写 `std::formatter` 特化，成本会很高，而且维护起来也很碎。

所以这里做了一个小而实用的“强化器”：让 `std::format` 尽量自动识别几种常见的输出方式，并按固定优先级选择最合适的格式化结果。

其实本身`std::format` 的前身 `fmt` 在新版本中就提供了基于ADL的 `format_as` 方式，但是似乎在引入标准库的时候还没有这个方案，所以 `std::format` 也没有这个功能

## 目标

这套实现的目标很简单：

* 尽量少写模板代码
* 尽量复用类型本身已经提供的字符串化能力
* 对常见场景提供默认回退
* 让 `std::format("{}", value)` 在项目里更接近“开箱即用”

## 思路

&#x20;通过 `std::formatter` 特化，为用户类型建立统一的回退链路。

除了 fmt 的 format\_as 方式，我们顺手再引入类似Python的\_\_str\_\_和\_\_repr\_\_方式，按照优先级来自动格式化，以及兼容一下传统的流式输出。这样对于一些提供了流式输出的类，能更丝滑的兼容

这里有个对于enum类型的特化，其实每当我们想输出enum类型是，大部分情况下都想看到的是具体定义，而不是底层数据的值，比如

```
enum class Color { Red };
std::format("{}", Color::Red); 
```

大部分情况下都希望输出的是Red，而不是0这样的数字。想实现这个方法在没有反射的时代很麻烦，所以这里我们用一个第三库来做，enum\_magic。

我们这里定义一下查找顺序规则

<table><thead><tr><th width="88.20001220703125">优先级</th><th width="292.20001220703125">规则</th><th>结果</th></tr></thead><tbody><tr><td>1</td><td>存在 <code>format_as(value)</code></td><td>直接复用 <code>format_as</code> 的结果</td></tr><tr><td>2</td><td>存在 <code>to_string()</code></td><td>使用 <code>to_string()</code></td></tr><tr><td>3</td><td>存在 <code>to_repr()</code></td><td>使用 <code>to_repr()</code></td></tr><tr><td>4</td><td>枚举类型</td><td>使用 <code>magic_enum::enum_name(value)</code></td></tr><tr><td>5</td><td>支持 <code>operator&#x3C;&#x3C;</code> 的用户定义类型</td><td>通过流输出到 <code>std::ostringstream</code></td></tr></tbody></table>

这意味着，只要你的类型满足上面任意一种能力，它就可以直接参与 `std::format`。

### `has_*` 概念

先定义了一组检测概念：

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

* 项目里有很多轻量类型，不想逐个写 `std::formatter`
* 已经有 `to_string()` / `to_repr()` 的代码风格
* 大量使用枚举，需要稳定、可读的名字输出
* 旧代码里已经写了 `operator<<`，想平滑接入 `std::format`

## 需要注意的点

### 这是对 `std` 的定制

这里通过 `std::formatter` 特化把行为接进了标准格式化流程。它很实用，但也意味着你要对“格式化规则的全局影响”保持清醒认识。

### 优先级会影响最终输出

如果一个类型同时满足多个条件，例如既有 `to_string()` 又有 `operator<<`，最终只会走更靠前的那条路径。

### `operator<<` 回退只是兼容层

它解决的是“能格式化”，不是“最优格式化”。如果类型本身能提供更直接的字符串语义，优先实现 `format_as`、`to_string()` 或 `to_repr()` 会更清晰。
