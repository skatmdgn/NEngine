#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "nengine/assets/asset_guid.hpp"
#include "nengine/assets/asset_importer.hpp"

namespace nengine::assets {

struct AssetRecord {
    AssetGuid guid{};
    std::filesystem::path source_path{};
    std::filesystem::path relative_path{};
    std::filesystem::path meta_path{};
    std::string importer_id{};
    std::uint64_t file_size{0};
    std::int64_t write_stamp{0};
};

enum class AssetMessageSeverity : std::uint8_t {
    Info,
    Warning,
    Error,
};

struct AssetScanMessage {
    AssetMessageSeverity severity{AssetMessageSeverity::Info};
    std::filesystem::path path{};
    std::string message{};
};

struct AssetScanResult {
    std::size_t added{0};
    std::size_t updated{0};
    std::size_t removed{0};
    std::size_t meta_created{0};
    std::size_t meta_repaired{0};
    std::vector<AssetScanMessage> messages{};

    bool changed() const noexcept {
        return added != 0 || updated != 0 || removed != 0 ||
               meta_created != 0 || meta_repaired != 0;
    }
};

class AssetDatabase {
public:
    AssetDatabase() = default;
    explicit AssetDatabase(std::filesystem::path assets_root);

    void set_root(std::filesystem::path assets_root);
    const std::filesystem::path& root() const noexcept { return root_; }

    void set_importers(const ImporterRegistry* importers) noexcept {
        importers_ = importers;
    }

    AssetScanResult scan(bool create_missing_meta = true);

    const AssetRecord* find(AssetGuid guid) const noexcept;
    const AssetRecord* find_relative(std::string_view generic_relative_path) const noexcept;
    std::vector<AssetRecord> records() const;

    std::size_t size() const noexcept { return by_guid_.size(); }
    void clear();

private:
    struct MetaData {
        AssetGuid guid{};
        std::string importer_id{};
    };

    std::optional<MetaData> read_meta(
        const std::filesystem::path& path,
        std::string* error = nullptr) const;

    bool write_meta(
        const std::filesystem::path& path,
        const MetaData& meta,
        std::string* error = nullptr) const;

    std::string choose_importer(const std::filesystem::path& source) const;
    static std::string path_key(const std::filesystem::path& relative);

    std::filesystem::path root_{};
    const ImporterRegistry* importers_{nullptr};
    std::unordered_map<AssetGuid, AssetRecord, AssetGuidHash> by_guid_{};
    std::unordered_map<std::string, AssetGuid> by_path_{};
};

} // namespace nengine::assets
