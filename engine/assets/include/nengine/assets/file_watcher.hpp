#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace nengine::assets {

enum class FileChangeKind : std::uint8_t {
    Added,
    Modified,
    Removed,
};

struct FileChange {
    FileChangeKind kind{FileChangeKind::Modified};
    std::filesystem::path relative_path{};
};

class PollingFileWatcher {
public:
    PollingFileWatcher() = default;
    explicit PollingFileWatcher(std::filesystem::path root);

    void set_root(std::filesystem::path root);
    const std::filesystem::path& root() const noexcept { return root_; }

    void reset();
    std::vector<FileChange> poll();

private:
    struct Signature {
        std::uint64_t size{0};
        std::int64_t write_stamp{0};

        friend bool operator==(const Signature&, const Signature&) = default;
    };

    using Snapshot = std::unordered_map<std::string, Signature>;

    Snapshot capture() const;
    static bool should_watch(const std::filesystem::path& path);

    std::filesystem::path root_{};
    Snapshot snapshot_{};
    bool initialized_{false};
};

} // namespace nengine::assets
