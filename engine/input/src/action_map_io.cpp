#include "nengine/input/action_map_io.hpp"
#include <fstream>
#include <chrono>
#include <iterator>
#include <system_error>

namespace nengine::input {
namespace {
bool fail(std::string* error, std::string value) {
    if (error) *error = std::move(value);
    return false;
}
}

bool load_action_map_file(const std::filesystem::path& file, ActionMap& out,
                          std::string* error) {
    std::error_code ec;
    if (std::filesystem::is_symlink(file, ec) || ec)
        return fail(error, "action map symlink is not allowed");
    const auto size = std::filesystem::file_size(file, ec);
    if (ec || size > 65536u || !std::filesystem::is_regular_file(file, ec) || ec)
        return fail(error, "action map file missing, unsafe or exceeds 64KiB");
    std::ifstream stream(file, std::ios::binary);
    if (!stream) return fail(error, "cannot read action map file");
    std::string contents(static_cast<std::size_t>(size), '\0');
    if (!contents.empty() && !stream.read(contents.data(), contents.size()))
        return fail(error, "short action map read");
    char extra{};
    if (stream.get(extra)) return fail(error, "action map changed while reading");
    if (!stream.eof()) return fail(error, "action map stream error");
    return ActionMap::deserialize(contents, out, error);
}

} // namespace nengine::input
