#include "nengine/input/action_map_io.hpp"
#include <fstream>
#include <chrono>
#include <string_view>
#include <iterator>
#include <utility>
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


bool save_action_map_file(const std::filesystem::path& file, const ActionMap& map,
                          std::string* error) {
    std::string encoded;
    if (!map.serialize(encoded, error)) return false;
    std::error_code ec;
    if (file.empty() || file.filename().empty() ||
        !std::filesystem::is_directory(file.parent_path(), ec) || ec)
        return fail(error, "invalid action map output path");
    // symlink_status() distinguishes missing destinations from symlinks.
    // is_symlink(path, ec) alone reports ENOENT for a NEW file on Linux.
    const auto status = std::filesystem::symlink_status(file, ec);
    if (ec == std::errc::no_such_file_or_directory) ec.clear();
    if (ec || std::filesystem::is_symlink(status))
        return fail(error, "invalid or symlinked action map destination");
    // Never replace an existing file, including on POSIX.
    if (std::filesystem::exists(status))
        return fail(error, "action map target already exists");
    auto temporary = file;
    temporary += ".nengine-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count()) + ".tmp";
    if (std::filesystem::exists(temporary, ec) || ec)
        return fail(error, "cannot allocate action map staging file");
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        if (!out || !(out << encoded) || !out.flush()) {
            out.close();
            std::filesystem::remove(temporary, ec);
            return fail(error, "failed to stage action map data");
        }
    }
    // On Windows std::filesystem::rename does not replace existing target;
    // retain the existing good file instead of deleting it on failure.
    std::filesystem::rename(temporary, file, ec);
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return fail(error, "failed to atomically publish action map");
    }
    if (error) error->clear();
    return true;
}
} // namespace nengine::input
