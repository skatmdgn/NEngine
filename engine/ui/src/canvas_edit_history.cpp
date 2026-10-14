#include "nengine/ui/canvas_edit_history.hpp"
#include "nengine/ui/canvas_serialization.hpp"
#include <algorithm>
#include <sstream>
#include <utility>

namespace nengine::ui {
namespace {
bool serialize(const UiCanvas& canvas, std::string& result) {
    std::ostringstream output;
    if (!write_canvas(output, canvas)) return false;
    result = output.str();
    return true;
}
}

CanvasEditHistory::CanvasEditHistory(std::size_t max_undo) noexcept
    : max_undo_(std::min(max_undo, std::size_t{128u})) {}

bool CanvasEditHistory::reset(UiCanvas canvas) {
    std::string serialized;
    if (!serialize(canvas, serialized)) return false;
    current_ = std::move(canvas);
    undo_.clear();
    redo_.clear();
    return true;
}

bool CanvasEditHistory::commit(UiCanvas canvas) {
    std::string before, after;
    if (!serialize(current_, before) ||
        !serialize(canvas, after) || before == after) return false;
    if (max_undo_ != 0u) {
        if (undo_.size() == max_undo_) undo_.erase(undo_.begin());
        undo_.push_back(current_);
    }
    current_ = std::move(canvas);
    redo_.clear();
    return true;
}

bool CanvasEditHistory::undo() {
    if (undo_.empty()) return false;
    redo_.push_back(std::move(current_));
    current_ = std::move(undo_.back());
    undo_.pop_back();
    return true;
}

bool CanvasEditHistory::redo() {
    if (redo_.empty()) return false;
    if (max_undo_ != 0u) {
        if (undo_.size() == max_undo_) undo_.erase(undo_.begin());
        undo_.push_back(std::move(current_));
    }
    current_ = std::move(redo_.back());
    redo_.pop_back();
    return true;
}

} // namespace nengine::ui
