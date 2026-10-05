#include "nengine/assets/file_watcher.hpp"

#include <algorithm>
#include <system_error>

namespace nengine::assets {
namespace {

std::int64_t write_stamp(const std::filesystem::path& path) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(path, error);
    if (error) return 0;
    return static_cast<std::int64_t>(time.time_since_epoch().count());
}

} // namespace

PollingFileWatcher::PollingFileWatcher(std::filesystem::path root) {
    set_root(std::move(root));
}

void PollingFileWatcher::set_root(std::filesystem::path root) {
    root_ = std::move(root);
    reset();
}

void PollingFileWatcher::reset() {
    snapshot_.clear();
    initialized_ = false;
}

bool PollingFileWatcher::should_watch(const std::filesystem::path& path) {
    return path.extension() != ".meta";
}

PollingFileWatcher::Snapshot PollingFileWatcher::capture() const {
    Snapshot result;
    if (root_.empty()) return result;

    std::error_code error;
    if (!std::filesystem::exists(root_, error)) return result;

    std::filesystem::recursive_directory_iterator iterator(
        root_,
        std::filesystem::directory_options::skip_permission_denied,
        error);

    const std::filesystem::recursive_directory_iterator end;
    for (; !error && iterator != end; iterator.increment(error)) {
        const auto& entry = *iterator;
        if (!entry.is_regular_file(error) || error) {
            error.clear();
            continue;
        }

        const auto& path = entry.path();
        if (!should_watch(path)) continue;

        const auto relative =
            path.lexically_relative(root_).generic_string();

        Signature signature{};
        signature.size =
            static_cast<std::uint64_t>(entry.file_size(error));
        if (error) {
            error.clear();
            signature.size = 0;
        }
        signature.write_stamp = write_stamp(path);

        result.emplace(relative, signature);
    }

    return result;
}

std::vector<FileChange> PollingFileWatcher::poll() {
    const auto current = capture();

    if (!initialized_) {
        snapshot_ = current;
        initialized_ = true;
        return {};
    }

    std::vector<FileChange> changes;

    for (const auto& [path, signature] : current) {
        const auto previous = snapshot_.find(path);
        if (previous == snapshot_.end()) {
            changes.push_back({FileChangeKind::Added, path});
        } else if (!(previous->second == signature)) {
            changes.push_back({FileChangeKind::Modified, path});
        }
    }

    for (const auto& [path, _] : snapshot_) {
        if (!current.contains(path)) {
            changes.push_back({FileChangeKind::Removed, path});
        }
    }

    std::sort(
        changes.begin(),
        changes.end(),
        [](const auto& a, const auto& b) {
            return a.relative_path.generic_string() <
                   b.relative_path.generic_string();
        });

    snapshot_ = current;
    return changes;
}

} // namespace nengine::assets
