#pragma once

#include <cstdint>
#include <limits>

namespace nengine::core {

struct Entity {
    using value_type = std::uint64_t;

    value_type value{invalid_value};

    static constexpr value_type index_mask = 0x00000000FFFFFFFFull;
    static constexpr value_type invalid_value = std::numeric_limits<value_type>::max();

    static constexpr Entity invalid() noexcept { return Entity{}; }

    constexpr std::uint32_t index() const noexcept {
        return static_cast<std::uint32_t>(value & index_mask);
    }

    constexpr std::uint32_t generation() const noexcept {
        return static_cast<std::uint32_t>(value >> 32u);
    }

    constexpr bool valid() const noexcept { return value != invalid_value; }

    static constexpr Entity make(std::uint32_t index, std::uint32_t generation) noexcept {
        return Entity{(static_cast<value_type>(generation) << 32u) | index};
    }

    friend constexpr bool operator==(Entity, Entity) = default;
};

} // namespace nengine::core
