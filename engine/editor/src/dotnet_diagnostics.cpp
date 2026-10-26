#include "nengine/editor/dotnet_diagnostics.hpp"
#include <regex>
#include <sstream>
#include <system_error>
namespace nengine::editor {
std::vector<DotnetDiagnostic> parse_dotnet_diagnostics(
    std::string_view output,const std::filesystem::path& project_root) {
    std::vector<DotnetDiagnostic> found;
    if(output.size()>2u*1024u*1024u)return found;
    std::error_code ec;
    const auto root=std::filesystem::weakly_canonical(project_root,ec);
    if(ec||!root.is_absolute())return found;
    const std::regex pattern(
        R"(^(.+)\(([0-9]+),([0-9]+)\):[ ]*(error|warning)[ ]+([A-Za-z]+[0-9]+):[ ]*(.*)$)");
    std::istringstream stream(std::string{output});
    std::string line;
    while(std::getline(stream,line)&&found.size()<512u) {
        if(!line.empty()&&line.back()=='\r')line.pop_back();
        std::smatch match;
        if(!std::regex_match(line,match,pattern))continue;
        std::uint64_t row=0,column=0;
        try { row=std::stoull(match[2].str());column=std::stoull(match[3].str()); }
        catch(...){continue;}
        if(row==0||column==0||row>10000000||column>10000000)continue;
        const auto source_name=std::filesystem::u8path(match[1].str());
        const auto absolute=source_name.is_absolute()?source_name:root/source_name;
        const auto source=std::filesystem::weakly_canonical(absolute,ec);
        if(ec||!std::filesystem::is_regular_file(source,ec)||ec) {ec.clear();continue;}
        auto ri=root.begin(),si=source.begin();
        for(;ri!=root.end()&&si!=source.end()&&*ri==*si;++ri,++si){}
        if(ri!=root.end())continue;
        auto message=match[6].str();
        const auto project_suffix=message.rfind(" [");
        if(project_suffix!=std::string::npos&&!message.empty()&&message.back()==']')
            message.erase(project_suffix);
        found.push_back({match[4].str()=="error",source,
          static_cast<std::uint32_t>(row),static_cast<std::uint32_t>(column),
          match[5].str(),std::move(message)});
    }
    return found;
}
}
