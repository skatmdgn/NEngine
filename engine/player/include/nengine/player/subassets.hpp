#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "nengine/assets/import_pipeline.hpp"

namespace nengine::player {
// Portable v1 model-generated material/texture subasset inventory. All
// paths are relative to the cooked model directory and validated on read.
struct CookedSubasset {
    assets::AssetGuid guid{};
    std::string importer_id{};
    std::vector<assets::ImportArtifact> artifacts{};
};

bool write_model_subassets(const std::filesystem::path& model_folder,
    const std::vector<assets::GeneratedSubasset>& subassets,
    std::string* error = nullptr);

// Missing index means a legacy model without generated material slots.
bool read_model_subassets(const std::filesystem::path& model_folder,
    std::vector<CookedSubasset>& out,
    std::string* error = nullptr);
} // namespace nengine::player
