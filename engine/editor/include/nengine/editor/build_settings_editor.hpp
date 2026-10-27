#pragma once
#include <string>
#include <string_view>
#include "nengine/editor/project_session.hpp"
namespace nengine::editor {
struct BuildSettingsDraft {
    std::string project_name{};
    bool windows{true};
    bool android{true};
    std::string android_toolchain{"default"};
};
// EDTOOL-008: one transactional Editor-side API used by Windows/Android
// Build Settings controls. This configures targets; it does not build APKs.
bool apply_build_settings(ProjectSession& project,
                          const BuildSettingsDraft& draft,
                          std::string* error=nullptr);
BuildSettingsDraft read_build_settings(const ProjectSession& project);
}
