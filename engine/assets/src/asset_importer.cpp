#include "nengine/assets/asset_importer.hpp"

#include <algorithm>
#include <cctype>
#include <utility>

namespace nengine::assets {
namespace {

std::string normalized_extension(std::string value) {
    std::transform(
        value.begin(),
        value.end(),
        value.begin(),
        [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });

    if (!value.empty() && value.front() != '.') {
        value.insert(value.begin(), '.');
    }
    return value;
}

} // namespace

bool ImporterRegistry::register_importer(ImporterDescriptor descriptor) {
    if (descriptor.id.empty() || importers_.contains(descriptor.id)) {
        return false;
    }

    for (auto& extension : descriptor.extensions) {
        extension = normalized_extension(std::move(extension));
    }

    if (descriptor.fallback) {
        if (!fallback_id_.empty()) return false;
        fallback_id_ = descriptor.id;
    }

    importers_.emplace(descriptor.id, std::move(descriptor));
    return true;
}

const ImporterDescriptor* ImporterRegistry::find(std::string_view id) const noexcept {
    const auto it = importers_.find(std::string{id});
    return it == importers_.end() ? nullptr : &it->second;
}

const ImporterDescriptor* ImporterRegistry::find_for_path(
    const std::filesystem::path& path) const noexcept {

    const auto extension =
        normalized_extension(path.extension().string());

    for (const auto& [_, descriptor] : importers_) {
        if (descriptor.fallback) continue;

        if (std::find(
                descriptor.extensions.begin(),
                descriptor.extensions.end(),
                extension) != descriptor.extensions.end()) {
            return &descriptor;
        }
    }

    return fallback_id_.empty() ? nullptr : find(fallback_id_);
}

std::vector<ImporterDescriptor> ImporterRegistry::descriptors() const {
    std::vector<ImporterDescriptor> result;
    result.reserve(importers_.size());

    for (const auto& [_, descriptor] : importers_) {
        result.push_back(descriptor);
    }

    std::sort(
        result.begin(),
        result.end(),
        [](const auto& a, const auto& b) {
            return a.id < b.id;
        });

    return result;
}

} // namespace nengine::assets
