#include "nengine/editor/build_settings_editor.hpp"
namespace nengine::editor {
BuildSettingsDraft read_build_settings(const ProjectSession& project){
    const auto& m=project.manifest();
    return {m.name,m.target_windows,m.target_android,m.android_toolchain};
}
bool apply_build_settings(ProjectSession& project,
    const BuildSettingsDraft& draft,std::string* error){
    if(!project.is_open()){
        if(error)*error="Build Settings requires an open project";return false;
    }
    ProjectManifest next=project.manifest();
    next.name=draft.project_name;
    next.target_windows=draft.windows;
    next.target_android=draft.android;
    next.android_toolchain=draft.android_toolchain;
    if(!ProjectManifestSerializer::validate(next,error))return false;
    if(!ProjectManifestSerializer::save(next,project.manifest_path(),error))
        return false;
    project.manifest()=std::move(next);
    if(error)error->clear();
    return true;
}
}
