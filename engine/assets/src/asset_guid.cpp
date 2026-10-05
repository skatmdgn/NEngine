#include "nengine/assets/asset_guid.hpp"

#include <array>
#include <charconv>
#include <iomanip>
#include <random>
#include <sstream>

namespace nengine::assets {
namespace {

bool parse_u64(std::string_view text, std::uint64_t& value) {
    if (text.size() != 16) return false;
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value, 16);
    return result.ec == std::errc{} && result.ptr == end;
}

} // namespace

std::string AssetGuid::to_string() const {
    std::ostringstream stream;
    stream << std::hex << std::setfill('0')
           << std::setw(16) << high
           << std::setw(16) << low;
    return stream.str();
}

std::optional<AssetGuid> AssetGuid::parse(std::string_view text) {
    if (text.size() != 32) return std::nullopt;

    AssetGuid value{};
    if (!parse_u64(text.substr(0, 16), value.high) ||
        !parse_u64(text.substr(16, 16), value.low)) {
        return std::nullopt;
    }

    if (!value.valid()) return std::nullopt;
    return value;
}

AssetGuid AssetGuid::generate() {
    std::random_device source;
    std::seed_seq seed{
        source(), source(), source(), source(),
        source(), source(), source(), source()
    };
    std::mt19937_64 generator(seed);

    AssetGuid result{};
    do {
        result.high = generator();
        result.low = generator();
    } while (!result.valid());

    return result;
}

std::size_t AssetGuidHash::operator()(AssetGuid guid) const noexcept {
    const auto mixed = guid.high ^
        (guid.low + 0x9e3779b97f4a7c15ull +
         (guid.high << 6u) + (guid.high >> 2u));
    return static_cast<std::size_t>(mixed);
}

} // namespace nengine::assets
