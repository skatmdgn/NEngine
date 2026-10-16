#include "nengine/player/exporter.hpp"
#include "nengine/player/bundle.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/assets/asset_database.hpp"
#include "nengine/assets/builtin_processors.hpp"
#include "nengine/assets/asset_importer.hpp"
#include <chrono>
#include <array>
#include <algorithm>
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
    std::size_t copied_assets = 0;
    if (settings.copy_supported_assets) {
        const auto assets_root = root / "Assets";
        if (std::filesystem::is_symlink(assets_root, ec) || ec) {
            cleanup();
            return fail(error, "project Assets directory must not be a symlink");
        }
        if (std::filesystem::exists(assets_root)) {
            const auto asset_target = staging / "Content" / "Assets";
            std::filesystem::create_directory(asset_target, ec);
            if (ec) {
                cleanup();
                return fail(error, "cannot stage player Assets directory");
            }
            constexpr std::uintmax_t max_total = 1024ull * 1024 * 1024;
            constexpr std::uintmax_t max_file = 512ull * 1024 * 1024;
            std::uintmax_t total = 0;
            const std::array<std::string_view, 22> extensions{
                ".meta", ".nscene", ".ncanvas", ".nanim", ".nspriteanim", ".nanimctrl",
                ".nmat", ".glb", ".gltf", ".bin", ".png",
                ".jpg", ".jpeg", ".bmp", ".tga", ".webp",
                ".wav", ".ogg", ".mp3", ".flac", ".obj", ".mtl"};
            for (std::filesystem::recursive_directory_iterator it(
                     assets_root, ec), end; it != end && !ec;
                     it.increment(ec)) {
                if (it->is_symlink(ec) || ec) {
                    cleanup();
                    return fail(error, "project Assets contain a symlink");
                }
                if (it->is_directory(ec)) continue;
                if (ec || !it->is_regular_file(ec)) {
                    cleanup();
                    return fail(error, "invalid asset source file");
                }
                const auto source_path = it->path();
                const auto ext = source_path.extension().string();
                if (std::find(extensions.begin(), extensions.end(), ext) ==
                    extensions.end()) continue;
                const auto bytes = it->file_size(ec);
                if (ec || bytes > max_file || total > max_total - bytes ||
                    copied_assets >= 20000) {
                    cleanup();
                    return fail(error, "player assets exceed safe export limits");
                }
                total += bytes;
                const auto relative = source_path.lexically_relative(assets_root);
                const auto target = asset_target / relative;
                std::filesystem::create_directories(target.parent_path(), ec);
                if (ec) {
                    cleanup();
                    return fail(error, "cannot create staged asset directory");
                }
                std::filesystem::copy_file(source_path, target,
                    std::filesystem::copy_options::none, ec);
                if (ec) {
                    cleanup();
                    return fail(error, "cannot stage player asset");
                }
                ++copied_assets;
            }
            if (ec) {
                cleanup();
                return fail(error, "failed to enumerate project assets");
            }
        }
    }
    // Cook texture descriptors at export time, never inside the shipped
    // Player. The existing importer supplies source+descriptor roles.
    std::size_t cooked_textures = 0;
    if (settings.copy_supported_assets) {
        const auto packaged = staging / "Content" / "Assets";
        assets::AssetDatabase database(packaged);
        const auto scan = database.scan(false);
        for (const auto& message : scan.messages) {
            if (message.severity == assets::AssetMessageSeverity::Error) {
                cleanup();
                return fail(error, "cannot index packaged assets");
            }
        }
        const assets::ImporterDescriptor descriptor{
            "NEngine.Texture", 1, {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".webp"}, false};
        for (const auto& record : database.records()) {
            const auto extension = record.source_path.extension().string();
            if (extension != ".png" && extension != ".jpg" &&
                extension != ".jpeg" && extension != ".bmp" &&
                extension != ".tga" && extension != ".webp") continue;
            const auto folder = staging / "Content" / "Cooked" /
                record.guid.to_string();
            std::filesystem::create_directories(folder, ec);
            if (ec) { cleanup(); return fail(error, "cannot create cooked texture directory"); }
            const assets::ImportContext context{&record, &descriptor, folder};
            const auto cooked = assets::texture_source_importer(context);
            if (!cooked.success) {
                cleanup();
                return fail(error, "cannot cook Player texture: " + cooked.message);
            }
            ++cooked_textures;
        }
    }
    std::size_t managed_files = 0;
    if (!settings.managed_directory.empty()) {
        // Managed binaries are opt-in: never ship source code or arbitrary
        // build-directory files. Accept a closed set of runtime payloads.
        const auto name = settings.managed_assembly_name;
        if (name.empty() || name.size() > 100 ||
            !std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') ||
                       (c >= 'A' && c <= 'Z') ||
                       (c >= '0' && c <= '9') || c == '_' || c == '.';
            }) || name.front() == '.' || name.back() == '.') {
            cleanup(); return fail(error, "invalid managed assembly name");
        }
        const auto managed_root = std::filesystem::canonical(
            settings.managed_directory, ec);
        if (ec || !std::filesystem::is_directory(managed_root)) {
            cleanup(); return fail(error, "managed build directory missing");
        }
        const auto target = staging / "Content" / "Managed";
        std::filesystem::create_directory(target, ec);
        if (ec) { cleanup(); return fail(error, "cannot create managed payload directory"); }
        const std::array<std::string, 6> names{
            name + ".dll", name + ".runtimeconfig.json",
            name + ".deps.json", "NEngine.Bridge.dll",
            "NEngine.API.dll", name + ".pdb"};
        for (std::size_t i = 0; i < names.size(); ++i) {
            const auto source_path = managed_root / names[i];
            if (!std::filesystem::exists(source_path, ec)) {
                if (i < 2 || i == 3 || i == 4) {
                    cleanup(); return fail(error, "required managed runtime payload missing: " + names[i]);
                }
                continue;
            }
            const auto file_size = std::filesystem::file_size(source_path, ec);
            if (ec || std::filesystem::is_symlink(source_path, ec) ||
                file_size > 64u * 1024u * 1024u || ec) {
                cleanup(); return fail(error, "unsafe managed payload: " + names[i]);
            }
            std::filesystem::copy_file(source_path, target / names[i],
                std::filesystem::copy_options::none, ec);
            if (ec) { cleanup(); return fail(error, "failed to copy managed payload"); }
            ++managed_files;
        }
    }
    if (!settings.player_executable.empty()) {
        const auto executable = std::filesystem::canonical(
            settings.player_executable, ec);
        if (ec || !std::filesystem::is_regular_file(executable)) {
            cleanup();
            return fail(error, "Player executable is missing or not a regular file");
        }
#if defined(_WIN32)
        const auto target_name = "NEnginePlayer.exe";
#else
        const auto target_name = "NEnginePlayer";
#endif
        std::filesystem::copy_file(executable, staging / target_name,
            std::filesystem::copy_options::none, ec);
        if (ec) {
            cleanup();
            return fail(error, "cannot stage player executable");
        }
    }
    if (!save_bundle(manifest, staging / "NEngine.player", error)) {
        cleanup(); return false;
    }
    std::filesystem::rename(staging, final_path, ec);
    if (ec) { cleanup(); return fail(error, "cannot atomically finish player bundle"); }
    report = {final_path, scene.objects.size(), copied_assets,
              cooked_textures, managed_files};
    if (error) error->clear();
    return true;
}
} // namespace nengine::player
