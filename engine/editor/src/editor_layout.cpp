#include "nengine/editor/editor_layout.hpp"

#include <algorithm>
#include <fstream>
#include <utility>

namespace nengine::editor {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

} // namespace

void EditorLayoutState::clamp(
    int window_width,
    int window_height) noexcept {

    const int usable_width =
        std::max(700, window_width);

    const int usable_height =
        std::max(500, window_height);

    hierarchy_width =
        std::clamp(
            hierarchy_width,
            180,
            std::max(
                180,
                usable_width / 2 - 80));

    inspector_width =
        std::clamp(
            inspector_width,
            260,
            std::max(
                260,
                usable_width / 2 - 80));

    bottom_height =
        std::clamp(
            bottom_height,
            100,
            std::max(
                100,
                usable_height / 2));
}

bool EditorLayoutSerializer::save(
    const EditorLayoutState& layout,
    const std::filesystem::path& path,
    std::string* error) {

    std::error_code ec;
    std::filesystem::create_directories(
        path.parent_path(),
        ec);

    if (ec) {
        set_error(
            error,
            "could not create layout directory");
        return false;
    }

    std::ofstream output(
        path,
        std::ios::binary | std::ios::trunc);

    if (!output) {
        set_error(
            error,
            "could not open editor layout for writing");
        return false;
    }

    output << "NENGINE_EDITOR_LAYOUT 1\n";
    output << "HIERARCHY_WIDTH "
           << layout.hierarchy_width
           << "\n";
    output << "INSPECTOR_WIDTH "
           << layout.inspector_width
           << "\n";
    output << "BOTTOM_HEIGHT "
           << layout.bottom_height
           << "\n";
    output << "END_LAYOUT\n";

    if (!output.good()) {
        set_error(
            error,
            "failed while writing editor layout");
        return false;
    }

    return true;
}

bool EditorLayoutSerializer::load(
    const std::filesystem::path& path,
    EditorLayoutState& layout,
    std::string* error) {

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open editor layout");
        return false;
    }

    std::string token;
    int version = 0;
    EditorLayoutState parsed;

    if (!(input >> token >> version) ||
        token != "NENGINE_EDITOR_LAYOUT" ||
        version != 1) {

        set_error(
            error,
            "invalid editor layout header");
        return false;
    }

    if (!(input >> token >> parsed.hierarchy_width) ||
        token != "HIERARCHY_WIDTH") {
        set_error(error, "missing hierarchy width");
        return false;
    }

    if (!(input >> token >> parsed.inspector_width) ||
        token != "INSPECTOR_WIDTH") {
        set_error(error, "missing inspector width");
        return false;
    }

    if (!(input >> token >> parsed.bottom_height) ||
        token != "BOTTOM_HEIGHT") {
        set_error(error, "missing bottom height");
        return false;
    }

    if (!(input >> token) ||
        token != "END_LAYOUT") {
        set_error(error, "missing layout terminator");
        return false;
    }

    parsed.clamp(1440, 900);
    layout = parsed;
    return true;
}

} // namespace nengine::editor
