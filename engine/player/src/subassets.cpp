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
        return roles.size() == 1 && roles.contains("source");
    return roles.size() == 2 &&
           roles.contains("source") && roles.contains("texture-descriptor");
}
} // namespace

bool write_model_subassets(const std::filesystem::path& model_folder,
    const std::vector<assets::GeneratedSubasset>& subassets,
    std::string* error) {
    if (!subassets.empty()) return fail(error, "subasset writer not yet initialized");
    if (error) error->clear();
    return true;
}

bool read_model_subassets(const std::filesystem::path&,
    std::vector<CookedSubasset>& out, std::string* error) {
    out.clear();
    if (error) error->clear();
    return true;
}
} // namespace nengine::player
