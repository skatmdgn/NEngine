#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include "nengine/core/world.hpp"
#include "nengine/editor/play_session.hpp"
#include "nengine/input/input_state.hpp"
#include "nengine/physics/collision.hpp"
#include "nengine/player/ui_runtime.hpp"
#include "nengine/render/tilemap_physics.hpp"
#include "nengine/ui/canvas_serialization.hpp"

namespace {int failures=0;void expect(bool good,const char* why){if(!good){++failures;std::cerr<<"FAIL: "<<why<<"\n";}}}
int main(){
    using namespace nengine;
    std::string error;
    core::World w;
    auto preserve=w.create("Hero");
    render::Tilemap tiles;
    tiles.resize(8,4);
    tiles.palette.push_back(assets::AssetGuid::generate());
    for(int i=0;i<6;++i)tiles.set(i,0,0);
    tiles.set(3,1,0);
    tiles.set(5,1,0);
    render::TilemapCollisionLayer layer;
    expect(layer.rebuild(w,tiles,{0,0,0},6,false,&error) &&
           layer.collider_count()==3 && w.size()==4,
           "TWOD-004: run-merged static tilemap collider segments generated");
    auto hits=physics::overlap_box_2d(w,{1,0.5f},{0.3f,0.3f});
    expect(hits.size()==1,"TWOD-004: 2D Physics overlap finds tilemap solid");
    expect(physics::overlap_box_2d(w,{1,1.5f},{0.3f,0.3f}).empty(),
           "TWOD-004: empty tile stays non-colliding");
    const auto before=w.size();
    tiles.cell_width=-1;
    expect(!layer.rebuild(w,tiles,{},0,false,&error)&&
           w.size()==before&&layer.collider_count()==3,
           "TWOD-004: invalid Tilemap rebuild transactionally rejected");
    tiles.cell_width=1;
    tiles.cells.assign(32,-1);
    tiles.set(7,3,0);
    expect(layer.rebuild(w,tiles,{10,10,0}) &&w.size()==2 &&
           layer.collider_count()==1 &&w.is_alive(preserve),
           "TWOD-004: rebuilt tilemap deletes only stale owned colliders");
    expect(layer.clear(w)&&w.size()==1&&w.is_alive(preserve),
           "TWOD-004: removing tilemap keeps independently owned entities");

    input::InputState input;
    input::ActionMap actions;
    actions.bind("Shoot",input::Key::MouseLeft);
    actions.bind("Jump",input::Key::Space);
    input.begin_frame();
    input.set_pointer_position(10,10);
    input.set_key(input::Key::MouseLeft,true);
    expect(actions.pressed("Shoot",input),"INPUT-010: unconsumed gameplay input works");
    auto root=std::filesystem::temp_directory_path()/"nengine-consumed-ui-test-20261023";
    std::error_code ec;
    std::filesystem::remove_all(root,ec);
    auto canvas_path=root/"Content"/"Assets"/"UI"/"Main.ncanvas";
    std::filesystem::create_directories(canvas_path.parent_path(),ec);
    ui::UiCanvas canvas;
    auto button=canvas.add(ui::WidgetKind::Button);
    ui::RectAnchors anchors;
    anchors.offset_min={0,0};anchors.offset_max={90,50};
    canvas.set_anchors(button,anchors);
    {std::ofstream out(canvas_path,std::ios::binary);ui::write_canvas(out,canvas,&error);}
    player::PlayerUiRuntime runtime_ui;
    expect(runtime_ui.load(root,&error),"INPUT-010: load UI from Player bundle");
    runtime_ui.update(200,150,input);
    expect(input.pressed(input::Key::MouseLeft) &&
           !actions.pressed("Shoot",input) && input.consumed(input::Key::MouseLeft),
           "INPUT-010: active UI capture consumes gameplay click without erasing physical key");
    input.begin_frame();
    input.set_pointer_position(150,100);
    runtime_ui.update(200,150,input);
    expect(!actions.held("Shoot",input),
           "INPUT-010: captured mouse dragged off UI still blocked from gameplay");
    input.begin_frame();
    input.set_key(input::Key::MouseLeft,false);
    runtime_ui.update(200,150,input);
    expect(!actions.released("Shoot",input),
           "INPUT-010: UI captured pointer release is not a gameplay trigger");
    input.begin_frame();
    input.set_key(input::Key::MouseLeft,true);
    runtime_ui.update(200,150,input);
    expect(actions.pressed("Shoot",input),
           "INPUT-010: after UI capture ends, off-UI gameplay action works again");
    input.begin_frame();
    input.set_key(input::Key::Space,true);
    input.consume(input::Key::Space);
    expect(input.held(input::Key::Space)&&!actions.pressed("Jump",input),
           "INPUT-010: UI keyboard consumption suppresses ActionMap without changing hardware input");
    input.begin_frame();
    expect(actions.held("Jump",input),"INPUT-010: UI consume flags expire next frame");
    std::filesystem::remove_all(root,ec);

    editor::PlaySession session;
    core::World game;
    game.create("MovingWorld");
    expect(session.set_time_scale(0.0)&&session.play(game),
           "PHYS-011: PlaySession accepts stopped time");
    expect(session.consume_simulation_steps(1.0)==0,
           "PHYS-011: zero timescale freezes automatic FixedUpdate");
    expect(session.pause()&&session.step()&&
           session.consume_simulation_steps(1.0)==1,
           "PHYS-011: paused single-step executes exactly one fixed frame");
    expect(session.resume()&&session.set_time_scale(2.0)&&
           session.consume_simulation_steps(1.0/60.0)==2,
           "PHYS-011: double speed produces two fixed steps for 60Hz host tick");
    expect(!session.set_time_scale(-1.0)&&!session.set_time_scale(
           std::numeric_limits<double>::infinity())&&session.time_scale()==2.0,
           "PHYS-011: invalid time scale rejected without mutating active simulation");
    expect(session.stop(),"PHYS-011: Stop restores edit state");

    if(!failures)std::cout<<"NEngineRoundFiveRuntimeTests PASS\n";
    return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
