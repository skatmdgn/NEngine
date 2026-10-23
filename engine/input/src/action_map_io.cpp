#include "nengine/input/action_map_io.hpp"
#include <fstream>
#include <chrono>
#include <string_view>
#include <iterator>
#include <utility>
#include <system_error>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#endif

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
                          std::string* error, bool replace_existing) {
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
    // Existing action maps are only replaced when the caller explicitly
    // requests an edit rather than first-time creation.
    if (std::filesystem::exists(status) && !replace_existing)
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
    // New or edited maps are published without an intermediate deletion.
    // On Windows rename() alone cannot replace existing files.
#if defined(_WIN32)
    const DWORD flags = replace_existing
        ? MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH
        : MOVEFILE_WRITE_THROUGH;
    if (!MoveFileExW(temporary.c_str(), file.c_str(), flags))
        ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
    else
        ec.clear();
#else
    std::filesystem::rename(temporary, file, ec);
#endif
    if (ec) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return fail(error, "failed to atomically publish action map");
    }
    if (error) error->clear();
    return true;
}
} // namespace nengine::input
