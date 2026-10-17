#pragma once
#include <filesystem>
#include <string>
#include "nengine/input/input_state.hpp"

namespace nengine::input {
// Load only existing, non-symlinked, bounded v1 action-map files.
// Transactional on failure: out remains unchanged.
bool load_action_map_file(const std::filesystem::path& file, ActionMap& out,
                          std::string* error = nullptr);
// Atomic replacement for authoring tools; failure does not truncate existing.
bool save_action_map_file(const std::filesystem::path& file, const ActionMap& map,
                          std::string* error = nullptr);
} // namespace nengine::input
