// Round 2 acceptance: ASSET-004, ASSET-011, ASSET-012, ASSET-014.
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "nengine/assets/gltf_sidecars.hpp"
#include "nengine/assets/asset_database.hpp"

namespace {
int failures=0;
void check(bool b,const char* message){if(!b){++failures;std::cerr<<"FAIL: "<<message<<"\n";}}
void write(const std::filesystem::path& p,const std::string& text) {
    std::filesystem::create_directories(p.parent_path());
    std::ofstream out(p,std::ios::binary|std::ios::trunc);
    out<<text;
}
void external_resource_safety(const std::filesystem::path& root) {
    using namespace nengine::assets;
    auto model=root/"Models"/"sample.gltf";
    auto local=root/"Models"/"local.bin";
    auto secret=root/"private.bin";
    write(local,"BIN_DATA");
    write(secret,"OUTSIDE");
    write(model,R"({"buffers":[{"uri":"local.bin"}]})");
    std::vector<GltfSidecar> deps;
    std::string error;
    check(collect_gltf_sidecars(model,deps,&error)&&deps.size()==1&&
          deps.front().relative_path.generic_string()=="local.bin",
          "ASSET-004: resource within source directory is accepted");
    const auto last_valid=deps;
    for(const auto& uri:{"../private.bin","%2E%2E/private.bin",
                         "/absolute.bin","C:%5cWindows%5cwin.ini",
                         "https://evil.test/a.bin","local.bin?x=1",
                         "local.bin#fragment"}) {
        write(model,std::string(R"({"buffers":[{"uri":")")+uri+R"("}]})");
        check(!collect_gltf_sidecars(model,deps,&error)&&!error.empty()&&
              deps.size()==last_valid.size()&&
              deps[0].relative_path==last_valid[0].relative_path,
              "ASSET-004: traversal, schemes and malformed URI are rejected transactionally");
    }
    write(model,R"({"buffers":[{"uri":"local.bin"}]})");
}
void size_limits(const std::filesystem::path& root) {
    using namespace nengine::assets;
    auto gltf=root/"Models"/"oversized.gltf";
    std::string block(1024*1024,'x');
    {
        std::ofstream out(gltf,std::ios::binary|std::ios::trunc);
        for(int n=0;n<33;++n)out<<block;
    }
    std::vector<GltfSidecar> deps;
    std::string error;
    check(!collect_gltf_sidecars(gltf,deps,&error)&&
          error.find("32 MiB")!=std::string::npos,
          "ASSET-011: oversized glTF model JSON rejected before parsing");
    auto meta_root=root/"TooLarge";
    auto source=meta_root/"oversized.png";
    write(source,"PNG");
    auto meta=std::filesystem::path(source.string()+".meta");
    write(meta,std::string(9000,'X'));
    AssetDatabase db(meta_root);
    auto scanned=db.scan(false);
    check(db.size()==0 && !scanned.messages.empty(),
          "ASSET-011: oversized metadata rejected before import/repair in readonly scan");
}
void stable_guid(const std::filesystem::path& root) {
    using namespace nengine::assets;
    auto assets=root/"Stable";
    auto source=assets/"Character"/"hero.obj";
    write(source,"v 0 0 0\n");
    AssetDatabase db(assets);
    const auto first=db.scan(true);
    const auto* initial=db.find_relative("Character/hero.obj");
    check(first.meta_created==1&&initial&&initial->guid.valid(),
          "ASSET-012: source indexed with stable GUID metadata");
    if(!initial)return;
    auto guid=initial->guid;
    write(source,"v 1 0 0\n");
    db.scan(true);
    const auto* reimported=db.find_relative("Character/hero.obj");
    check(reimported&&reimported->guid==guid,
          "ASSET-012: changed/reimported source retains GUID");
    const auto moved=assets/"Character"/"moved.obj";
    std::filesystem::rename(source,moved);
    std::filesystem::rename(source.string()+".meta",moved.string()+".meta");
    db.scan(true);
    const auto* renamed=db.find_relative("Character/moved.obj");
    check(renamed&&renamed->guid==guid&&db.find_relative("Character/hero.obj")==nullptr,
          "ASSET-012: rename with sidecar metadata preserves GUID");
    db.clear();db.scan(false);
    const auto* restarted=db.find_relative("Character/moved.obj");
    check(restarted&&restarted->guid==guid,
          "ASSET-012: fresh read-only scan restores GUID");
}
void hostile_symlinks(const std::filesystem::path& root) {
    using namespace nengine::assets;
    auto safe=root/"Sandbox";
    auto outside=root/"Outside"/"private.png";
    write(outside,"external asset data");
    write(safe/"inside.png","internal asset");
    std::error_code ec;
    auto link=safe/"linked.png";
    std::filesystem::create_symlink(outside,link,ec);
    if(!ec) {
        AssetDatabase db(safe);
        auto result=db.scan(true);
        check(db.find_relative("linked.png")==nullptr &&
              !std::filesystem::exists(link.string()+".meta") &&
              db.find_relative("inside.png")!=nullptr,
              "ASSET-014: malicious symlink asset cannot be indexed or create meta");
        bool warned=false;
        for(const auto& msg:result.messages)
            if(msg.message.find("symlink")!=std::string::npos)warned=true;
        check(warned,"ASSET-014: symlink rejection produces actionable warning");
    } else {
        std::cout<<"ASSET-014: symlink fixture unavailable on this host; direct URI tests still run\n";
    }
    auto model=safe/"outside.gltf";
    write(model,R"({"buffers":[{"uri":"linked.png"}]})");
    std::vector<GltfSidecar> old;
    std::string error;
    if(std::filesystem::exists(link))
        check(!collect_gltf_sidecars(model,old,&error)&&!error.empty(),
              "ASSET-014: glTF resolver rejects symlink escape");
}
}
int main() {
 std::error_code ec;
 auto root=std::filesystem::temp_directory_path()/"nengine-acceptance-assets-20261023";
 std::filesystem::remove_all(root,ec);
 std::filesystem::create_directories(root,ec);
 external_resource_safety(root);size_limits(root);stable_guid(root);hostile_symlinks(root);
 std::filesystem::remove_all(root,ec);
 if(!failures)std::cout<<"RoundTwoAssetAcceptanceTests PASS\n";
 return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
