#pragma once

#include <cstdint>
#include <string>
#include <variant>

#include "nengine/core/entity.hpp"
#include "nengine/core/math.hpp"

namespace nengine::core {

using PropertyValue = std::variant<
    std::monostate,
    bool,
    std::int64_t,
    std::uint64_t,
    double,
    std::string,
    Vec3,
    Quat,
    Entity>;

} // namespace nengine::core
