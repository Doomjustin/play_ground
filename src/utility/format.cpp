#include "format.h"

namespace pg {

auto format_as(const std::error_code& ec) -> std::string
{
    return ec.message();
}

} // namespace pg