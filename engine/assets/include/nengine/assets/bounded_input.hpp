#pragma once

#include <array>
#include <cstddef>
#include <istream>
#include <string>
#include <utility>

namespace nengine::assets {

// Read a bounded text record source before any formatted extraction (notably
// std::quoted) can allocate unbounded strings. Transactional output.
inline bool read_bounded_text(std::istream& source, std::string& result,
                              std::size_t limit) {
    std::string staged;
    std::array<char, 4096u> block{};
    while (true) {
        source.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto count = static_cast<std::size_t>(source.gcount());
        if (count > limit - staged.size()) return false;
        staged.append(block.data(), count);
        if (source.eof()) break;
        if (!source) return false;
    }
    result = std::move(staged);
    return true;
}

} // namespace nengine::assets
