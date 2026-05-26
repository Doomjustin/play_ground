#include <cstdlib>
#include <format>
#include <iostream>
#include <sstream>

template<typename T>
concept has_format_as = requires(const T& t) { format_as(t); };

template<typename T>
concept has_ostream = requires(const T& t, std::ostream& os) { os << t; };

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

    return EXIT_SUCCESS;
}