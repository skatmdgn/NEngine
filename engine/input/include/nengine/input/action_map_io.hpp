#pragma once
#include <filesystem>
#include <string>
#include "nengine/input/input_state.hpp"

namespace nengine::input {
// Load only existing, non-symlinked, bounded v1 action-map files.
// Transactional on failure: out remains unchanged.
bool load_action_map_file(const std::filesystem::path& file, ActionMap& out,
                          std::string* error = nullptr);
// Creation does not overwrite an existing file by default. Authoring tools can
// explicitly request an atomic update; failures keep the previous good file.
bool save_action_map_file(const std::filesystem::path& file, const ActionMap& map,
                          std::string* error = nullptr, bool replace_existing = false);
} // namespace nengine::input
