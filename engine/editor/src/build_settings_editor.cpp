#include "nengine/editor/build_settings_editor.hpp"
#include <algorithm>
#include <fstream>
#include <set>
#include <iomanip>
#include <system_error>
#include <utility>
#ifdef _WIN32
#include <windows.h>
#endif
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
    if(ec)return tool_error(error,"cannot create Android settings directory");
    if(std::filesystem::exists(target,ec)){
        if(ec||std::filesystem::is_symlink(target,ec)||ec)
            return tool_error(error,"unsafe Android toolchain settings target");
    }else if(ec)return tool_error(error,"cannot stat Android settings target");
    const auto temp=target.string()+".tmp";
    if(std::filesystem::exists(temp,ec)){
        if(ec||std::filesystem::is_symlink(temp,ec)||ec)
            return tool_error(error,"unsafe Android settings temporary path");
    }else if(ec)return tool_error(error,"cannot stat Android settings temp path");
    {
        std::ofstream stream(temp,std::ios::binary|std::ios::trunc);
        if(!stream)return tool_error(error,"cannot write Android tool paths");
        stream<<"NENGINE_ANDROID_PATHS 1\n"
              <<std::quoted(paths.sdk.generic_string())<<"\n"
              <<std::quoted(paths.ndk.generic_string())<<"\n"
              <<std::quoted(paths.jdk.generic_string())<<"\n";
        if(!stream.good())return tool_error(error,"Android tool path write failed");
    }
#ifdef _WIN32
    if(!MoveFileExW(std::filesystem::path(temp).c_str(),target.c_str(),
                    MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        std::filesystem::remove(temp);
        return tool_error(error,"cannot atomically publish Android settings");
    }
#else
    std::filesystem::rename(temp,target,ec);
    if(ec){
        std::filesystem::remove(temp);
        return tool_error(error,"cannot publish Android tool settings");
    }
#endif
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

namespace {
bool safe_package_token(std::string_view s){
    return !s.empty()&&s.size()<=128u&&s.front()!='.' &&
           std::all_of(s.begin(),s.end(),[](unsigned char c){
               return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                      (c>='0'&&c<='9')||c=='.'||c=='_'||c=='-';
           });
}
std::filesystem::path packages_path(const ProjectSession& project){
    return project.root()/"Packages"/"managed-packages.txt";
}
}
bool load_managed_packages(const ProjectSession& project,
                           ManagedPackageRows& rows,std::string* error){
    if(!project.is_open())return tool_error(error,"open a project first");
    const auto path=packages_path(project);
    std::error_code ec;
    if(std::filesystem::exists(path,ec)){
        if(ec || std::filesystem::is_symlink(path,ec) || ec ||
           std::filesystem::file_size(path,ec)>65536u || ec)
            return tool_error(error,"unsafe NuGet manifest file");
    }else if(ec)return tool_error(error,"cannot inspect NuGet manifest");
    std::string parse_error;
    const auto parsed=nengine::scripting::ManagedProjectGenerator::
        load_package_manifest(path,&parse_error);
    if(!parse_error.empty())return tool_error(error,parse_error);
    if(parsed.size()>128u)return tool_error(error,"too many NuGet packages");
    std::set<std::string> seen;
    for(const auto& p:parsed){
        if(!safe_package_token(p.id)||!safe_package_token(p.version)||
           !seen.insert(p.id).second)
            return tool_error(error,"invalid or duplicate NuGet package");
    }
    auto sorted=parsed;
    std::sort(sorted.begin(),sorted.end(),[](const auto& a,const auto& b){
        return a.id<b.id;
    });
    rows=std::move(sorted);
    if(error)error->clear();
    return true;
}
bool edit_managed_package(ProjectSession& project,std::string id,
                          std::string version,std::string* error){
    if(!safe_package_token(id))
        return tool_error(error,"invalid NuGet package ID");
    if(!version.empty()&&!safe_package_token(version))
        return tool_error(error,"invalid NuGet package version");
    ManagedPackageRows rows;
    if(!load_managed_packages(project,rows,error))return false;
    rows.erase(std::remove_if(rows.begin(),rows.end(),
        [&](const auto& p){return p.id==id;}),rows.end());
    if(!version.empty())rows.push_back({std::move(id),std::move(version)});
    if(rows.size()>128u)return tool_error(error,"NuGet package limit exceeded");
    std::sort(rows.begin(),rows.end(),[](const auto& a,const auto& b){
        return a.id<b.id;
    });
    auto path=packages_path(project);
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(),ec);
    if(ec)return tool_error(error,"cannot create package manifest directory");
    const auto temp=path.string()+".tmp";
    if(std::filesystem::exists(temp,ec)){
        if(ec||std::filesystem::is_symlink(temp,ec)||ec)
            return tool_error(error,"unsafe NuGet temporary file");
    }else if(ec)return tool_error(error,"cannot inspect NuGet temporary file");
    {
        std::ofstream file(temp,std::ios::binary|std::ios::trunc);
        if(!file)return tool_error(error,"cannot write NuGet manifest");
        for(const auto& p:rows)file<<p.id<<" "<<p.version<<"\n";
        if(!file.good())return tool_error(error,"NuGet manifest write failed");
    }
#ifdef _WIN32
    if(!MoveFileExW(std::filesystem::path(temp).c_str(),path.c_str(),
                    MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        std::filesystem::remove(temp);
        return tool_error(error,"cannot publish NuGet manifest");
    }
#else
    std::filesystem::rename(temp,path,ec);
    if(ec){
        std::filesystem::remove(temp);
        return tool_error(error,"cannot publish NuGet manifest");
    }
#endif
    if(error)error->clear();
    return true;
}
}
