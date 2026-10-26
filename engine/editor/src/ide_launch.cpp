#include "nengine/editor/ide_launch.hpp"
#include <algorithm>
#include <cwctype>
#include <system_error>
namespace nengine::editor {
std::optional<IdeLaunch> prepare_ide_launch(
    std::filesystem::path solution,IdeChoice preferred,
    const std::filesystem::path& executable_override,std::string* error) {
    const auto fail=[&](const char* reason)->std::optional<IdeLaunch>{
        if(error)*error=reason;return std::nullopt;
    };
    std::error_code ec;
    if(!solution.is_absolute()||
       !std::filesystem::is_regular_file(solution,ec)||ec)
        return fail("IDE solution is not an existing absolute file");
    auto ext=solution.extension().wstring();
    std::transform(ext.begin(),ext.end(),ext.begin(),
                   [](wchar_t c){return std::towlower(c);});
    if(ext!=L".sln"&&ext!=L".slnx")
        return fail("IDE accepts only .sln or .slnx solution files");
    IdeLaunch launch;
    launch.solution=std::move(solution);
    launch.working_directory=launch.solution.parent_path();
    if(preferred==IdeChoice::Associated&&executable_override.empty()){
        launch.executable=launch.solution;
        return launch;
    }
    launch.use_file_association=false;
    if(!executable_override.empty()) {
        if(!executable_override.is_absolute()||
           !std::filesystem::is_regular_file(executable_override,ec)||ec)
            return fail("IDE executable override is missing or not absolute");
        launch.executable=executable_override;
    }else if(preferred==IdeChoice::Rider){
        launch.executable=L"rider64.exe";
    }else if(preferred==IdeChoice::VisualStudio){
        launch.executable=L"devenv.exe";
    }else {
        return fail("an IDE executable must be selected");
    }
    // Windows ShellExecuteW handles quoting for path parameters if wrapped
    // once; paths with spaces and Hangul remain UTF-16 end-to-end.
    launch.parameters=L"\""+launch.solution.wstring()+L"\"";
    return launch;
}
} // namespace nengine::editor
