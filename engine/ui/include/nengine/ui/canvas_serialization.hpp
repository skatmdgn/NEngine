#pragma once

#include <istream>
#include <ostream>
#include <string>

#include "nengine/ui/canvas.hpp"

namespace nengine::ui {

// Versioned .ncanvas UI widget tree format. Read is transactional and
// preserves stable WidgetId values, hierarchy order, and paint order.
bool write_canvas(
    std::ostream& output, const UiCanvas& canvas,
    std::string* error = nullptr);
bool read_canvas(
    std::istream& input, UiCanvas& canvas,
    std::string* error = nullptr);

} // namespace nengine::ui
