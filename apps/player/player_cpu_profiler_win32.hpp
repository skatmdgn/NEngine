#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <numeric>
#include <string>
#include "nengine/player/runtime.hpp"

// EDTOOL-006: F3 toggles a real standalone Windows Player CPU profiler.
// This deliberately does not label CPU Vulkan submission as actual GPU time.
namespace nengine::app {
class PlayerCpuProfiler {
public:
    ~PlayerCpuProfiler() { if (IsWindow(window_)) DestroyWindow(window_); }
    void sample(double frame_ms, double simulation_ms, double render_ms,
                const player::PlayerFrameStats& frame,
                std::size_t entities) {
        const bool pressed=(GetAsyncKeyState(VK_F3)&0x8000)!=0;
        if (pressed && !was_pressed_) {
            if (IsWindow(window_)) {DestroyWindow(window_);window_=nullptr;}
            else window_=open_window();
        }
        was_pressed_=pressed;
        if(frame_ms<0||frame_ms>60000||simulation_ms<0||
           simulation_ms>60000||render_ms<0||render_ms>60000)return;
        frames_[cursor_]=frame_ms;sim_[cursor_]=simulation_ms;
        render_[cursor_]=render_ms;
        cursor_=(cursor_+1)%frames_.size();
        count_=std::min(count_+1,frames_.size());
        const auto now=GetTickCount64();
        if (!IsWindow(window_) || now<next_refresh_)return;
        next_refresh_=now+250;
        HWND list=GetDlgItem(window_,1);
        if(!list)return;
        const auto avg=[this](const auto& data){
            return std::accumulate(data.begin(),data.begin()+count_,0.0)/
                   static_cast<double>(count_);
        };
        const std::array<std::wstring,8> rows{
            L"Standalone Windows Player CPU Profiler (F3 to toggle)",
            L"CPU frame mean: "+std::to_wstring(avg(frames_))+L" ms",
            L"Simulation mean: "+std::to_wstring(avg(sim_))+L" ms",
            L"Vulkan submit/present CPU mean: "+std::to_wstring(avg(render_))+L" ms",
            L"Physics steps (last): "+std::to_wstring(frame.physics_steps),
            L"Managed updates (last): "+std::to_wstring(frame.managed_updates),
            L"Entities: "+std::to_wstring(entities)+L" | samples: "+std::to_wstring(count_),
            L"Actual GPU duration and VRAM are not measured"
        };
        SendMessageW(list,WM_SETREDRAW,FALSE,0);
        SendMessageW(list,LB_RESETCONTENT,0,0);
        for(const auto& line:rows)
            SendMessageW(list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(line.c_str()));
        SendMessageW(list,WM_SETREDRAW,TRUE,0);
        InvalidateRect(list,nullptr,FALSE);
    }
private:
    static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {
        if(m==WM_SIZE) {
            if(HWND list=GetDlgItem(h,1))
                MoveWindow(list,8,8,std::max(50,int(LOWORD(l))-16),
                           std::max(50,int(HIWORD(l))-16),TRUE);
            return 0;
        }
        if(m==WM_CLOSE){DestroyWindow(h);return 0;}
        return DefWindowProcW(h,m,w,l);
    }
    static HWND open_window() {
        const wchar_t* name=L"NEngine.Player.ProfileWindow";
        WNDCLASSEXW c{};c.cbSize=sizeof(c);
        c.hInstance=GetModuleHandleW(nullptr);
        c.hCursor=LoadCursorW(nullptr,IDC_ARROW);
        c.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
        c.lpfnWndProc=proc;c.lpszClassName=name;
        WNDCLASSEXW existing{};existing.cbSize=sizeof(existing);
        if(!GetClassInfoExW(c.hInstance,name,&existing)&&!RegisterClassExW(&c))
            return nullptr;
        HWND window=CreateWindowExW(WS_EX_TOOLWINDOW,name,
            L"NEngine Player Profiler",WS_OVERLAPPEDWINDOW|WS_VISIBLE,
            CW_USEDEFAULT,CW_USEDEFAULT,520,260,nullptr,nullptr,
            c.hInstance,nullptr);
        if(!window)return nullptr;
        if(!CreateWindowExW(0,L"LISTBOX",L"",
            WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL,
            8,8,480,210,window,reinterpret_cast<HMENU>(1),
            c.hInstance,nullptr)) {
            DestroyWindow(window);
            return nullptr;
        }
        return window;
    }
    HWND window_{nullptr};
    bool was_pressed_{false};
    ULONGLONG next_refresh_{0};
    std::array<double,120> frames_{},sim_{},render_{};
    std::size_t cursor_{0},count_{0};
};
}
