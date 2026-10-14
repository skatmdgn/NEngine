#include "nengine/ui/canvas.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <utility>
#include <unordered_set>

namespace nengine::ui {
namespace {

bool finite_rect(const UiRect& r) noexcept {
    return std::isfinite(r.x) && std::isfinite(r.y) &&
        std::isfinite(r.width) && std::isfinite(r.height);
}

bool finite_anchors(const RectAnchors& a) noexcept {
    return std::isfinite(a.anchor_min.x) &&
        std::isfinite(a.anchor_min.y) &&
        std::isfinite(a.anchor_max.x) &&
        std::isfinite(a.anchor_max.y) &&
        std::isfinite(a.offset_min.x) &&
        std::isfinite(a.offset_min.y) &&
        std::isfinite(a.offset_max.x) &&
        std::isfinite(a.offset_max.y);
}

UiRect resolve_rect(const UiRect& parent, const RectAnchors& a) noexcept {
    const float left = parent.x + parent.width * a.anchor_min.x +
        a.offset_min.x;
    const float top = parent.y + parent.height * a.anchor_min.y +
        a.offset_min.y;
    const float right = parent.x + parent.width * a.anchor_max.x +
        a.offset_max.x;
    const float bottom = parent.y + parent.height * a.anchor_max.y +
        a.offset_max.y;
    return {left, top, right - left, bottom - top};
}

} // namespace

bool UiRect::contains(core::Vec2 point) const noexcept {
    return finite_rect(*this) && width > 0.0f && height > 0.0f &&
        std::isfinite(point.x) && std::isfinite(point.y) &&
        point.x >= x && point.x < x + width &&
        point.y >= y && point.y < y + height;
}

UiRect intersect_rect(UiRect a, UiRect b) noexcept {
    if (!finite_rect(a) || !finite_rect(b) ||
        a.width <= 0.0f || a.height <= 0.0f ||
        b.width <= 0.0f || b.height <= 0.0f)
        return {};
    const float left = std::max(a.x, b.x);
    const float top = std::max(a.y, b.y);
    const float right = std::min(a.x + a.width, b.x + b.width);
    const float bottom = std::min(a.y + a.height, b.y + b.height);
    return {left, top, std::max(0.0f, right - left),
            std::max(0.0f, bottom - top)};
}

WidgetId UiCanvas::add(WidgetKind kind, WidgetId parent) {
    if (parent != invalid_widget && !find(parent)) return invalid_widget;
    if (next_id_ == invalid_widget) return invalid_widget;
    const auto id = next_id_++;
    Widget widget{id, parent, kind};
    // Text has no backing geometry until glyph rendering is implemented.
    if (kind == WidgetKind::Text) widget.color = {1, 1, 1, 0};
    if (kind == WidgetKind::Panel) widget.color = {0.18f, 0.20f, 0.24f, 1};
    if (kind == WidgetKind::Button) widget.color = {0.30f, 0.38f, 0.48f, 1};
    widgets_.push_back(std::move(widget));
    return id;
}

bool UiCanvas::restore(Widget widget) {
    if (widget.id == invalid_widget ||
        widget.id == UINT64_MAX || find(widget.id) ||
        (widget.parent != invalid_widget && !find(widget.parent)))
        return false;
    next_id_ = std::max(next_id_, widget.id + 1u);
    widgets_.push_back(std::move(widget));
    return true;
}

Widget* UiCanvas::find(WidgetId id) noexcept {
    for (auto& widget : widgets_)
        if (widget.id == id && id != invalid_widget) return &widget;
    return nullptr;
}

const Widget* UiCanvas::find(WidgetId id) const noexcept {
    for (const auto& widget : widgets_)
        if (widget.id == id && id != invalid_widget) return &widget;
    return nullptr;
}

bool UiCanvas::reparent(WidgetId id, WidgetId new_parent) {
    const auto* source = find(id);
    if (!source || id == new_parent ||
        (new_parent != invalid_widget && !find(new_parent)))
        return false;

    // Collect descendants even when the current paint order interleaves
    // separate subtrees. Serialization requires every parent before child.
    std::unordered_set<WidgetId> moving{id};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& node : widgets_)
            if (moving.contains(node.parent) &&
                moving.insert(node.id).second)
                changed = true;
    }
    if (new_parent != invalid_widget && moving.contains(new_parent))
        return false;
    if (source->parent == new_parent) return true;

    std::vector<Widget> subtree, remaining;
    subtree.reserve(moving.size());
    remaining.reserve(widgets_.size() - moving.size());
    for (const auto& node : widgets_)
        (moving.contains(node.id) ? subtree : remaining).push_back(node);
    // The root always precedes its descendants.
    subtree.front().parent = new_parent;

    std::size_t at = remaining.size();
    if (new_parent != invalid_widget) {
        std::unordered_set<WidgetId> descendants{new_parent};
        for (std::size_t i = 0; i < remaining.size(); ++i)
            if (descendants.contains(remaining[i].id) ||
                descendants.contains(remaining[i].parent)) {
                descendants.insert(remaining[i].id);
                at = i + 1u;
            }
    }
    remaining.insert(
        remaining.begin() + static_cast<std::ptrdiff_t>(at),
        subtree.begin(), subtree.end());
    widgets_ = std::move(remaining);
    return true;
}

