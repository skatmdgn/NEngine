#include "nengine/assets/asset_database.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <system_error>
#include <utility>

namespace nengine::assets {
namespace {

std::int64_t write_stamp(const std::filesystem::path& path) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(path, error);
    if (error) return 0;
    return static_cast<std::int64_t>(time.time_since_epoch().count());
}

bool same_record(const AssetRecord& a, const AssetRecord& b) {
    return a.guid == b.guid &&
           a.relative_path == b.relative_path &&
           a.importer_id == b.importer_id &&
           a.file_size == b.file_size &&
           a.write_stamp == b.write_stamp;
}

} // namespace

AssetDatabase::AssetDatabase(std::filesystem::path assets_root) {
    set_root(std::move(assets_root));
}

void AssetDatabase::set_root(std::filesystem::path assets_root) {
    std::error_code error;
    auto absolute = std::filesystem::absolute(assets_root, error);
    root_ = (error ? std::move(assets_root) : std::move(absolute)).lexically_normal();
    clear();
}

std::string AssetDatabase::path_key(const std::filesystem::path& relative) {
    return relative.generic_string();
}

std::string AssetDatabase::choose_importer(
    const std::filesystem::path& source) const {

    if (importers_) {
        if (const auto* descriptor = importers_->find_for_path(source)) {
            return descriptor->id;
        }
    }
    return "NEngine.Raw";
}

std::optional<AssetDatabase::MetaData> AssetDatabase::read_meta(
    const std::filesystem::path& path,
    std::string* error) const {

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "could not open meta file";
        return std::nullopt;
    }

    std::string token;
    std::uint32_t version = 0;

    if (!(input >> token >> version) ||
        token != "NENGINE_META" ||
        version != 1) {
        if (error) *error = "invalid meta header";
        return std::nullopt;
    }

    std::string guid_text;
    if (!(input >> token >> guid_text) || token != "GUID") {
        if (error) *error = "missing GUID";
        return std::nullopt;
    }

    const auto guid = AssetGuid::parse(guid_text);
    if (!guid) {
        if (error) *error = "invalid GUID";
        return std::nullopt;
    }

    MetaData meta;
    meta.guid = *guid;

    if (!(input >> token) || token != "IMPORTER") {
        if (error) *error = "missing IMPORTER";
        return std::nullopt;
    }

    if (!(input >> std::quoted(meta.importer_id))) {
        if (error) *error = "invalid importer id";
        return std::nullopt;
    }

    if (!(input >> token) || token != "END_META") {
        if (error) *error = "missing END_META";
        return std::nullopt;
    }

    return meta;
}

bool AssetDatabase::write_meta(
    const std::filesystem::path& path,
    const MetaData& meta,
    std::string* error) const {

    std::ofstream output(
        path,
        std::ios::binary | std::ios::trunc);

    if (!output) {
        if (error) *error = "could not open meta file for writing";
        return false;
    }

    output << "NENGINE_META 1\n";
    output << "GUID " << meta.guid.to_string() << "\n";
    output << "IMPORTER " << std::quoted(meta.importer_id) << "\n";
    output << "END_META\n";

    if (!output.good()) {
        if (error) *error = "failed while writing meta file";
        return false;
    }
    return true;
}

