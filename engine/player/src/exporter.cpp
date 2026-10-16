#include "nengine/player/exporter.hpp"
#include "nengine/player/bundle.hpp"
#include "nengine/core/scene.hpp"
#include <chrono>
#include <filesystem>
#include <string>
#include <utility>

namespace nengine::player {
namespace {
bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}
bool inside(const std::filesystem::path& child,
            const std::filesystem::path& root) {
    const auto rel = child.lexically_relative(root);
    return !rel.empty() && rel != "." && !rel.is_absolute() &&
           *rel.begin() != "..";
}
}

bool export_player_data(const ExportSettings& settings,
                        ExportReport& report, std::string* error) {
    std::error_code ec;
    if (settings.startup_scene.empty() ||
        settings.startup_scene.is_absolute() ||
        settings.startup_scene.has_root_name() ||
        settings.startup_scene.extension() != ".nscene")
        return fail(error, "startup scene must be a relative .nscene file");
    for (const auto& part : settings.startup_scene)
        if (part == ".." || part == "." || part.empty())
            return fail(error, "startup scene contains traversal");

    const auto root = std::filesystem::canonical(settings.project_root, ec);
    if (ec || !std::filesystem::is_directory(root))
        return fail(error, "project root does not exist");
    const auto source = std::filesystem::canonical(
        root / settings.startup_scene, ec);
    if (ec || !inside(source, root) ||
        !std::filesystem::is_regular_file(source))
        return fail(error, "project startup scene is absent or outside root");

    const auto destination = std::filesystem::absolute(
        settings.destination, ec).lexically_normal();
    if (ec || settings.destination.empty() || destination == root ||
        inside(destination, root))
        return fail(error, "export destination must be outside project");
    const auto parent = std::filesystem::canonical(
        destination.parent_path(), ec);
    if (ec || !std::filesystem::is_directory(parent) ||
        std::filesystem::exists(destination))
        return fail(error, "destination parent missing or destination exists");
    const auto final_path = parent / destination.filename();
    if (final_path == root || inside(final_path, root))
        return fail(error, "export destination must be outside project");

    BundleManifest manifest;
    manifest.title = settings.title;
    manifest.width = settings.width;
    manifest.height = settings.height;
    if (!validate_bundle(manifest, error)) return false;
    core::SceneData scene;
    if (!core::SceneSerializer::load_file(source, scene, error))
        return fail(error, "cannot export invalid startup Scene");

    const auto nonce = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto staging = final_path;
    staging += ".nengine-staging-" + nonce;
    if (!std::filesystem::create_directory(staging, ec) || ec)
        return fail(error, "failed to create player export staging directory");
    const auto cleanup = [&] {
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
    };
    std::filesystem::create_directory(staging / "Content", ec);
    if (ec) { cleanup(); return fail(error, "cannot create player Content directory"); }
    std::filesystem::copy_file(
        source, staging / "Content" / "Main.nscene",
        std::filesystem::copy_options::none, ec);
    if (ec) { cleanup(); return fail(error, "cannot copy player startup Scene"); }
    if (!save_bundle(manifest, staging / "NEngine.player", error)) {
        cleanup(); return false;
    }
    std::filesystem::rename(staging, final_path, ec);
    if (ec) { cleanup(); return fail(error, "cannot atomically finish player bundle"); }
    report = {final_path, scene.objects.size()};
    if (error) error->clear();
    return true;
}
} // namespace nengine::player
