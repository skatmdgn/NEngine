#include "nengine/editor/action_map_editor.hpp"
#include "nengine/input/action_map_io.hpp"
#include <system_error>
namespace nengine::editor {
bool ActionMapEditor::open(std::filesystem::path root,std::string* error) {
    std::error_code ec;
    if(!std::filesystem::is_directory(root/"ProjectSettings",ec)||ec) {
        if(error)*error="ProjectSettings directory is missing";
        return false;
    }
    auto new_path=root/"ProjectSettings"/"InputActions.naction";
    input::ActionMap temp;
    const auto status=std::filesystem::symlink_status(new_path,ec);
    if(ec==std::errc::no_such_file_or_directory)ec.clear();
    if(ec||std::filesystem::is_symlink(status)) {
        if(error)*error="Unsafe action-map file";return false;
    }
    if(std::filesystem::exists(status)&&
       !input::load_action_map_file(new_path,temp,error))return false;
    map_=std::move(temp);
    path_=std::move(new_path);
    if(error)error->clear();
    return true;
}
bool ActionMapEditor::bind(std::string name,input::Key key,std::string* error) {
    if(path_.empty()||!map_.bind(std::move(name),key)){
        if(error)*error="No project open, bad action name, key or capacity";
        return false;
    }
    if(error)error->clear();return true;
}
bool ActionMapEditor::erase(std::string_view name,std::string* error) {
    if(path_.empty()||!map_.unbind(name)){
        if(error)*error="Action does not exist";return false;
    }
    if(error)error->clear();return true;
}
bool ActionMapEditor::save(std::string* error) {
    if(path_.empty()) {
        if(error)*error="No project open";return false;
    }
    return input::save_action_map_file(path_,map_,error,true);
}
}
