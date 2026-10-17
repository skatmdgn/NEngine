#include "nengine/player/subassets.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>

namespace nengine::player {
namespace {
constexpr std::size_t max_subassets = 1024;
constexpr std::size_t max_artifacts = 12;
constexpr std::uintmax_t max_manifest_size = 2u * 1024u * 1024u;
bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
}
bool safe_relative(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute() || path.has_root_path() ||
        path.generic_string().size() > 512u) return false;
    for (const auto& part : path) {
        if (part.empty() || part == "." || part == "..") return false;
    }
    const auto text = path.generic_string();
    return text.find(':') == std::string::npos &&
           text.find('\\') == std::string::npos;
}
bool contained_file(const std::filesystem::path& folder,
                    const std::filesystem::path& relative) {
    if (!safe_relative(relative) ||
        *relative.begin() != "subassets") return false;
    std::error_code ec;
    const auto root = std::filesystem::canonical(folder, ec);
    if (ec) return false;
    auto next = folder;
    for (const auto& part : relative) {
        next /= part;
        if (std::filesystem::is_symlink(next, ec) || ec) return false;
    }
    const auto path = std::filesystem::canonical(next, ec);
    if (ec || !std::filesystem::is_regular_file(path, ec) || ec) return false;
    const auto rel = path.lexically_relative(root);
    if (rel.empty() || rel.is_absolute() || *rel.begin() == "..") return false;
    const auto bytes = std::filesystem::file_size(path, ec);
    return !ec && bytes <= 512ull * 1024ull * 1024ull;
}
bool valid_importer(const std::string& id) {
    return id == "NEngine.Texture" || id == "NEngine.Material";
}
bool valid_role(const CookedSubasset& item) {
    if (!item.guid.valid() || !valid_importer(item.importer_id) ||
        item.artifacts.empty() || item.artifacts.size() > max_artifacts)
        return false;
    std::set<std::string> roles;
    for (const auto& a : item.artifacts)
        if (!roles.insert(a.role).second) return false;
    if (item.importer_id == "NEngine.Material")
        return roles.size() == 1 && roles.contains("source") &&
            std::any_of(item.artifacts.begin(), item.artifacts.end(),
                [](const auto& a) { return a.role == "source" &&
                                          a.path.extension() == ".nmat"; });
    if (!(roles.size() == 2 && roles.contains("source") &&
          roles.contains("texture-descriptor"))) return false;
    return std::any_of(item.artifacts.begin(), item.artifacts.end(),
        [](const auto& a) {
            return a.role == "texture-descriptor" &&
                   a.path.extension() == ".nasset";
        }) && std::any_of(item.artifacts.begin(), item.artifacts.end(),
        [](const auto& a) {
            return a.role == "source" && a.path.extension() == ".tga";
        });
}
} // namespace

bool write_model_subassets(const std::filesystem::path& model_folder,
    const std::vector<assets::GeneratedSubasset>& subassets,
    std::string* error) {
    if (subassets.size() > max_subassets)
        return fail(error, "too many generated model subassets");
    if (subassets.empty()) { if (error) error->clear(); return true; }
    std::set<std::string> seen;
    std::ostringstream encoded;
    encoded << "NENGINE_PLAYER_SUBASSETS 1\nCOUNT " << subassets.size() << "\n";
    for (const auto& original : subassets) {
        CookedSubasset item{original.guid, original.importer_id, original.artifacts};
        if (!valid_role(item) || !seen.insert(item.guid.to_string()).second)
            return fail(error, "invalid or duplicate generated subasset");
        encoded << "ITEM " << std::quoted(item.guid.to_string()) << " "
                << std::quoted(item.importer_id) << " "
                << item.artifacts.size() << "\n";
        for (const auto& artifact : item.artifacts) {
            const auto rel = artifact.path.lexically_relative(model_folder);
            if (!contained_file(model_folder, rel))
                return fail(error, "unsafe generated artifact path");
            encoded << "ARTIFACT " << std::quoted(rel.generic_string()) << " "
                    << std::quoted(artifact.role) << "\n";
        }
        encoded << "END_ITEM\n";
    }
    encoded << "END_SUBASSETS\n";
    if (encoded.str().size() > max_manifest_size)
        return fail(error, "generated model subasset index exceeds size limit");
    std::ofstream stream(model_folder / "subassets.nasset", std::ios::binary);
    if (!stream || !(stream << encoded.str()) || !stream.flush())
        return fail(error, "failed to publish model subasset index");
    if (error) error->clear();
    return true;
}

bool read_model_subassets(const std::filesystem::path& model_folder,
    std::vector<CookedSubasset>& out, std::string* error) {
    const auto path = model_folder / "subassets.nasset";
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec == std::errc::no_such_file_or_directory) ec.clear();
    if (ec || std::filesystem::is_symlink(status))
        return fail(error, "unsafe model subasset index");
    if (!std::filesystem::exists(status)) {
        out.clear();
        if (error) error->clear();
        return true;
    }
    const auto bytes = std::filesystem::file_size(path, ec);
    if (ec || bytes > max_manifest_size)
        return fail(error, "model subasset index exceeds size limit");
    std::ifstream stream(path, std::ios::binary);
    std::string token;
    int version = 0;
    std::size_t count = 0;
    if (!stream || !(stream >> token >> version) ||
        token != "NENGINE_PLAYER_SUBASSETS" || version != 1 ||
        !(stream >> token >> count) || token != "COUNT" || count > max_subassets)
        return fail(error, "invalid model subasset index header");
    std::vector<CookedSubasset> parsed;
    std::set<std::string> seen;
    for (std::size_t i = 0; i < count; ++i) {
        CookedSubasset item;
        std::string guid;
        std::size_t artifacts = 0;
        if (!(stream >> token >> std::quoted(guid) >>
              std::quoted(item.importer_id) >> artifacts) || token != "ITEM" ||
            artifacts == 0 || artifacts > max_artifacts)
            return fail(error, "invalid model subasset entry");
        const auto parsed_guid = assets::AssetGuid::parse(guid);
        if (!parsed_guid || !parsed_guid->valid() || !seen.insert(guid).second)
            return fail(error, "duplicate or invalid model subasset GUID");
        item.guid = *parsed_guid;
        for (std::size_t j = 0; j < artifacts; ++j) {
            std::string relative, role;
            if (!(stream >> token >> std::quoted(relative) >> std::quoted(role)) ||
                token != "ARTIFACT" ||
                !contained_file(model_folder, relative))
                return fail(error, "invalid or unsafe model subasset artifact");
            item.artifacts.push_back({model_folder / relative, role});
        }
        if (!(stream >> token) || token != "END_ITEM" || !valid_role(item))
            return fail(error, "invalid model subasset type or roles");
        parsed.push_back(std::move(item));
    }
    if (!(stream >> token) || token != "END_SUBASSETS" || stream >> token)
        return fail(error, "model subasset index terminator invalid");
    if (!stream.eof()) return fail(error, "model subasset index trailing stream error");
    out = std::move(parsed);
    if (error) error->clear();
    return true;
}
} // namespace nengine::player
