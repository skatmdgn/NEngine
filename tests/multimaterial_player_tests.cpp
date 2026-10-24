#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include "nengine/assets/asset_guid.hpp"
#include "nengine/core/scene.hpp"
#include "nengine/player/exporter.hpp"
#include "nengine/player/runtime.hpp"
#include "nengine/player/subassets.hpp"
#include "nengine/render/components.hpp"
#include "nengine/render/registration.hpp"
#include "nengine/render/asset_resources.hpp"
#include "nengine/render/gltf_mesh.hpp"
#include "nengine/render/decoded_mesh.hpp"
#include "nengine/render/obj_mesh.hpp"

namespace {
int failures=0;
void check(bool good,const char* why) {
    if(!good){++failures;std::cerr<<"FAIL "<<why<<"\n";}
}
}
int main() {
    using namespace nengine;
    std::error_code ec;
    auto root=std::filesystem::temp_directory_path()/"nengine-multi-material-export-20261024";
    std::filesystem::remove_all(root,ec);
    auto project=root/"Project";
    auto model_path=project/"Assets"/"Models"/"two.obj";
    std::filesystem::create_directories(model_path.parent_path(),ec);
    std::filesystem::create_directories(project/"Assets"/"Scenes",ec);
    const auto model_guid=assets::AssetGuid::generate();
    {std::ofstream obj(model_path);
     obj<<"mtllib two.mtl\n"
        <<"v 0 0 0\nv 1 0 0\nv 0 1 0\nv 1 1 0\n"
        <<"usemtl Red\nf 1 2 3\n"
        <<"usemtl Blue\nf 2 4 3\n";}
    {std::ofstream mtl(model_path.parent_path()/"two.mtl");
     mtl<<"newmtl Red\nKd 1 0 0\nd 1.0\n"
        <<"newmtl Blue\nKd 0 0 1\nd 1.0\n";}
    {std::ofstream meta(model_path.string()+".meta");
     meta<<"NENGINE_META 1\nGUID "<<model_guid.to_string()
         <<"\nIMPORTER \"NEngine.Model\"\nEND_META\n";}
    core::World authored;
    auto mesh_entity=authored.create("Two-material mesh");
    auto* renderer=authored.add_component<render::MeshRenderer>(
        mesh_entity,render::mesh_renderer_type());
    if(!renderer)return 2;
    renderer->mesh=model_guid;
    core::ComponentSerializationRegistry codecs;
    std::string error;
    check(render::register_component_serializers(codecs),
          "ASSET-010: MeshRenderer codec available");
    core::SceneData scene;
    check(core::SceneSerializer::capture_checked(authored,scene,"MultiMaterial",&error,&codecs),
          "ASSET-010: source Scene captures model GUID");
    check(core::SceneSerializer::save_file(scene,project/"Assets"/"Scenes"/"Main.nscene",&error),
          "ASSET-010: source Scene saved");
    std::vector<std::set<std::string>> identities;
    for(int iteration=0;iteration<2;++iteration){
        player::ExportSettings cfg;
        cfg.project_root=project;
        cfg.startup_scene="Assets/Scenes/Main.nscene";
        cfg.destination=root/(iteration?"Export2":"Export1");
        cfg.copy_supported_assets=true;
        player::ExportReport report;
        check(player::export_player_data(cfg,report,&error) &&
              report.cooked_models==1 &&
              report.generated_materials==2 &&
              report.generated_textures==2,
              "ASSET-009/010: model export cooks all two materials and corresponding textures");
        const auto folder=cfg.destination/"Content"/"Cooked"/model_guid.to_string();
        std::vector<player::CookedSubasset> parts;
        check(player::read_model_subassets(folder,parts,&error) &&
              parts.size()==4,
              "ASSET-009: all generated GUID subassets survive on-disk catalog");
        std::set<std::string> ids;
        std::size_t material_count=0,texture_count=0;
        player::PlayerRuntime runtime;
        check(runtime.load(cfg.destination,&error)&&runtime.entity_count()==1,
              "ASSET-010: fresh bundled Player loads multi-material Scene");
        for(const auto& item:parts){
            ids.insert(item.guid.to_string());
            if(item.importer_id=="NEngine.Material")++material_count;
            if(item.importer_id=="NEngine.Texture")++texture_count;
            auto cooked=runtime.assets().cooked_artifacts(item.guid);
            check(cooked.has_value()&&cooked->artifacts.size()>0,
                  "ASSET-010: each generated material/texture resolves in Player by GUID");
        }
        check(ids.size()==4&&material_count==2&&texture_count==2,
              "ASSET-009: four unique stable GUIDs for both material slots");
        auto model=runtime.assets().cooked_artifacts(model_guid);
        check(model.has_value(),"ASSET-010: bundled model mesh is loadable by GUID");
        if(model){
            auto resolved=render::resolve_model_asset(model_guid,*model,&error);
            render::MeshData decoded;
            check(resolved.has_value()&&
                  render::decode_obj_mesh(*resolved,decoded,&error)&&
                  decoded.valid()&&decoded.indices.size()==6,
                  "ASSET-010: exported model geometry includes both triangles");
        }
        if(auto* comp=runtime.world().get_component<render::MeshRenderer>(
            runtime.world().entities()[0],render::mesh_renderer_type()))
            check(comp->mesh==model_guid,
                  "ASSET-010: Player Scene model GUID preserved");
        else check(false,"ASSET-010: model renderer is missing from Player");
        identities.push_back(std::move(ids));
    }
    check(identities.size()==2 &&identities[0]==identities[1],
          "ASSET-009: repeated export reproduces identical material and texture GUIDs");
    std::filesystem::remove_all(root,ec);
    if(!failures)std::cout<<"NEngineMultiMaterialPlayerTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
