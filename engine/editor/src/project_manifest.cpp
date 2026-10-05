#include "nengine/editor/project_manifest.hpp"

#include <fstream>
#include <iomanip>
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

bool ProjectManifestSerializer::validate(
    const ProjectManifest& manifest,
    std::string* error) {

    if (manifest.name.empty()) {
        set_error(
            error,
            "project name cannot be empty");
        return false;
    }

    if (manifest.startup_scene.empty() ||
        manifest.startup_scene.is_absolute()) {
        set_error(
            error,
            "startup scene must be a project-relative path");
        return false;
    }

    for (const auto& part :
         manifest.startup_scene) {
        if (part == "..") {
            set_error(
                error,
                "startup scene cannot escape the project root");
            return false;
        }
    }

    if (manifest.startup_scene.extension() !=
        ".nscene") {
        set_error(
            error,
            "startup scene must use the .nscene extension");
        return false;
    }

    if (manifest.scripting.empty()) {
        set_error(
            error,
            "scripting mode cannot be empty");
        return false;
    }

    if (!manifest.target_windows &&
        !manifest.target_android) {
        set_error(
            error,
            "project must enable at least one build target");
        return false;
    }

    if (manifest.target_android &&
        manifest.android_toolchain.empty()) {
        set_error(
            error,
            "Android target requires a toolchain profile");
        return false;
    }

    return true;
}

bool ProjectManifestSerializer::save(
    const ProjectManifest& manifest,
    const std::filesystem::path& path,
    std::string* error) {

    if (!validate(
            manifest,
            error)) {
        return false;
    }

    std::ofstream output(
        path,
        std::ios::binary | std::ios::trunc);

    if (!output) {
        set_error(
            error,
            "could not open project manifest for writing");
        return false;
    }

    output
        << "NENGINE_PROJECT "
        << manifest.version
        << '\n';

    output
        << "NAME "
        << std::quoted(manifest.name)
        << '\n';

    output
        << "STARTUP_SCENE "
        << std::quoted(
            manifest.startup_scene.generic_string())
        << '\n';

    output
        << "SCRIPTING "
        << std::quoted(manifest.scripting)
        << '\n';

    output
        << "TARGET_WINDOWS "
        << (manifest.target_windows ? 1 : 0)
        << '\n';

    output
        << "TARGET_ANDROID "
        << (manifest.target_android ? 1 : 0)
        << '\n';

    output
        << "ANDROID_TOOLCHAIN "
        << std::quoted(
            manifest.android_toolchain)
        << '\n';

    output << "END_PROJECT\n";

    if (!output.good()) {
        set_error(
            error,
            "failed while writing project manifest");
        return false;
    }

    return true;
}

bool ProjectManifestSerializer::load(
    const std::filesystem::path& path,
    ProjectManifest& manifest,
    std::string* error) {

    std::ifstream input(
        path,
        std::ios::binary);

    if (!input) {
        set_error(
            error,
            "could not open project manifest");
        return false;
    }

    ProjectManifest parsed;
    std::string token;

    if (!(input >> token) ||
        token != "NENGINE_PROJECT" ||
        !(input >> parsed.version)) {

        set_error(
            error,
            "invalid project manifest header");
        return false;
    }

    if (parsed.version >
        ProjectManifest::current_version) {

        set_error(
            error,
            "project manifest is newer than this editor");
        return false;
    }

    if (!(input >> token) ||
        token != "NAME" ||
        !(input >> std::quoted(parsed.name))) {

        set_error(error, "missing project name");
        return false;
    }

    std::string startup_scene;
    if (!(input >> token) ||
        token != "STARTUP_SCENE" ||
        !(input >> std::quoted(startup_scene))) {

        set_error(error, "missing startup scene");
        return false;
    }
    parsed.startup_scene =
        std::filesystem::path{
            std::move(startup_scene)
        };

    if (!(input >> token) ||
        token != "SCRIPTING" ||
        !(input >> std::quoted(parsed.scripting))) {

        set_error(error, "missing scripting mode");
        return false;
    }

    int target_windows = 0;
    if (!(input >> token) ||
        token != "TARGET_WINDOWS" ||
        !(input >> target_windows)) {

        set_error(error, "missing Windows target flag");
        return false;
    }
    parsed.target_windows =
        target_windows != 0;

    int target_android = 0;
    if (!(input >> token) ||
        token != "TARGET_ANDROID" ||
        !(input >> target_android)) {

        set_error(error, "missing Android target flag");
        return false;
    }
    parsed.target_android =
        target_android != 0;

    if (!(input >> token) ||
        token != "ANDROID_TOOLCHAIN" ||
        !(input >> std::quoted(
            parsed.android_toolchain))) {

        set_error(
            error,
            "missing Android toolchain profile");
        return false;
    }

    if (!(input >> token) ||
        token != "END_PROJECT") {

        set_error(
            error,
            "missing project manifest terminator");
        return false;
    }

    if (!validate(
            parsed,
            error)) {
        return false;
    }

    manifest = std::move(parsed);
    return true;
}

} // namespace nengine::editor
