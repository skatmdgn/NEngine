// Round 2 acceptance: INPUT-007, INPUT-011, UI-004, UI-010.
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "nengine/input/input_state.hpp"
#include "nengine/input/action_map_io.hpp"
#include "nengine/ui/canvas.hpp"
#include "nengine/ui/pointer_router.hpp"
#include "nengine/ui/keyboard_router.hpp"

namespace {
int failures=0;
void check(bool ok,const char* msg){if(!ok){++failures;std::cerr<<"FAIL: "<<msg<<"\n";}}
bool event(const std::vector<nengine::ui::UiPointerEvent>& events,
           nengine::ui::WidgetId id,nengine::ui::UiPointerEventType type) {
    for(const auto& e:events)if(e.target==id&&e.type==type)return true;
    return false;
}
void focus_loss() {
    using namespace nengine::input;
    InputState input;
    ActionMap map;
    map.bind("Fire",Key::MouseLeft);
    map.bind("Move",Key::W);
    input.begin_frame();
    input.set_key(Key::MouseLeft,true);
    input.set_key(Key::W,true);
    check(map.pressed("Fire",input)&&map.pressed("Move",input),
          "INPUT-007: initial key-down observed");
    input.release_all();
    check(!map.held("Fire",input)&&!map.held("Move",input)&&
          !map.pressed("Fire",input)&&!map.pressed("Move",input)&&
          map.released("Fire",input)&&map.released("Move",input),
          "INPUT-007: focus loss cancels down and releases all actions");
    input.begin_frame();
    check(!map.pressed("Fire",input)&&!map.released("Fire",input),
          "INPUT-007: next frame has no stale edges");
    input.set_pointer_position(10,20);
    input.set_pointer_position(100,200);
    input.release_all();
    input.set_pointer_position(900,900);
    check(input.pointer().delta_x==0&&input.pointer().delta_y==0,
          "INPUT-007: return to focus resets pointer delta baseline");
    input.begin_frame();
    input.set_key(Key::MouseLeft,true);
    check(map.pressed("Fire",input)&&map.held("Fire",input),
          "INPUT-007: focus regain supports new press");
}
void input_file_errors() {
    using namespace nengine::input;
    std::error_code ec;
    const auto root=std::filesystem::temp_directory_path()/"nengine-input-acceptance-20261023";
    std::filesystem::remove_all(root,ec);
    std::filesystem::create_directories(root,ec);
    ActionMap old;old.bind("Persist",Key::Enter);
    std::string error;
    for(const auto& sample:{"BAD INPUT","NENGINE_ACTION_MAP 1\nbad format\n",
                             "NENGINE_ACTION_MAP 1\nACTIONS 1000000\n"}) {
        const auto p=root/"corrupt.ninput";
        {std::ofstream out(p,std::ios::binary|std::ios::trunc);out<<sample;}
        check(!load_action_map_file(p,old,&error)&&old.contains("Persist")&&
              old.action_count()==1&&!error.empty(),
              "INPUT-011: corrupt action map does not overwrite running bindings");
    }
    const auto missing=root/"missing.ninput";
    check(!load_action_map_file(missing,old,&error)&&old.contains("Persist"),
          "INPUT-011: missing source preserves action map");
    const auto too_large=root/"huge.ninput";
    {std::ofstream out(too_large,std::ios::binary);std::string chunk(4096,'x');
     for(int i=0;i<17;++i)out<<chunk;}
    check(!load_action_map_file(too_large,old,&error)&&old.contains("Persist"),
          "INPUT-011: oversized action map rejected");
    const auto valid=root/"valid.ninput";
    check(save_action_map_file(valid,old,&error),"INPUT-011: save valid source");
    ActionMap read;
    check(load_action_map_file(valid,read,&error)&&read.contains("Persist"),
          "INPUT-011: valid map reload succeeds");
    check(!save_action_map_file(valid,old,&error),
          "INPUT-011: output collision cannot overwrite valid map");
    std::filesystem::remove_all(root,ec);
}
void ui_pointer_keyboard() {
    using namespace nengine;
    ui::UiCanvas canvas;
    auto first=canvas.add(ui::WidgetKind::Button);
    auto second=canvas.add(ui::WidgetKind::Button);
    auto* a=canvas.find(first);
    a->anchors.offset_min={0,0};
    a->anchors.offset_max={100,60};
    auto* b=canvas.find(second);
    b->anchors.offset_min={150,0};
    b->anchors.offset_max={250,60};
    auto layout=canvas.layout(400,200);
    input::InputState input;
    ui::UiPointerRouter mouse;
    input.begin_frame();input.set_pointer_position(30,30);
    auto hover=mouse.update(layout,input);
    check(event(hover,first,ui::UiPointerEventType::Enter),
          "UI-004: button receives pointer Enter");
    input.set_key(input::Key::MouseLeft,true);
    auto down=mouse.update(layout,input);
    check(event(down,first,ui::UiPointerEventType::Down),
          "UI-004: button captures pointer Down");
    input.set_pointer_position(170,30);
    auto drag=mouse.update(layout,input);
    check(event(drag,first,ui::UiPointerEventType::Exit)&&
          event(drag,second,ui::UiPointerEventType::Enter),
          "UI-004: drag changes hover without changing captured button");
    input.set_key(input::Key::MouseLeft,false);
    auto up=mouse.update(layout,input);
    check(event(up,first,ui::UiPointerEventType::Up)&&
          !event(up,first,ui::UiPointerEventType::Click),
          "UI-004: release outside capture cannot click");
    input.set_pointer_position(30,30);mouse.update(layout,input);
    input.set_key(input::Key::MouseLeft,true);mouse.update(layout,input);
    mouse.cancel_capture();
    input.set_key(input::Key::MouseLeft,false);
    auto cancelled=mouse.update(layout,input);
    check(!event(cancelled,first,ui::UiPointerEventType::Click),
          "UI-004: focus cancellation cannot accidentally click");
    ui::UiKeyboardRouter keyboard;
    auto tab=keyboard.update(layout,true,false);
    check(keyboard.focused()==first&&event(tab,first,ui::UiPointerEventType::Enter),
          "UI-004: Tab focuses first visible button");
    keyboard.update(layout,false,true);
    auto activate=keyboard.update(layout,false,false);
    check(event(activate,first,ui::UiPointerEventType::Click),
          "UI-004: Enter key release invokes focused button");
}
void anchored_layout() {
    using namespace nengine;
    ui::UiCanvas canvas;
    auto stretch=canvas.add(ui::WidgetKind::Panel);
    ui::RectAnchors bounds{};
    bounds.anchor_min={0,0};bounds.anchor_max={1,1};
    bounds.offset_min={20,10};bounds.offset_max={-20,-10};
    check(canvas.set_anchors(stretch,bounds),"UI-010: set responsive anchors");
    const auto a=canvas.layout(320,200).items.front().rect;
    const auto b=canvas.layout(640,400).items.front().rect;
    check(a.x==20&&a.y==10&&a.width==280&&a.height==180&&
          b.x==20&&b.y==10&&b.width==600&&b.height==380,
          "UI-010: resizing preserves fixed safe margins and expands center");
    const auto zero=canvas.layout(0,0);
    check(!zero.items.empty()&&zero.items.front().rect.width<=0,
          "UI-010: zero viewport must not produce a drawable button");
    auto original=ui::UiRect{50,60,100,40};
    auto preset=ui::apply_anchor_preset({0,0,320,200},original,
                                        ui::AnchorPreset::MiddleCenter);
    check(preset.has_value(),"UI-010: center anchor conversion succeeds");
    if(preset){
      check(canvas.set_anchors(stretch,*preset),"UI-010: apply center preset");
      auto rect=canvas.layout(320,200).items.front().rect;
      check(std::fabs(rect.x-original.x)<0.01f&&
            std::fabs(rect.y-original.y)<0.01f&&
            std::fabs(rect.width-original.width)<0.01f,
            "UI-010: DPI/anchor authoring preserves visible rect");
    }
}
}
int main(){
  focus_loss();input_file_errors();ui_pointer_keyboard();anchored_layout();
  if(!failures)std::cout<<"RoundTwoInputUiAcceptanceTests PASS\n";
  return failures?EXIT_FAILURE:EXIT_SUCCESS;
}
