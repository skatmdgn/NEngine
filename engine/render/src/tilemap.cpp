#include "nengine/render/tilemap.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

#include "nengine/assets/bounded_input.hpp"

namespace nengine::render {
namespace {
bool fail(std::string* error, const char* message) {
    if (error) *error = message;
    return false;
}
constexpr std::uint32_t max_edge = 1024;
constexpr std::size_t max_palette = 4096;
}
bool Tilemap::valid() const noexcept {
    if (width == 0 || height == 0 ||
        width > max_edge || height > max_edge ||
        !std::isfinite(cell_width) || !std::isfinite(cell_height) ||
        cell_width <= 0 || cell_height <= 0 ||
        palette.size() > max_palette ||
        cells.size() != static_cast<std::size_t>(width) * height) return false;
    for (auto id : palette) if (!id.valid()) return false;
    for (auto cell : cells) if (cell < -1 ||
        (cell >= 0 && static_cast<std::size_t>(cell) >= palette.size()))
        return false;
    return true;
}
bool Tilemap::resize(std::uint32_t w, std::uint32_t h) {
    if (w == 0 || h == 0 || w > max_edge || h > max_edge) return false;
    auto next = std::vector<std::int32_t>(static_cast<std::size_t>(w)*h, -1);
    for (std::uint32_t y=0; y<std::min(h,height); ++y)
        for (std::uint32_t x=0; x<std::min(w,width); ++x)
            if (cells.size() == static_cast<std::size_t>(width)*height)
                next[static_cast<std::size_t>(y)*w+x] =
                    cells[static_cast<std::size_t>(y)*width+x];
    width=w; height=h; cells=std::move(next);
    return true;
}
bool Tilemap::set(std::uint32_t x, std::uint32_t y, std::int32_t index) {
    if (x>=width || y>=height || cells.size()!=static_cast<std::size_t>(width)*height ||
        index < -1 || (index>=0 && static_cast<std::size_t>(index)>=palette.size()))
        return false;
    cells[static_cast<std::size_t>(y)*width+x]=index;
    return true;
}
std::int32_t Tilemap::get(std::uint32_t x, std::uint32_t y) const noexcept {
    return x<width && y<height && cells.size()==static_cast<std::size_t>(width)*height
        ? cells[static_cast<std::size_t>(y)*width+x] : -1;
}
bool write_tilemap(std::ostream& out, const Tilemap& map, std::string* error) {
    if (!map.valid()) return fail(error,"invalid tilemap");
    std::ostringstream staged;
    staged << std::setprecision(std::numeric_limits<float>::max_digits10);
    staged << "NENGINE_TILEMAP 1\nGRID " << map.width << ' ' << map.height
           << ' ' << map.cell_width << ' ' << map.cell_height << "\n";
    staged << "PALETTE " << map.palette.size() << "\n";
    for (auto id : map.palette)
        staged << "TILE " << id.to_string() << "\n";
    staged << "CELLS " << map.cells.size() << "\n";
    for (auto cell : map.cells) staged << cell << '\n';
    staged << "END_TILEMAP\n";
    auto text=staged.str();
    if (text.size() > 16u*1024u*1024u) return fail(error,"tilemap exceeds 16 MiB");
    out << text;
    if (!out.good()) return fail(error,"tilemap output stream failed");
    if (error) error->clear();
    return true;
}
bool read_tilemap(std::istream& in, Tilemap& output, std::string* error) {
    std::string text;
    if (!assets::read_bounded_text(in,text,16u*1024u*1024u))
        return fail(error,"tilemap exceeds 16 MiB or read failed");
    std::istringstream input{text};
    std::string token; unsigned version=0;
    Tilemap staged;
    if (!(input>>token>>version) || token!="NENGINE_TILEMAP" || version!=1 ||
        !(input>>token) || token!="GRID" ||
        !(input>>staged.width>>staged.height>>staged.cell_width>>staged.cell_height) ||
        staged.width==0 || staged.height==0 || staged.width>max_edge ||
        staged.height>max_edge || !std::isfinite(staged.cell_width) ||
        !std::isfinite(staged.cell_height) || staged.cell_width<=0 ||
        staged.cell_height<=0)
        return fail(error,"invalid tilemap header/grid");
    std::size_t palette_count=0;
    if (!(input>>token>>palette_count) || token!="PALETTE" ||
        palette_count>max_palette) return fail(error,"invalid palette count");
    std::unordered_set<std::string> unique;
    for (std::size_t i=0;i<palette_count;++i) {
        std::string guid;
        if (!(input>>token>>guid) || token!="TILE")
            return fail(error,"truncated palette");
        auto parsed=assets::AssetGuid::parse(guid);
        if (!parsed || !parsed->valid() || !unique.insert(guid).second)
            return fail(error,"invalid or duplicate tile GUID");
        staged.palette.push_back(*parsed);
    }
    std::size_t cell_count=0;
    if (!(input>>token>>cell_count) || token!="CELLS" ||
        cell_count!=static_cast<std::size_t>(staged.width)*staged.height)
        return fail(error,"wrong cell count");
    staged.cells.reserve(cell_count);
    for(std::size_t i=0;i<cell_count;++i) {
        std::int32_t index;
        if (!(input>>index)) return fail(error,"truncated cells");
        staged.cells.push_back(index);
    }
    if (!(input>>token) || token!="END_TILEMAP" || (input>>token) || !staged.valid())
        return fail(error,"invalid cell index or trailing tilemap data");
    output=std::move(staged);
    if(error) error->clear();
    return true;
}
} // namespace nengine::render
