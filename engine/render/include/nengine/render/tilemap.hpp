#pragma once
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "nengine/assets/asset_guid.hpp"

namespace nengine::render {

// Persistent tile grid, independent from an Editor or a particular GPU backend.
// A cell is -1 (empty), otherwise an index into a GUID palette.
struct Tilemap {
    std::uint32_t width{0};
    std::uint32_t height{0};
    float cell_width{1.0f};
    float cell_height{1.0f};
    std::vector<assets::AssetGuid> palette{};
    std::vector<std::int32_t> cells{};

    bool valid() const noexcept;
    bool resize(std::uint32_t w, std::uint32_t h);
    bool set(std::uint32_t x, std::uint32_t y, std::int32_t palette_index);
    std::int32_t get(std::uint32_t x, std::uint32_t y) const noexcept;
};

// Deterministic, versioned .ntilemap text; bounded to 1024x1024 cells,
// 4096 palette entries and 16 MiB serialized input.
// Load is transactional: invalid input never mutates an existing grid.
bool write_tilemap(std::ostream& stream, const Tilemap& map,
                   std::string* error = nullptr);
bool read_tilemap(std::istream& stream, Tilemap& map,
                  std::string* error = nullptr);

} // namespace nengine::render
