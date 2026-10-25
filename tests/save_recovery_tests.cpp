#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include "nengine/player/save_data.hpp"
int main(){
  using namespace nengine::player;
  std::error_code ec;
  const auto root=std::filesystem::temp_directory_path()/"nengine-save-recovery-20261024";
  std::filesystem::remove_all(root,ec);
  // The initial save must work even when neither the root nor Backup exists.
  if(std::filesystem::exists(root,ec)||ec)return 8;
  GameSaveStore saves(root);
  GameSave one{1,{1,2,3}},two{2,{4,5,6}};
  std::string error;
  if(!saves.write("Continue",one,&error)||!saves.write("Continue",two,&error)){
    std::cerr<<error<<"\n";return 1;
  }
  if(!std::filesystem::is_directory(root,ec)||ec||
     !std::filesystem::is_directory(root/"Backup",ec)||ec)return 9;
  GameSave stored,backup;
  if(!saves.read("Continue",stored,&error)||stored.version!=2 ||
     !saves.read_backup("Continue",backup,&error)||backup.version!=1 ||
     backup.bytes!=one.bytes)return 2;
  // Simulate process crash corrupting latest file. The last-good backup
  // remains intact and is never automatically overwritten or loaded silently.
  {std::fstream out(root/"Continue.nsave",std::ios::in|std::ios::out|std::ios::binary);
   out.seekp(24);out.put('x');}
  const auto original=stored;
  if(saves.read("Continue",stored,&error)||stored.bytes!=original.bytes||
     error.find("read_backup()")==std::string::npos)return 3;
  if(!saves.read_backup("Continue",backup,&error)||backup.version!=1||
     backup.bytes!=one.bytes)return 4;
  // Write healthy recovery content back without clobbering last-good backup.
  if(!saves.write("Continue",backup,&error)||
     !saves.read("Continue",stored,&error)||stored.bytes!=one.bytes)return 5;
  if(!saves.read_backup("Continue",backup,&error)||backup.bytes!=one.bytes)return 6;
  GameSave sentinel{8,{8}};
  if(saves.read_backup("Nonexistent",sentinel,&error)||
     sentinel.bytes!=std::vector<std::uint8_t>{8}||
     error.find("external copy")==std::string::npos)return 7;
  std::filesystem::remove_all(root,ec);
  std::cout<<"NEngineGameSaveRecoveryTests PASS\n";
  return 0;
}
