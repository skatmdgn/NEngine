#pragma once
#include <cstddef>
#include <vector>
#include "nengine/ui/canvas.hpp"

namespace nengine::ui {

// A bounded, transactional in-memory authoring history. Save to an imported
// .ncanvas file is deliberately a separate editor/project operation.
class CanvasEditHistory {
public:
    explicit CanvasEditHistory(std::size_t max_undo = 32u) noexcept;
    const UiCanvas& canvas() const noexcept { return current_; }
    bool reset(UiCanvas canvas);
    bool commit(UiCanvas canvas);
    bool undo();
    bool redo();
    std::size_t undo_count() const noexcept { return undo_.size(); }
    std::size_t redo_count() const noexcept { return redo_.size(); }
private:
    std::size_t max_undo_{32u};
    UiCanvas current_{};
    std::vector<UiCanvas> undo_{};
    std::vector<UiCanvas> redo_{};
};
} // namespace nengine::ui
