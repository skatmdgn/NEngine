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
bool fail(std::string* error, std::string message) {
    if (error) *error = std::move(message);
    return false;
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
