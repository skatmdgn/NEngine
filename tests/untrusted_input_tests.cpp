#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "nengine/assets/gltf_sidecars.hpp"
#include "nengine/assets/scriptable_data.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/input/input_state.hpp"
#include "nengine/render/tilemap.hpp"

int main() {
    int failures=0;
    auto expect=[&](bool success,const char* what){
        if(!success){++failures;std::cerr<<"FAIL: "<<what<<"\n";}
    };
    std::minstd_rand random{0xBADC0DE};
    std::string error;
    nengine::core::SceneData prior_scene;
    prior_scene.name="Unchanged";
    nengine::render::Tilemap prior_tiles;
    prior_tiles.resize(2,2);
    nengine::assets::ScriptableData prior_config;
    prior_config.values["safe"]=std::int64_t{4};
    nengine::input::ActionMap prior_actions;
    prior_actions.bind("Stay",nengine::input::Key::Space);

    // Deterministic untrusted byte corpus across unrelated production parsers.
    // Expected behavior is rejection *without destroying the caller state*.
    for(int iteration=0;iteration<512;++iteration) {
        std::string bytes;
        const int length=static_cast<int>(random()%1200);
        bytes.reserve(length);
        for(int n=0;n<length;++n)
            bytes.push_back(static_cast<char>(random()%256));
        {
            std::istringstream input{bytes};
            nengine::core::SceneData target=prior_scene;
            const bool ok=nengine::core::SceneSerializer::read(input,target,&error);
            expect(!ok && target.name=="Unchanged" && target.objects.empty(),
                   "QA-016: untrusted Scene parsing transactional");
        }
        {
            std::istringstream input{bytes};
            auto target=prior_tiles;
            const bool ok=nengine::render::read_tilemap(input,target,&error);
            expect(!ok&&target.width==2&&target.height==2,
                   "QA-016: untrusted Tilemap parsing transactional");
        }
        {
            std::istringstream input{bytes};
            auto target=prior_config;
            const bool ok=nengine::assets::read_scriptable_data(input,target,&error);
            expect(!ok&&target.values.size()==1&&target.values.contains("safe"),
                   "QA-016: untrusted ScriptableData parsing transactional");
        }
        {
            auto target=prior_actions;
            const bool ok=nengine::input::ActionMap::deserialize(bytes,target,&error);
            expect(!ok&&target.action_count()==1&&target.contains("Stay"),
                   "QA-016: untrusted ActionMap parsing transactional");
        }
    }
    std::error_code ec;
    auto root=std::filesystem::temp_directory_path()/"nengine-untrusted-fixtures-20261023";
    std::filesystem::remove_all(root,ec);
    std::filesystem::create_directories(root,ec);
    auto model=root/"corrupt.gltf";
    std::vector<nengine::assets::GltfSidecar> sidecars;
    for(const auto& payload:{
        R"({"buffers":[{"uri":"../private.bin"}]})",
        R"({"buffers":[{"uri":"%2e%2e/secret.bin"}]})",
        R"({"buffers":[{"uri":"/absolute.bin"}]})",
        R"({"buffers":[{"uri":"https://example.net/evil.bin"}]})",
        R"({"buffers":[{"uri":"C:/Windows/system32/a.bin"}]})",
        R"({"images":[{"uri":"data:text/plain,evil"}]})",
        R"({"asset":{"version":"2.0"},"buffers":[{"uri":"missing.bin"}]})"
    }) {
        {std::ofstream file(model,std::ios::binary|std::ios::trunc);file<<payload;}
        const bool good=nengine::assets::collect_gltf_sidecars(model,sidecars,&error);
        // Valid embedded data URIs are permitted, but must never generate
        // any filesystem dependency or touch external project files.
        expect(!good || sidecars.empty(),
               "QA-016: hostile glTF URI cannot escape dependency sandbox");
    }
    std::filesystem::remove_all(root,ec);
    if(!failures)std::cout<<"NEngineUntrustedInputTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
