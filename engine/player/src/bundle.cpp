#include "nengine/player/bundle.hpp"

#include <array>
#include <algorithm>
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
    if (manifest.version != 1u && manifest.version != 2u &&
        manifest.version != BundleManifest::current_version)
        return fail(error, "unsupported player manifest version");
    if (manifest.title.empty() || manifest.title.size() > 256 ||
        manifest.title.find('\0') != std::string::npos ||
        manifest.title.find('\n') != std::string::npos)
        return fail(error, "invalid player title");
    const auto& v=manifest.game_version;
    if(v.empty()||v.size()>64u||
       !std::all_of(v.begin(),v.end(),[](unsigned char ch){
           return (ch>='0'&&ch<='9')||(ch>='A'&&ch<='Z')||
                  (ch>='a'&&ch<='z')||ch=='.'||ch=='-'||ch=='+';
       }))return fail(error,"invalid game build version");
    const auto& assembly = manifest.managed_assembly_name;
    if (assembly.empty() || assembly.size() > 100u ||
        (manifest.version == 1u && assembly != "GameScripts") ||
        assembly.front() == '.' || assembly.back() == '.' ||
        !std::all_of(assembly.begin(), assembly.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') ||
                   (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_' || c == '.';
        }))
        return fail(error, "invalid player managed assembly name");
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
           << "RESOLUTION " << manifest.width << ' ' << manifest.height << '\n';
    if (manifest.version >= 2u)
        output << "MANAGED_ASSEMBLY "
               << std::quoted(manifest.managed_assembly_name) << '\n';
    if(manifest.version>=3u)
        output<<"GAME_VERSION "<<std::quoted(manifest.game_version)<<'\n';
    output << "END_PLAYER_BUNDLE\n";
    return output.good() || fail(error, "writing player manifest failed");
}

bool read_bundle(std::istream& input, BundleManifest& manifest,
                 std::string* error) {
    // Bound the complete stream before std::quoted can allocate strings.
    constexpr std::size_t max_manifest_bytes = 8192u;
    std::string text;
    std::array<char, 1024> block{};
    while (true) {
        input.read(block.data(), static_cast<std::streamsize>(block.size()));
        const auto read = static_cast<std::size_t>(input.gcount());
        if (read > max_manifest_bytes - text.size())
            return fail(error, "player manifest exceeds 8KiB");
        text.append(block.data(), read);
        if (input.eof()) break;
        if (!input) return fail(error, "player manifest read failure");
    }

    std::istringstream fields{text};
    BundleManifest candidate;
    std::string key;
    std::string scene;
    if (!(fields >> key) || key != "NENGINE_PLAYER_BUNDLE" ||
        !(fields >> candidate.version) ||
        !(fields >> key) || key != "TITLE" ||
        !(fields >> std::quoted(candidate.title)) ||
        !(fields >> key) || key != "SCENE" ||
        !(fields >> std::quoted(scene)) ||
        !(fields >> key) || key != "RESOLUTION" ||
        !(fields >> candidate.width >> candidate.height))
        return fail(error, "invalid player manifest format");
    if (candidate.version >= 2u) {
        if (!(fields >> key) || key != "MANAGED_ASSEMBLY" ||
            !(fields >> std::quoted(candidate.managed_assembly_name)))
            return fail(error, "player managed assembly field missing");
    }
    if(candidate.version>=3u){
        if(!(fields>>key)||key!="GAME_VERSION"||
           !(fields>>std::quoted(candidate.game_version)))
            return fail(error,"game version field missing");
    }
    if (!(fields >> key) || key != "END_PLAYER_BUNDLE")
        return fail(error, "invalid player manifest terminator");
    fields >> std::ws;
    if (!fields.eof()) return fail(error, "trailing player manifest fields");
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
