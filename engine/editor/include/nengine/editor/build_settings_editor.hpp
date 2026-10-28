#pragma once
#include <filesystem>
#include <string>
#include <string_view>
#include "nengine/editor/project_session.hpp"
#include "nengine/scripting/managed_project.hpp"
#include <vector>
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

// EDTOOL-009: save machine-local tool paths outside the portable .nproject.
// A successful validation is not Android cross-compilation or device proof.
struct AndroidToolPaths {
    std::filesystem::path sdk{},ndk{},jdk{};
};
bool validate_android_tool_paths(const AndroidToolPaths& paths,
                                 std::string* error=nullptr);
bool save_android_tool_paths(const ProjectSession& project,
                             const AndroidToolPaths& paths,
                             std::string* error=nullptr);
bool load_android_tool_paths(const ProjectSession& project,
                             AndroidToolPaths& paths,
                             std::string* error=nullptr);

using ManagedPackageRows=std::vector<nengine::scripting::NuGetPackageReference>;
// EDTOOL-005: source-backed NuGet manifest edit operations used by Editor UI.
bool load_managed_packages(const ProjectSession& project,ManagedPackageRows& rows,
                           std::string* error=nullptr);
bool edit_managed_package(ProjectSession& project,std::string id,
                          std::string version,std::string* error=nullptr);
}
