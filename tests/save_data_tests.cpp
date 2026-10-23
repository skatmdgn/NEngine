#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "nengine/player/save_data.hpp"

namespace {
int failures=0;
void check(bool ok,const char* message){
    if(!ok){++failures;std::cerr<<"FAIL: "<<message<<"\n";}
}
}
int main(){
    using namespace nengine::player;
    std::error_code ignored;
    const auto root=std::filesystem::temp_directory_path()/"nengine-save-data-acceptance-20261023";
    std::filesystem::remove_all(root,ignored);
    std::filesystem::create_directories(root,ignored);
    GameSaveStore store(root);
    GameSave save{1,{1,2,3,4,5}};
    std::string error;
    check(store.write("slot_01",save,&error),
          "BUILD-006: new valid save commits to user-data store");
    GameSave result;
    check(store.read("slot_01",result,&error)&&result.bytes==save.bytes&&
          result.version==1,
          "BUILD-006: slot binary data roundtrips exactly");
    check(!store.write("../../bad",save,&error) &&
          !std::filesystem::exists(root.parent_path()/"bad.nsave"),
          "BUILD-006: save path traversal rejected");
    check(!store.write("unsafe name",save,&error),
          "BUILD-006: unsafe slot name rejected");
    GameSave upgraded;
    check(store.read_migrated("slot_01",2,
        [](std::uint32_t before,std::uint32_t after,std::vector<std::uint8_t>& bytes){
            if(before!=1 || after!=2)return false;
            bytes.push_back(9);
            return true;
        },upgraded,&error) &&
        upgraded.version==2 && upgraded.bytes.size()==6,
        "BUILD-006: old data migrates in memory to latest application version");
    check(!store.read_migrated("slot_01",2,
        [](std::uint32_t,std::uint32_t,std::vector<std::uint8_t>&){return false;},
        upgraded,&error) && upgraded.version==2 && upgraded.bytes.size()==6,
        "BUILD-006: failed migration never overwrites current caller data");
    check(!store.read_migrated("slot_01",0,{},result,&error)&&
          result.bytes==save.bytes,
          "BUILD-006: invalid target version rejected");
    auto p=root/"slot_01.nsave";
    {
        std::fstream file(p,std::ios::in|std::ios::out|std::ios::binary);
        file.seekp(24);
        file.put('X');
    }
    check(!store.read("slot_01",result,&error)&&result.bytes==save.bytes,
          "BUILD-006: corrupted save CRC blocks loading without mutating output");
    check(store.write("slot_01",save,&error)&&store.read("slot_01",result,&error),
          "BUILD-006: corrupted data can be deliberately replaced by healthy save");
    GameSave oversized{1,std::vector<std::uint8_t>(4*1024*1024+1,0)};
    check(!store.write("slot_01",oversized,&error)&&
          store.read("slot_01",result,&error)&&result.bytes==save.bytes,
          "BUILD-006: rejected oversized payload cannot erase valid save");
    check(!GameSaveStore::user_data_directory("../escape") &&
          GameSaveStore::user_data_directory("MyGame").has_value(),
          "BUILD-006: OS user data directories validated by game ID");
    for(const auto& entry:std::filesystem::directory_iterator(root)){
        check(entry.path().filename().string().find(".tmp")==std::string::npos,
              "BUILD-006: transactional writes leave no staging files");
    }
    std::filesystem::remove_all(root,ignored);
    if(!failures)std::cout<<"NEngineGameSaveTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
