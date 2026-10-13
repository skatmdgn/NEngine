#include "nengine/ui/canvas_serialization.hpp"

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
    staged << "NENGINE_CANVAS 1\nWIDGETS " << canvas.widgets().size() << '\n';
    std::unordered_set<WidgetId> written;
    for (const auto& node : canvas.widgets()) {
        if (node.id == invalid_widget || node.id == UINT64_MAX ||
            !written.insert(node.id).second ||
            (node.parent != invalid_widget && !written.contains(node.parent)) ||
            static_cast<unsigned>(node.kind) >
                static_cast<unsigned>(WidgetKind::Button) ||
            !finite_widget(node))
            return fail(error, "cannot serialize invalid Canvas hierarchy");

        const auto& a = node.anchors;
        const auto& c = node.color;
        staged << "WIDGET " << node.id << ' ' << node.parent << ' '
               << static_cast<unsigned>(node.kind) << ' '
               << (node.visible ? 1 : 0) << ' '
               << (node.interactable ? 1 : 0) << ' '
               << (node.clip_children ? 1 : 0) << ' '
               << std::quoted(node.text) << ' '
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
        version != 1u || !(input >> tag >> count) ||
        tag != "WIDGETS" || count > 8192u)
        return fail(error, "invalid Canvas header or widget count");

    UiCanvas candidate;
    for (std::size_t n = 0u; n < count; ++n) {
        Widget node;
        unsigned kind = 0u;
        int visible = -1, interactable = -1, clip = -1;
        auto& a = node.anchors;
        auto& c = node.color;
        if (!(input >> tag) || tag != "WIDGET" ||
            !(input >> node.id >> node.parent >> kind >>
                visible >> interactable >> clip >>
                std::quoted(node.text) >>
                a.anchor_min.x >> a.anchor_min.y >>
                a.anchor_max.x >> a.anchor_max.y >>
                a.offset_min.x >> a.offset_min.y >>
                a.offset_max.x >> a.offset_max.y >>
                c.r >> c.g >> c.b >> c.a) ||
            kind > static_cast<unsigned>(WidgetKind::Button) ||
            (visible != 0 && visible != 1) ||
            (interactable != 0 && interactable != 1) ||
            (clip != 0 && clip != 1))
            return fail(error, "malformed Canvas WIDGET record");

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
