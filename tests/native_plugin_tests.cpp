#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include "nengine/scripting/native_plugin.hpp"
namespace {int failure=0;void ok(bool b,const char* msg) {if(!b){++failure;std::cerr<<"FAIL: "<<msg<<"\n";}}}
int main(int argc,char** argv) {
    if(argc!=2)return 2;
    const auto original=std::filesystem::absolute(std::filesystem::u8path(argv[1]));
    std::error_code ec;
    const auto tmp=std::filesystem::temp_directory_path()/"nengine-native-plugins-20261023";
    std::filesystem::remove_all(tmp,ec);
    const auto plugins=tmp/"Plugins";
    std::filesystem::create_directories(plugins,ec);
    const auto valid=plugins/original.filename();
    std::filesystem::copy_file(original,valid,
       std::filesystem::copy_options::overwrite_existing,ec);
    ok(!ec,"PLUG-007: native fixture copied into Plugins");
    nengine::scripting::NativePlugin lib;
    std::string error;
    ok(lib.open(plugins,valid,&error)&&lib.loaded(),
       "PLUG-007: load native plugin from project Plugins path");
    using Fn=int(*)(int);
    const auto fn=reinterpret_cast<Fn>(lib.function("nengine_plugin_smoke"));
    ok(fn&&fn(35)==42,"PLUG-007: resolve export and execute native library");
    ok(lib.function("missing_export")==nullptr,
       "PLUG-008: unknown export diagnosed without memory corruption");
    ok(!lib.open(plugins,valid,&error)&&!error.empty(),
       "PLUG-008: cannot overwrite live library handle");
    lib.close();
    ok(!lib.loaded()&&lib.function("nengine_plugin_smoke")==nullptr,
       "PLUG-007: unload clears exported functions");
    ok(!lib.open(plugins,original,&error)&&!error.empty(),
       "PLUG-008: absolute library outside Plugins cannot load");
    ok(!lib.open(plugins,tmp/"Plugins"/".."/original.filename(),&error),
       "PLUG-008: path traversal cannot leave sandbox");
    const auto missing=plugins/("missing"+original.extension().string());
    ok(!lib.open(plugins,missing,&error)&&!error.empty(),
       "PLUG-008: missing plugin provides diagnostic");
    auto link=plugins/("shortcut"+original.extension().string());
    std::filesystem::create_symlink(valid,link,ec);
    if(!ec) {
        ok(!lib.open(plugins,link,&error),
           "PLUG-008: symlinked native library is rejected");
    }
    lib.close();
    std::filesystem::remove_all(tmp,ec);
    if(!failure)std::cout<<"NEngineNativePluginTests PASS\n";
    return failure?EXIT_FAILURE:EXIT_SUCCESS;
}
