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


AssetGuid derive_subasset_guid(
    AssetGuid parent,
    std::string_view name_space,
    std::uint64_t local_key) noexcept {

    auto mix =
        [](std::uint64_t value) noexcept {

            value +=
                0x9e3779b97f4a7c15ull;
            value =
                (value ^
                 (value >> 30u)) *
                0xbf58476d1ce4e5b9ull;
            value =
                (value ^
                 (value >> 27u)) *
                0x94d049bb133111ebull;

            return
                value ^
                (value >> 31u);
        };

    std::uint64_t namespace_hash =
        14695981039346656037ull;

    for (const auto ch :
         name_space) {

        namespace_hash ^=
            static_cast<unsigned char>(
                ch);

        namespace_hash *=
            1099511628211ull;
    }

    AssetGuid derived{
        mix(
            parent.high ^
            namespace_hash ^
            (local_key *
             0xd6e8feb86659fd93ull)),
        mix(
            parent.low ^
            (namespace_hash <<
                1u) ^
            (local_key *
             0xa0761d6478bd642full))
    };

    if (!derived.valid()) {
        derived.low = 1u;
    }

    return derived;
}

} // namespace nengine::assets
