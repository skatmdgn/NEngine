#pragma once
#include <filesystem>
#include <string>
#include <vector>
#include "nengine/input/input_state.hpp"
namespace nengine::editor {
// INPUT-003: GUI-backed validated project action-map document, saved through
// the existing atomic .naction transaction format.
class ActionMapEditor {
public:
    bool open(std::filesystem::path project_root,std::string* error=nullptr);
    bool bind(std::string name,input::Key key,std::string* error=nullptr);
    bool erase(std::string_view name,std::string* error=nullptr);
    bool save(std::string* error=nullptr);
    const input::ActionMap& map() const noexcept {return map_;}
    std::vector<std::string> actions() const {return map_.action_names();}
private:
    std::filesystem::path path_{};
    input::ActionMap map_{};
};
}
