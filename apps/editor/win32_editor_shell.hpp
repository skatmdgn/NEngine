#pragma once

#include <memory>

#include "nengine/editor/editor_model.hpp"

namespace nengine::app {

class Win32EditorShell {
public:
    explicit Win32EditorShell(nengine::editor::EditorModel& editor);
    ~Win32EditorShell();

    Win32EditorShell(const Win32EditorShell&) = delete;
    Win32EditorShell& operator=(const Win32EditorShell&) = delete;

    bool attach(void* native_window);
    void tick();
    void refresh();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nengine::app
