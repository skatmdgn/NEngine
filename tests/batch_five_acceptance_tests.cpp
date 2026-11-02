// Five-target single-commit acceptance: INPUT-008 is in NEngineInputTests,
// EDIT-016 in NEngineEditorTests; exercise VFX-005, EDTOOL-002, TWOD-005 here.
#include <cmath>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "nengine/core/scene.hpp"
#include "nengine/editor/editor_model.hpp"
#include "nengine/editor/prefab_isolation.hpp"
#include "nengine/editor/sprite_sheet_editor.hpp"
#include "nengine/render/particle.hpp"

namespace {
int failed=0;
void check(bool condition,const char* label) {
    if(!condition){std::cerr<<"FAIL: "<<label<<"\n";++failed;}
}
void prefab() {
    using namespace nengine;
    editor::EditorModel editor;
    core::World input;
    const auto root=input.create("PrefabRoot");
    const auto child=input.create("PrefabChild");
    input.set_parent(child,root);
    core::PrefabData prefab;
    prefab.name="Prefab Fixture";
    prefab.root_local_id=0;
    prefab.template_scene=core::SceneSerializer::capture(
        input,"Prefab Fixture",&editor.component_serialization());
    std::string error;
    const auto before=editor.world().size();
    check(core::PrefabModel::validate(prefab,&error),
          "EDTOOL-002 source prefab validates");
    check(editor.begin_prefab_isolation(prefab,&error) &&
          editor.prefab_isolation_active() &&
          editor.world().size()==2u,
          "EDTOOL-002 begin opens detached prefab hierarchy in editor");
    check(!editor.begin_play_mode(),
          "EDTOOL-002 Play Mode cannot overwrite isolated prefab world");
    editor.world().create("AddedOnlyInPrefab");
    core::PrefabData saved;
    check(editor.end_prefab_isolation(true,&saved,&error) &&
          !editor.prefab_isolation_active() &&
          editor.world().size()==before &&
          saved.template_scene.objects.size()==3u &&
          core::PrefabModel::validate(saved,&error),
          "EDTOOL-002 Save returns validated detached changes without touching Scene");
    check(editor.begin_prefab_isolation(saved,&error),
          "EDTOOL-002 saved prefabs can be reopened");
    editor.world().create("Cancelled");
    check(editor.end_prefab_isolation(false,nullptr,&error) &&
          editor.world().size()==before,
          "EDTOOL-002 Cancel discards isolated changes");
    core::PrefabData invalid=saved;
    invalid.root_local_id=999u;
    check(!editor.begin_prefab_isolation(invalid,&error) &&
          !editor.prefab_isolation_active(),
          "EDTOOL-002 invalid prefab rejected without entering edit mode");
}
void particles() {
    using namespace nengine;
    editor::EditorModel editor;
    const auto entity=editor.world().create("Particle Actor");
    check(editor.component_factories().contains(render::particle_emitter_type()) &&
          editor.component_registry().find(render::particle_emitter_type())!=nullptr,
          "VFX-005 particle component metadata is registered");
    auto* authored=editor.world().add_component<render::ParticleEmitter>(
        entity,render::particle_emitter_type());
    check(authored!=nullptr,"VFX-005 emitter can be attached");
    if(!authored)return;
    authored->initial_burst=3;
    authored->rate_per_second=0.f;
    const auto id=render::particle_emitter_type();
    check(editor.property_access().write(editor.world(),entity,id,"Rate",
             core::PropertyValue{20.0}) &&
          std::abs(authored->rate_per_second-20.f)<1e-5f &&
          !editor.property_access().write(editor.world(),entity,id,"Rate",
             core::PropertyValue{-3.0}) &&
          std::abs(authored->rate_per_second-20.f)<1e-5f,
          "VFX-005 Inspector updates valid rates and rejects malformed values transactionally");
    authored->rate_per_second=0.f;
    check(editor.begin_play_mode() && editor.play_session().runtime_world(),
          "VFX-005 begins detached runtime preview");
    if(!editor.play_session().runtime_world())return;
    auto* runtime=editor.play_session().runtime_world()->get_component<render::ParticleEmitter>(
        entity,id);
    render::ParticleEmitter candidate=*runtime;
    candidate.initial_burst=3;candidate.max_particles=16;
    check(editor.set_play_particle_emitter(entity,candidate) &&
          editor.play_particle_count(entity)==0,
          "VFX-005 Play Mode properties can be changed live");
    editor.step_particle_preview(0.10f);
    check(editor.play_particle_count(entity)==3u,
          "VFX-005 runtime emits preview particles with authored burst");
    candidate.rate_per_second=-9.f;
    check(!editor.set_play_particle_emitter(entity,candidate),
          "VFX-005 invalid Play Mode emitter rejected");
    check(editor.stop_play_mode() &&
          editor.play_particle_count(entity)==0u &&
          authored->initial_burst==3u,
          "VFX-005 leaving Play Mode clears preview and preserves authored World");
}
void sheet() {
    using namespace nengine;
    editor::SpriteSheetEditor slices;
    const auto texture=assets::AssetGuid::generate();
    check(slices.open(texture,64,32)&&slices.auto_grid(16,16) &&
          slices.regions().size()==8u &&
          slices.regions()[7]==editor::SpriteSheetRegion{48,16,16,16},
          "TWOD-005 automatic grid slicing generates exact rectangles");
    check(!slices.add({0,0,16,16}) &&
          slices.erase(0)&&slices.add({0,0,16,16}) &&
          !slices.add({60,30,8,8}),
          "TWOD-005 manual slice edits reject duplicates and out-of-bounds rectangles");
    std::stringstream bytes;
    editor::SpriteSheetEditor restored;
    check(slices.write(bytes)&&restored.read(bytes)&&
          restored.texture()==texture&&restored.regions()==slices.regions(),
          "TWOD-005 manual and automatic slices persist losslessly");
    std::stringstream corrupt("NENGINE_SPRITE_SLICES 1\nTEXTURE "+
        texture.to_string()+"\nSIZE 64 32\nREGIONS 1\nRECT 60 30 20 20\nEND_SLICES\n");
    check(!restored.read(corrupt)&&restored.regions()==slices.regions(),
          "TWOD-005 malformed source cannot overwrite valid authoring state");
    editor::EditorModel model;
    auto root=std::filesystem::temp_directory_path()/
        ("nengine_five_sheet_"+std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    std::string error;
    check(model.open_project(root,&error),"TWOD-005 fixture project opens");
    const auto source=root/"Assets"/"sprite.png";
    {std::ofstream file(source,std::ios::binary);file<<"texture-fixture";}
    model.project().refresh_assets();
    const auto* asset=model.project().assets().find_relative("sprite.png");
    check(asset && model.begin_sprite_sheet_edit(asset->guid,64,32,&error) &&
          model.sprite_sheet_editor().auto_grid(16,16) &&
          model.save_sprite_sheet_edit(&error) &&
          std::filesystem::exists(source.string()+".nslices"),
          "TWOD-005 Editor saves sliced sheet adjacent to imported texture");
    if(asset) {
        check(model.sprite_sheet_editor().erase(0) &&
              model.save_sprite_sheet_edit(&error),
              "TWOD-005 repeat save safely replaces prior document");
    }
    model.project().close();
    std::error_code ec;std::filesystem::remove_all(root,ec);
}
}
int main(){
    prefab();particles();sheet();
    if(failed){std::cerr<<"FiveBatchTests: "<<failed<<" failed\n";return 1;}
    std::cout<<"FiveBatchTests: all checks passed\n";return 0;
}
