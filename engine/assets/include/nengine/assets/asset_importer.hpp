#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nengine::assets {

struct ImporterDescriptor {
    std::string id{};
    std::uint32_t version{1};
    std::vector<std::string> extensions{};
    bool fallback{false};
};

class ImporterRegistry {
public:
    bool register_importer(ImporterDescriptor descriptor);
    const ImporterDescriptor* find(std::string_view id) const noexcept;
    const ImporterDescriptor* find_for_path(const std::filesystem::path& path) const noexcept;
    std::vector<ImporterDescriptor> descriptors() const;

private:
    std::unordered_map<std::string, ImporterDescriptor> importers_{};
    std::string fallback_id_{};
};

} // namespace nengine::assets
