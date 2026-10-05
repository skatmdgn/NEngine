#pragma once

#include <filesystem>
#include <string>

namespace nengine::editor {

struct EditorLayoutState {
    int hierarchy_width{270};
    int inspector_width{330};
    int bottom_height{150};

    void clamp(int window_width, int window_height) noexcept;
};

class EditorLayoutSerializer {
public:
    static bool save(
        const EditorLayoutState& layout,
        const std::filesystem::path& path,
        std::string* error = nullptr);

    static bool load(
        const std::filesystem::path& path,
        EditorLayoutState& layout,
        std::string* error = nullptr);
};

} // namespace nengine::editor
