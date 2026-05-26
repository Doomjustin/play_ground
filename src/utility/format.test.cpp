#include <cstdint>
#include <format>
#include <string>
#include <system_error>

#include <catch2/catch_test_macros.hpp>

#include <utility/format.h>

namespace {

struct sample_with_to_string {
    auto to_string() const -> std::string
    {
        return "sample";
    }
};

struct sample_with_to_repr {
    auto to_repr() const -> std::string
    {
        return "repr";
    }
};

struct sample_with_ostream {};

auto operator<<(std::ostream& os, const sample_with_ostream& value) -> std::ostream&
{
    return os << "stream";
}

enum class sample_enum : std::uint8_t {
    alpha,
};

struct sample_with_format_as {};

auto format_as(const sample_with_format_as& value) -> int
{
    static_cast<void>(value);
    return 42;
}

} // namespace

TEST_CASE("format_as formats std::error_code")
{
    const auto ec = std::make_error_code(std::errc::invalid_argument);

    REQUIRE(std::format("{}", ec) == ec.message());
}

TEST_CASE("format_as passes formatting options through to the returned value")
{
    REQUIRE(std::format("{:#06x}", sample_with_format_as{}) == "0x002a");
}

TEST_CASE("formatter falls back to to_string")
{
    REQUIRE(std::format("{}", sample_with_to_string{}) == "sample");
}

TEST_CASE("formatter falls back to to_repr")
{
    REQUIRE(std::format("{}", sample_with_to_repr{}) == "repr");
}

TEST_CASE("formatter falls back to operator<<")
{
    REQUIRE(std::format("{}", sample_with_ostream{}) == "stream");
}

TEST_CASE("formatter formats enums using their names")
{
    REQUIRE(std::format("{}", sample_enum::alpha) == "alpha");
}