bool UiCanvas::set_sibling_index(WidgetId id, std::size_t index) {
    const auto* source = find(id);
    if (!source) return false;
    const WidgetId parent = source->parent;
    std::vector<WidgetId> siblings;
    for (const auto& node : widgets_)
        if (node.parent == parent && node.id != id)
            siblings.push_back(node.id);

    // Keep all descendants together, even for older Canvases whose
    // source order interleaves descendant blocks.
    std::unordered_set<WidgetId> moving{id};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& node : widgets_)
            if (moving.contains(node.parent) &&
                moving.insert(node.id).second)
                changed = true;
    }
    std::vector<Widget> subtree, remaining;
    subtree.reserve(moving.size());
    remaining.reserve(widgets_.size() - moving.size());
    for (const auto& node : widgets_)
        (moving.contains(node.id) ? subtree : remaining).push_back(node);

    std::size_t at = remaining.size();
    if (index < siblings.size()) {
        const auto target = siblings[index];
        const auto it = std::find_if(
            remaining.begin(), remaining.end(),
            [target](const Widget& node) { return node.id == target; });
        if (it == remaining.end()) return false;
        at = static_cast<std::size_t>(it - remaining.begin());
    } else if (parent != invalid_widget) {
        std::unordered_set<WidgetId> descendants{parent};
        for (std::size_t i = 0; i < remaining.size(); ++i)
            if (descendants.contains(remaining[i].id) ||
                descendants.contains(remaining[i].parent)) {
                descendants.insert(remaining[i].id);
                at = i + 1u;
            }
    }
    remaining.insert(
        remaining.begin() + static_cast<std::ptrdiff_t>(at),
        subtree.begin(), subtree.end());
    widgets_ = std::move(remaining);
    return true;
}

bool UiCanvas::remove(WidgetId id) {
    if (!find(id)) return false;
    std::unordered_set<WidgetId> removed{id};
    bool changed = true;
    while (changed) {
        changed = false;
        for (const auto& node : widgets_)
            if (!removed.contains(node.id) && removed.contains(node.parent)) {
                removed.insert(node.id);
                changed = true;
            }
    }
    std::erase_if(widgets_, [&](const Widget& node) {
        return removed.contains(node.id);
    });
    return true;
}

void UiCanvas::clear() noexcept {
    widgets_.clear();
    // IDs are never recycled, even if callers retain an old widget ID.
}

UiLayout UiCanvas::layout(float width, float height) const {
    UiLayout result;
    if (!std::isfinite(width) || !std::isfinite(height) ||
        width <= 0.0f || height <= 0.0f) return result;

    struct Resolved {
        UiRect rect{};
        UiRect clipping{};
        bool visible{false};
    };
    std::unordered_map<WidgetId, Resolved> resolved;
    resolved.reserve(widgets_.size());

    const UiRect viewport{0.0f, 0.0f, width, height};
    for (const auto& node : widgets_) {
        Resolved parent{viewport, viewport, true};
        if (node.parent != invalid_widget) {
            const auto it = resolved.find(node.parent);
            if (it == resolved.end()) continue;
            parent = it->second;
        }
        if (!parent.visible || !node.visible ||
            !finite_anchors(node.anchors)) {
            resolved.emplace(node.id, Resolved{});
            continue;
        }
        const auto rect = resolve_rect(parent.rect, node.anchors);
        if (!finite_rect(rect) || rect.width <= 0.0f ||
            rect.height <= 0.0f) {
            resolved.emplace(node.id, Resolved{});
            continue;
        }
        const auto clipped = intersect_rect(rect, parent.clipping);
        result.items.push_back({
            node.id, node.kind, rect, clipped, node.text,
            node.kind == WidgetKind::Button && node.interactable,
            node.color
        });
        // When clip_children=false, descendants inherit the parent's
        // effective clipping from ancestors, not the parent's own bounds.
        resolved.emplace(node.id, Resolved{
            rect,
            node.clip_children ? clipped : parent.clipping,
            true
        });
    }
    return result;
}

} // namespace nengine::ui
