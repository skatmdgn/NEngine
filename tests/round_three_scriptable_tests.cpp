#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include "nengine/assets/scriptable_data.hpp"

int main(){
    using namespace nengine::assets;
    int failures=0;
    const auto check=[&](bool good,const char* why){
        if(!good){++failures;std::cerr<<"FAIL: "<<why<<"\n";}
    };
    ScriptableData data;
    data.values["Settings.GameTitle"]=std::string("한글 테스트");
    data.values["Settings.Volume"]=0.25;
    data.values["Settings.IsHard"]=true;
    data.values["Settings.Lives"]=std::int64_t{5};
    std::string error;
    std::stringstream stream;
    check(write_scriptable_data(stream,data,&error) && error.empty(),
          "SCN-005: emits deterministic versioned ScriptableData");
    const auto bytes=stream.str();
    check(bytes.find("Settings.GameTitle")<bytes.find("Settings.Lives") &&
          bytes.find("Settings.Lives")<bytes.find("Settings.Volume"),
          "SCN-005: stable lexical order independent of unordered map insertion");
    ScriptableData restored;
    check(read_scriptable_data(stream,restored,&error) &&
          restored.values.size()==data.values.size() &&
          std::get<std::string>(restored.values.at("Settings.GameTitle"))=="한글 테스트" &&
          std::get<double>(restored.values.at("Settings.Volume"))==0.25 &&
          std::get<std::int64_t>(restored.values.at("Settings.Lives"))==5 &&
          std::get<bool>(restored.values.at("Settings.IsHard")),
          "SCN-005: bool/int/float/UTF-8 string survive roundtrip");
    for(const auto& bad:{
        "NENGINE_SCRIPTABLE 1\nENTRIES 2\nENTRY \"x\" BOOL 1\nEND_SCRIPTABLE\n",
        "NENGINE_SCRIPTABLE 1\nENTRIES 1\nENTRY \"../../evil\" INT 9\nEND_SCRIPTABLE\n",
        "NENGINE_SCRIPTABLE 1\nENTRIES 1\nENTRY \"x\" BOOL 7\nEND_SCRIPTABLE\n",
        "NENGINE_SCRIPTABLE 1\nENTRIES 1\nENTRY \"x\" STRING \"unterminated\n",
        "NENGINE_SCRIPTABLE 1\nENTRIES 2\nENTRY \"x\" INT 1\nENTRY \"x\" INT 2\nEND_SCRIPTABLE\n",
        "NENGINE_SCRIPTABLE 1\nENTRIES 1025\nEND_SCRIPTABLE\n",
        "NENGINE_SCRIPTABLE 1\nENTRIES 0\nEND_SCRIPTABLE\njunk"
    }){
        std::istringstream corrupted{bad};
        check(!read_scriptable_data(corrupted,restored,&error) &&
              restored.values.size()==4 && !error.empty(),
              "SCN-005: malformed, traversal-key, duplicate and trailing input fails transactionally");
    }
    ScriptableData impossible=data;
    impossible.values["Settings.Volume"]=std::numeric_limits<double>::infinity();
    std::stringstream rejected;
    check(!write_scriptable_data(rejected,impossible,&error)&&rejected.str().empty(),
          "SCN-005: invalid float not partially published");
    std::cout<<(failures?"SCN-005 failed":"SCN-005 PASS")<<"\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
