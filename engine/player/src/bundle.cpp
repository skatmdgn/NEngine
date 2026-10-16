#include "nengine/player/bundle.hpp"

#include <fstream>
#include <iomanip>
#include <sstream>
#include <utility>

namespace nengine::player {
namespace {
bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}
}

bool validate_bundle(const BundleManifest& manifest, std::string* error) {
    if (manifest.version != BundleManifest::current_version)
        return fail(error, "unsupported player manifest version");
    if (manifest.title.empty() || manifest.title.size() > 256 ||
        manifest.title.find('\0') != std::string::npos ||
        manifest.title.find('\n') != std::string::npos)
        return fail(error, "invalid player title");
    if (manifest.width < 64 || manifest.height < 64 ||
        manifest.width > 16384 || manifest.height > 16384)
        return fail(error, "invalid player resolution");
    if (manifest.scene.empty() || manifest.scene.is_absolute() ||
        manifest.scene.has_root_name() || manifest.scene.has_root_directory() ||
        manifest.scene.extension() != ".nscene")
        return fail(error, "invalid player startup scene path");
    for (const auto& part : manifest.scene)
        if (part == "." || part == ".." || part.empty())
            return fail(error, "player startup scene path cannot traverse");
    if (manifest.scene.generic_string().size() > 1024)
        return fail(error, "player startup scene path too long");
    if (error) error->clear();
    return true;
}

std::optional<std::filesystem::path> resolve_bundle_scene(
    const std::filesystem::path& root, const BundleManifest& manifest,
    std::string* error) {
    if (!validate_bundle(manifest, error)) return std::nullopt;
    std::error_code ec;
    const auto base = std::filesystem::canonical(root, ec);
    if (ec || !std::filesystem::is_directory(base))
        return fail(error, "bundle root does not exist"), std::nullopt;
    const auto requested = base / manifest.scene;
    if (std::filesystem::is_symlink(requested, ec) || ec)
        return fail(error, "bundle scene may not be a symlink"), std::nullopt;
    const auto source = std::filesystem::canonical(requested, ec);
    if (ec || !std::filesystem::is_regular_file(source))
        return fail(error, "bundle scene is not an existing file"), std::nullopt;
    const auto relative = source.lexically_relative(base);
    if (relative.empty() || relative == "." || relative.is_absolute() ||
        *relative.begin() == "..")
        return fail(error, "bundle scene escapes root"), std::nullopt;
    if (error) error->clear();
    return source;
}

bool write_bundle(std::ostream& output, const BundleManifest& manifest,
                  std::string* error) {
    if (!validate_bundle(manifest, error)) return false;
    output << "NENGINE_PLAYER_BUNDLE " << manifest.version << '\n'
           << "TITLE " << std::quoted(manifest.title) << '\n'
           << "SCENE " << std::quoted(manifest.scene.generic_string()) << '\n'
           << "RESOLUTION " << manifest.width << ' ' << manifest.height << '\n'
           << "END_PLAYER_BUNDLE\n";
    return output.good() || fail(error, "writing player manifest failed");
}

bool read_bundle(std::istream& input, BundleManifest& manifest,
                 std::string* error) {
    BundleManifest candidate;
    std::string key;
    std::string scene;
    if (!(input >> key) || key != "NENGINE_PLAYER_BUNDLE" ||
        !(input >> candidate.version) ||
        !(input >> key) || key != "TITLE" ||
        !(input >> std::quoted(candidate.title)) ||
        !(input >> key) || key != "SCENE" ||
        !(input >> std::quoted(scene)) ||
        !(input >> key) || key != "RESOLUTION" ||
        !(input >> candidate.width >> candidate.height) ||
        !(input >> key) || key != "END_PLAYER_BUNDLE")
        return fail(error, "invalid player manifest format");
    input >> std::ws;
    if (!input.eof()) return fail(error, "trailing player manifest fields");
    candidate.scene = std::filesystem::u8path(scene);
    if (!validate_bundle(candidate, error)) return false;
    manifest = std::move(candidate);
    if (error) error->clear();
    return true;
}

bool save_bundle(const BundleManifest& manifest,
                 const std::filesystem::path& file, std::string* error) {
    std::ofstream stream(file, std::ios::binary | std::ios::trunc);
    if (!stream) return fail(error, "cannot open player manifest for writing");
    return write_bundle(stream, manifest, error);
}

bool load_bundle(const std::filesystem::path& file, BundleManifest& manifest,
                 std::string* error) {
    std::ifstream stream(file, std::ios::binary);
    if (!stream) return fail(error, "cannot open player manifest");
    return read_bundle(stream, manifest, error);
}

} // namespace nengine::player
