#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <optional>
#include <string>

namespace nengine::player {

// A portable, versioned runtime launch descriptor. A player bundle is not a
// Unity project and never executes commands embedded in a manifest.
struct BundleManifest {
    static constexpr std::uint32_t current_version = 1;
    std::uint32_t version{current_version};
    std::string title{"NEngine Game"};
    std::filesystem::path scene{"Content/Main.nscene"};
    std::uint32_t width{1280};
    std::uint32_t height{720};
};

// Resolve existing scene only if it remains inside the bundle after symlinks.
std::optional<std::filesystem::path> resolve_bundle_scene(
    const std::filesystem::path& root, const BundleManifest& manifest,
    std::string* error = nullptr);

bool validate_bundle(const BundleManifest& manifest,
                     std::string* error = nullptr);
bool write_bundle(std::ostream& output, const BundleManifest& manifest,
                  std::string* error = nullptr);
bool read_bundle(std::istream& input, BundleManifest& manifest,
                 std::string* error = nullptr);
bool save_bundle(const BundleManifest& manifest,
                 const std::filesystem::path& file,
                 std::string* error = nullptr);
bool load_bundle(const std::filesystem::path& file, BundleManifest& manifest,
                 std::string* error = nullptr);

} // namespace nengine::player