AssetScanResult AssetDatabase::scan(bool create_missing_meta) {
    AssetScanResult result;

    if (root_.empty()) {
        result.messages.push_back({
            AssetMessageSeverity::Error,
            {},
            "asset root is empty"
        });
        return result;
    }

    std::error_code error;
    std::filesystem::create_directories(root_, error);
    if (error) {
        result.messages.push_back({
            AssetMessageSeverity::Error,
            root_,
            "could not create asset root"
        });
        return result;
    }

    const auto old_by_guid = by_guid_;

    std::unordered_map<AssetGuid, AssetRecord, AssetGuidHash> next_by_guid;
    std::unordered_map<std::string, AssetGuid> next_by_path;

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

        const auto source = entry.path();
        if (source.extension() == ".meta") continue;

        auto relative = source.lexically_relative(root_);
        if (relative.empty()) continue;

        auto meta_path = source;
        meta_path += ".meta";

        MetaData meta;
        bool meta_valid = false;
        std::string meta_error;

        if (std::filesystem::exists(meta_path, error) && !error) {
            if (const auto loaded = read_meta(meta_path, &meta_error)) {
                meta = *loaded;
                meta_valid = true;
            } else {
                result.messages.push_back({
                    AssetMessageSeverity::Warning,
                    meta_path,
                    "invalid meta repaired: " + meta_error
                });
            }
        }
        error.clear();

        if (!meta_valid) {
            if (!create_missing_meta) {
                result.messages.push_back({
                    AssetMessageSeverity::Warning,
                    source,
                    "asset has no valid meta file"
                });
                continue;
            }

            meta.guid = AssetGuid::generate();
            meta.importer_id = choose_importer(source);

            if (!write_meta(meta_path, meta, &meta_error)) {
                result.messages.push_back({
                    AssetMessageSeverity::Error,
                    meta_path,
                    "failed to write meta: " + meta_error
                });
                continue;
            }

            if (std::filesystem::exists(meta_path, error)) {
                if (result.messages.empty() ||
                    result.messages.back().path != meta_path) {
                    ++result.meta_created;
                } else {
                    ++result.meta_repaired;
                }
            }
            error.clear();
        }

        if (meta.importer_id.empty() ||
            meta.importer_id == "auto") {
            meta.importer_id = choose_importer(source);
            if (create_missing_meta) {
                write_meta(meta_path, meta, nullptr);
            }
        }

        if (next_by_guid.contains(meta.guid)) {
            const auto old_guid = meta.guid;
            meta.guid = AssetGuid::generate();

            if (!write_meta(meta_path, meta, &meta_error)) {
                result.messages.push_back({
                    AssetMessageSeverity::Error,
                    meta_path,
                    "duplicate GUID could not be repaired"
                });
                continue;
            }

            ++result.meta_repaired;
            result.messages.push_back({
                AssetMessageSeverity::Warning,
                meta_path,
                "duplicate GUID repaired; previous GUID was " +
                    old_guid.to_string()
            });
        }

        AssetRecord record;
        record.guid = meta.guid;
        record.source_path = source;
        record.relative_path = relative;
        record.meta_path = meta_path;
        record.importer_id = meta.importer_id;
        record.file_size = static_cast<std::uint64_t>(
            entry.file_size(error));
        if (error) {
            record.file_size = 0;
            error.clear();
        }
        record.write_stamp = write_stamp(source);

        next_by_path.emplace(
            path_key(relative),
            record.guid);

        next_by_guid.emplace(
            record.guid,
            std::move(record));
    }

    if (error) {
        result.messages.push_back({
            AssetMessageSeverity::Error,
            root_,
            "asset directory scan did not complete cleanly"
        });
    }

    for (const auto& [guid, record] : next_by_guid) {
        const auto previous = old_by_guid.find(guid);
        if (previous == old_by_guid.end()) {
            ++result.added;
        } else if (!same_record(previous->second, record)) {
            ++result.updated;
        }
    }

    for (const auto& [guid, _] : old_by_guid) {
        if (!next_by_guid.contains(guid)) {
            ++result.removed;
        }
    }

    by_guid_ = std::move(next_by_guid);
    by_path_ = std::move(next_by_path);
    return result;
}

const AssetRecord* AssetDatabase::find(AssetGuid guid) const noexcept {
    const auto it = by_guid_.find(guid);
    return it == by_guid_.end() ? nullptr : &it->second;
}

const AssetRecord* AssetDatabase::find_relative(
    std::string_view generic_relative_path) const noexcept {

    const auto path = by_path_.find(
        std::string{generic_relative_path});

    if (path == by_path_.end()) return nullptr;
    return find(path->second);
}

std::vector<AssetRecord> AssetDatabase::records() const {
    std::vector<AssetRecord> result;
    result.reserve(by_guid_.size());

    for (const auto& [_, record] : by_guid_) {
        result.push_back(record);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const auto& a, const auto& b) {
            return a.relative_path.generic_string() <
                   b.relative_path.generic_string();
        });

    return result;
}

void AssetDatabase::clear() {
    by_guid_.clear();
    by_path_.clear();
}

} // namespace nengine::assets
