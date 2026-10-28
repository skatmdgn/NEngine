#include "nengine/editor/build_settings_editor.hpp"
#include <fstream>
#include <iomanip>
#include <system_error>
#include <utility>
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

namespace {
bool tool_error(std::string* error,const std::string& why){
    if(error)*error=why;
    return false;
}
bool real_file(const std::filesystem::path& path){
    std::error_code ec;
    return !std::filesystem::is_symlink(path,ec) && !ec &&
           std::filesystem::is_regular_file(path,ec) && !ec;
}
bool real_dir(const std::filesystem::path& path){
    std::error_code ec;
    return !path.empty() && !std::filesystem::is_symlink(path,ec) && !ec &&
           std::filesystem::is_directory(path,ec) && !ec;
}
std::filesystem::path paths_file(const ProjectSession& project){
    return project.root()/"ProjectSettings"/"AndroidToolchain.paths";
}
}
bool validate_android_tool_paths(const AndroidToolPaths& p,std::string* error){
    if(!real_dir(p.sdk) || !real_file(p.sdk/"platforms"/"android-35"/"android.jar"))
        return tool_error(error,"Android SDK missing android-35/android.jar");
    if(!real_dir(p.ndk) || !real_file(p.ndk/"source.properties") ||
       !real_file(p.ndk/"build"/"cmake"/"android.toolchain.cmake"))
        return tool_error(error,"NDK missing source.properties or CMake toolchain");
    {
        std::ifstream source(p.ndk/"source.properties");
        std::string line;
        bool found=false;
        while(std::getline(source,line)){
            if(line.find("Pkg.Revision")!=std::string::npos &&
               line.find("27.")!=std::string::npos)found=true;
        }
        if(!found)return tool_error(error,"NDK revision 27.x required");
    }
#ifdef _WIN32
    const auto java=p.jdk/"bin"/"java.exe";
#else
    const auto java=p.jdk/"bin"/"java";
#endif
    if(!real_dir(p.jdk)||!real_file(java))
        return tool_error(error,"JDK missing bin/java executable");
    if(error)error->clear();
    return true;
}
bool save_android_tool_paths(const ProjectSession& project,
                             const AndroidToolPaths& paths,std::string* error){
    if(!project.is_open())return tool_error(error,"open a project first");
    if(!validate_android_tool_paths(paths,error))return false;
    const auto target=paths_file(project);
    std::error_code ec;
    std::filesystem::create_directories(target.parent_path(),ec);
    if(ec || std::filesystem::is_symlink(target,ec) || ec)
        return tool_error(error,"unsafe Android toolchain settings target");
    const auto temp=target.string()+".tmp";
    if(std::filesystem::is_symlink(temp,ec)||ec)
        return tool_error(error,"unsafe Android settings temporary path");
    {
        std::ofstream stream(temp,std::ios::binary|std::ios::trunc);
        if(!stream)return tool_error(error,"cannot write Android tool paths");
        stream<<"NENGINE_ANDROID_PATHS 1\n"
              <<std::quoted(paths.sdk.generic_string())<<"\n"
              <<std::quoted(paths.ndk.generic_string())<<"\n"
              <<std::quoted(paths.jdk.generic_string())<<"\n";
        if(!stream.good())return tool_error(error,"Android tool path write failed");
    }
    std::filesystem::rename(temp,target,ec);
    if(ec){
        std::filesystem::remove(temp);
        return tool_error(error,"cannot publish Android tool settings");
    }
    if(error)error->clear();
    return true;
}
bool load_android_tool_paths(const ProjectSession& project,
                             AndroidToolPaths& paths,std::string* error){
    if(!project.is_open())return tool_error(error,"open a project first");
    const auto target=paths_file(project);
    std::error_code ec;
    if(!std::filesystem::exists(target,ec)&&!ec){
        paths={};if(error)error->clear();return true;
    }
    if(ec || std::filesystem::is_symlink(target,ec) || ec ||
       !std::filesystem::is_regular_file(target,ec) || ec ||
       std::filesystem::file_size(target,ec)>16384u || ec)
        return tool_error(error,"unsafe Android tool path settings");
    std::ifstream file(target,std::ios::binary);
    std::string magic;
    unsigned version=0;
    AndroidToolPaths loaded;
    std::string sdk,ndk,jdk,tail;
    if(!(file>>magic>>version)||magic!="NENGINE_ANDROID_PATHS"||version!=1 ||
       !(file>>std::quoted(sdk)>>std::quoted(ndk)>>std::quoted(jdk)) ||
       (file>>tail) || sdk.size()>4096u||ndk.size()>4096u||jdk.size()>4096u)
        return tool_error(error,"malformed Android tool path settings");
    loaded.sdk=sdk;loaded.ndk=ndk;loaded.jdk=jdk;
    paths=std::move(loaded);
    if(error)error->clear();
    return true;
}
}
