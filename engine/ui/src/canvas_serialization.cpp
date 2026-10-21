#include "nengine/ui/canvas_serialization.hpp"

#include <array>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>

namespace nengine::ui {
namespace {
bool finite_widget(const Widget& w) noexcept {
    const auto& a = w.anchors;
    const auto& c = w.color;
    return std::isfinite(a.anchor_min.x) && std::isfinite(a.anchor_min.y) &&
        std::isfinite(a.anchor_max.x) && std::isfinite(a.anchor_max.y) &&
        std::isfinite(a.offset_min.x) && std::isfinite(a.offset_min.y) &&
        std::isfinite(a.offset_max.x) && std::isfinite(a.offset_max.y) &&
        std::isfinite(c.r) && std::isfinite(c.g) &&
        std::isfinite(c.b) && std::isfinite(c.a);
}
bool fail(std::string* error, const char* reason) {
    if (error) *error = reason;
    return false;
}
} // namespace

bool write_canvas(
    std::ostream& output, const UiCanvas& canvas, std::string* error) {

    std::ostringstream staged;
    staged << std::setprecision(std::numeric_limits<float>::max_digits10);
    staged << "NENGINE_CANVAS 3\nWIDGETS " << canvas.widgets().size() << '\n';
    std::unordered_set<WidgetId> written;
    std::unordered_set<std::string> names;
    for (const auto& node : canvas.widgets()) {
        if (node.id == invalid_widget || node.id == UINT64_MAX ||
            !written.insert(node.id).second ||
            (node.parent != invalid_widget && !written.contains(node.parent)) ||
            static_cast<unsigned>(node.kind) >
                static_cast<unsigned>(WidgetKind::Button) ||
            !finite_widget(node) ||
            !valid_widget_name(node.name) ||
            node.text.size() > 4096u ||
            (node.kind != WidgetKind::Image && node.image_texture.valid()) ||
            (!node.name.empty() && !names.insert(node.name).second))
            return fail(error, "cannot serialize invalid Canvas hierarchy");

        const auto& a = node.anchors;
        const auto& c = node.color;
        staged << "WIDGET " << node.id << ' ' << node.parent << ' '
               << static_cast<unsigned>(node.kind) << ' '
               << (node.visible ? 1 : 0) << ' '
               << (node.interactable ? 1 : 0) << ' '
               << (node.clip_children ? 1 : 0) << ' '
               << std::quoted(node.text) << ' '
               << std::quoted(node.name) << ' '
               << std::quoted(node.image_texture.valid()
                    ? node.image_texture.to_string() : std::string{}) << ' '
               << a.anchor_min.x << ' ' << a.anchor_min.y << ' '
               << a.anchor_max.x << ' ' << a.anchor_max.y << ' '
               << a.offset_min.x << ' ' << a.offset_min.y << ' '
               << a.offset_max.x << ' ' << a.offset_max.y << ' '
               << c.r << ' ' << c.g << ' ' << c.b << ' ' << c.a << '\n';
    }
    staged << "END_CANVAS\n";
    output << staged.str();
    if (!output.good()) return fail(error, "could not write Canvas");
    if (error) error->clear();
    return true;
}

bool read_canvas(
    std::istream& input, UiCanvas& canvas, std::string* error) {

    std::string tag;
    unsigned version = 0u;
    std::size_t count = 0u;
    if (!(input >> tag >> version) || tag != "NENGINE_CANVAS" ||
        (version != 1u && version != 2u && version != 3u) ||
        !(input >> tag >> count) ||
        tag != "WIDGETS" || count > 8192u)
        return fail(error, "invalid Canvas header or widget count");

    // A widget record must fit on a single reasonably sized line;
    // bounded reads prevent hostile .ncanvas sources from allocating an
    // unbounded quoted string before validation.
    std::array<char, 256> header_tail{};
    if (!input.getline(header_tail.data(),
            static_cast<std::streamsize>(header_tail.size())))
        return fail(error, "Canvas header line too large or truncated");
    if (std::string_view(header_tail.data()).find_first_not_of(" \t\r") != std::string_view::npos)
        return fail(error, "invalid Canvas header trailing data");

    UiCanvas candidate;
    for (std::size_t n = 0u; n < count; ++n) {
        std::array<char, 16384> record{};
        if (!input.getline(
                record.data(), static_cast<std::streamsize>(record.size())))
            return fail(error, "Canvas widget record too large or truncated");

        std::istringstream fields(record.data());
        Widget node;
        unsigned kind = 0u;
        int visible = -1, interactable = -1, clip = -1;
        auto& a = node.anchors;
        auto& c = node.color;
        if (!(fields >> tag) || tag != "WIDGET" ||
            !(fields >> node.id >> node.parent >> kind >>
                visible >> interactable >> clip >>
                std::quoted(node.text)))
            return fail(error, "malformed Canvas WIDGET record");
        if (version >= 2u && !(fields >> std::quoted(node.name)))
            return fail(error, "malformed Canvas widget name");
        if (version >= 3u) {
            std::string image_guid;
            if (!(fields >> std::quoted(image_guid)) ||
                (kind != static_cast<unsigned>(WidgetKind::Image) &&
                 !image_guid.empty()))
                return fail(error, "invalid Canvas image texture field");
            if (!image_guid.empty()) {
                const auto parsed = assets::AssetGuid::parse(image_guid);
                if (!parsed || !parsed->valid())
                    return fail(error, "invalid Canvas image GUID");
                node.image_texture = *parsed;
            }
        }
        if (!(fields >> a.anchor_min.x >> a.anchor_min.y >>
                a.anchor_max.x >> a.anchor_max.y >>
                a.offset_min.x >> a.offset_min.y >>
                a.offset_max.x >> a.offset_max.y >>
                c.r >> c.g >> c.b >> c.a) ||
            kind > static_cast<unsigned>(WidgetKind::Button) ||
            (visible != 0 && visible != 1) ||
            (interactable != 0 && interactable != 1) ||
            (clip != 0 && clip != 1))
            return fail(error, "malformed Canvas WIDGET record");

        std::string trailing;
        if (fields >> trailing)
            return fail(error, "Canvas widget contains trailing data");

        node.kind = static_cast<WidgetKind>(kind);
        node.visible = visible != 0;
        node.interactable = interactable != 0;
        node.clip_children = clip != 0;
        if (!finite_widget(node) || !candidate.restore(std::move(node)))
            return fail(error, "invalid, duplicate or orphan Canvas widget");
    }

    if (!(input >> tag) || tag != "END_CANVAS")
        return fail(error, "Canvas terminator missing");
    if (input >> tag)
        return fail(error, "trailing Canvas data");

    canvas = std::move(candidate);
    if (error) error->clear();
    return true;
}

} // namespace nengine::ui
