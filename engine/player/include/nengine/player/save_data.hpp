#pragma once
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace nengine::player {

// Binary game data is independent of C# or the Editor. A game can supply a
// serialized payload, and migrate older versions before committing a new save.
struct GameSave {
    std::uint32_t version{1};
    std::vector<std::uint8_t> bytes{};
};
using SaveMigration = std::function<bool(
    std::uint32_t old_version,
    std::uint32_t target_version,
    std::vector<std::uint8_t>& payload)>;

class GameSaveStore {
public:
    explicit GameSaveStore(std::filesystem::path root):root_(std::move(root)) {}
    bool write(std::string slot, const GameSave& data,
               std::string* error=nullptr) const;
    bool read(std::string slot,GameSave& output,
              std::string* error=nullptr) const;
    bool read_migrated(std::string slot,std::uint32_t expected_version,
                       const SaveMigration& migrate,GameSave& output,
                       std::string* error=nullptr) const;
    const std::filesystem::path& root() const noexcept {return root_;}
    static std::optional<std::filesystem::path> user_data_directory(
        std::string app_id);
private:
    std::filesystem::path root_;
};

} // namespace nengine::player
