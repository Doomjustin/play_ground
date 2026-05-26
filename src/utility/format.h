#ifndef PLAYGROUND_UTILITY_FORMAT_H
#define PLAYGROUND_UTILITY_FORMAT_H

#include <format>
#include <ostream>
#include <sstream>
#include <string>
#include <system_error>
#include <type_traits>

#include <magic_enum/magic_enum.hpp>

namespace pg {

auto format_as(const std::error_code& ec) -> std::string;

template<typename T>
concept has_format_as = requires(const T& t) { format_as(t); };

template<typename T>
concept has_to_string = requires(const T& t) { t.to_string(); };

template<typename T>
concept has_to_repr = requires(const T& t) { t.to_repr(); };

template<typename T>
concept has_ostream = requires(const T& t, std::ostream& os) { os << t; };

template<typename T>
concept user_defined_type =
    std::is_class_v<std::remove_cvref_t<T>> || std::is_union_v<std::remove_cvref_t<T>>;

} // namespace pg

namespace std {

template<typename T>
    requires pg::has_format_as<T>
struct formatter<T> : formatter<decltype(pg::format_as(std::declval<T>()))> {
    auto format(const T& value, auto& ctx) const
    {
        return formatter<decltype(pg::format_as(value))>::format(pg::format_as(value), ctx);
    }
};

template<typename T>
    requires(!pg::has_format_as<T>) && pg::has_to_string<T>
struct formatter<T> : formatter<std::string> {
    auto format(const T& value, auto& ctx) const
    {
        return formatter<std::string>::format(value.to_string(), ctx);
    }
};

template<typename T>
    requires(!pg::has_format_as<T>) && (!pg::has_to_string<T>) && pg::has_to_repr<T>
struct formatter<T> : formatter<std::string> {
    auto format(const T& value, auto& ctx) const
    {
        return formatter<std::string>::format(value.to_repr(), ctx);
    }
};

template<typename T>
    requires(!pg::has_format_as<T>) && (!pg::has_to_string<T>) && (!pg::has_to_repr<T>) &&
            std::is_enum_v<T>
struct formatter<T> : formatter<std::string_view> {
    auto format(const T& value, auto& ctx) const
    {
        return formatter<std::string_view>::format(magic_enum::enum_name(value), ctx);
    }
};

template<typename T>
    requires(!pg::has_format_as<T>) && (!pg::has_to_string<T>) && (!pg::has_to_repr<T>) &&
            pg::user_defined_type<T> && pg::has_ostream<T>
struct formatter<T> : formatter<std::string> {
    auto format(const T& value, auto& ctx) const
    {
        std::ostringstream os;
        os << value;
        return formatter<std::string>::format(os.str(), ctx);
    }
};

} // namespace std

#endif // PLAYGROUND_UTILITY_FORMAT_H