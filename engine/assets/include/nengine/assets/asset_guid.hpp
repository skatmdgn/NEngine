#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace nengine::assets {

struct AssetGuid {
    std::uint64_t high{0};
    std::uint64_t low{0};

    constexpr bool valid() const noexcept {
        return high != 0 || low != 0;
    }

    std::string to_string() const;
    static std::optional<AssetGuid> parse(std::string_view text);
    static AssetGuid generate();

    friend constexpr bool operator==(AssetGuid, AssetGuid) = default;
};

struct AssetGuidHash {
    std::size_t operator()(AssetGuid guid) const noexcept;
};

} // namespace nengine::assets
