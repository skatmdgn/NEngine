#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "nengine/assets/asset_importer.hpp"
#include "nengine/assets/builtin_processors.hpp"
#include "nengine/assets/import_pipeline.hpp"
#include "nengine/assets/gltf_sidecars.hpp"
#include "nengine/render/asset_resources.hpp"
#include "nengine/render/gltf_mesh.hpp"

namespace {int failures=0;void check(bool b,const char* what){if(!b){++failures;std::cerr<<"FAIL: "<<what<<"\n";}}}
int main(){
    using namespace nengine;
    std::error_code ec;
    const auto root=std::filesystem::temp_directory_path()/
        "nengine-multiple-external-gltf-20261023";
    std::filesystem::remove_all(root,ec);
    auto assets_root=root/"Assets";
    const auto model=assets_root/"models"/"dual.gltf";
    std::filesystem::create_directories(model.parent_path()/"images",ec);
    std::ofstream pos(model.parent_path()/"pos.bin",std::ios::binary);
    const std::array<float,9> xyz{0,0,0,1,0,0,0,1,0};
    pos.write(reinterpret_cast<const char*>(xyz.data()),sizeof(xyz));
    pos.close();
    {std::ofstream indices(model.parent_path()/"indices.bin",std::ios::binary);
     const std::array<std::uint16_t,3> order{0,1,2};
     indices.write(reinterpret_cast<const char*>(order.data()),sizeof(order));}
const std::array<std::uint8_t,74> png_bytes{
                0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au,
                0x00u, 0x00u, 0x00u, 0x0Du, 0x49u, 0x48u, 0x44u, 0x52u,
                0x00u, 0x00u, 0x00u, 0x02u, 0x00u, 0x00u, 0x00u, 0x01u,
                0x08u, 0x06u, 0x00u, 0x00u, 0x00u, 0xF4u, 0x22u, 0x7Fu,
                0x8Au, 0x00u, 0x00u, 0x00u, 0x11u, 0x49u, 0x44u, 0x41u,
                0x54u, 0x78u, 0xDAu, 0x63u, 0xF8u, 0xCFu, 0xC0u, 0xF0u,
                0x9Fu, 0xE1u, 0x3Fu, 0xC3u, 0x7Fu, 0x00u, 0x10u, 0xF8u,
                0x03u, 0xFDu, 0x3Cu, 0x9Fu, 0xE6u, 0xF6u, 0x00u, 0x00u,
                0x00u, 0x00u, 0x49u, 0x45u, 0x4Eu, 0x44u, 0xAEu, 0x42u,
                0x60u, 0x82u
            };
    for(auto name:{"red.png","blue.png"}){
        std::ofstream png(model.parent_path()/"images"/name,std::ios::binary);
        png.write(reinterpret_cast<const char*>(png_bytes.data()),png_bytes.size());
    }
    {
    std::ofstream gltf(model,std::ios::binary);
    gltf<<R"json({
    "asset":{"version":"2.0"},
    "buffers":[
      {"uri":"pos.bin","byteLength":36},
      {"uri":"indices.bin","byteLength":6}
    ],
    "bufferViews":[
      {"buffer":0,"byteOffset":0,"byteLength":36},
      {"buffer":1,"byteOffset":0,"byteLength":6}
    ],
    "accessors":[
      {"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},
      {"bufferView":1,"componentType":5123,"count":3,"type":"SCALAR"}
    ],
    "images":[{"uri":"images/red.png"},{"uri":"images/blue.png"}],
    "textures":[{"source":0},{"source":1}],
    "materials":[
      {"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}},
      {"pbrMetallicRoughness":{"baseColorTexture":{"index":1}}}
    ],
    "meshes":[{"primitives":[
      {"attributes":{"POSITION":0},"indices":1,"material":0},
      {"attributes":{"POSITION":0},"indices":1,"material":1}
    ]}],
    "nodes":[{"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0
    })json";
    }
    std::vector<assets::GltfSidecar> deps;
    std::string error;
    check(assets::collect_gltf_sidecars(model,deps,&error)&&deps.size()==4,
          "ASSET-003: discover two separate external BIN and two external image references");
    assets::AssetRecord asset;
    asset.guid=assets::AssetGuid::generate();
    asset.source_path=model;
    asset.importer_id="NEngine.Model";
    asset.file_size=std::filesystem::file_size(model);
    asset.write_stamp=1;
    assets::ImporterRegistry registry;
    registry.register_importer({"NEngine.Model",1,{".gltf"},false});
    assets::AssetImportPipeline pipeline;
    pipeline.register_processor("NEngine.Model",assets::model_source_importer);
    const auto cache=root/"Cache";
    auto result=pipeline.import(asset,registry,cache);
    auto artifacts=pipeline.cached_artifacts(asset,registry,cache);
    check(result.success&&artifacts&&artifacts->artifacts.size()==6,
          "ASSET-003: import pipeline stages glTF, descriptor, both BIN and both PNG");
    if(artifacts){
        auto resolved=render::resolve_model_asset(asset.guid,*artifacts,&error);
        render::MeshData mesh;
        check(resolved && render::decode_gltf_mesh(*resolved,mesh,&error)&&mesh.valid()&&
              mesh.indices.size()==6 && mesh.vertices.size()>=3,
              "ASSET-003: separate buffers decoded as geometry with two material primitives");
        for(std::size_t material=0;material<2;++material){
            render::DecodedTextureData decoded;
            check(resolved&&render::decode_gltf_material_base_color_texture(
                      *resolved,material,decoded,&error)&&decoded.valid()&&
                  decoded.rgba8.size()==8u,
                  "ASSET-003: external image decoded into usable material texture");
        }
    }
    auto missing=model.parent_path()/"indices.bin";
    std::filesystem::remove(missing,ec);
    auto prior=deps;
    error.clear();
    check(!assets::collect_gltf_sidecars(model,deps,&error)&&
          error.find("indices.bin")!=std::string::npos&&
          error.find("reimport")!=std::string::npos&&
          deps.size()==prior.size(),
          "ASSET-003: missing second external BIN diagnosed without partial dependency publication");
    std::filesystem::remove_all(root,ec);
    if(!failures)std::cout<<"NEngineMultiExternalGltfTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
