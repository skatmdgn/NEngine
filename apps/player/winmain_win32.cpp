#include <windows.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "nengine/platform/window.hpp"
#include "nengine/player/runtime.hpp"
#include "player_cpu_profiler_win32.hpp"
#include "nengine/core/build_identity.hpp"
#include <fstream>
#include "nengine/render/vulkan_context.hpp"
#include "nengine/render/vulkan_diagnostic_scene.hpp"
#include "nengine/audio/audio_device.hpp"
#include "nengine/audio/mix_snapshot.hpp"

namespace {
std::string current_game_build_version="unknown";
void diagnostic(std::string_view message) {
    wchar_t base[MAX_PATH]{};
    const auto size=GetEnvironmentVariableW(
        L"LOCALAPPDATA",base,static_cast<DWORD>(std::size(base)));
    if(size==0||size>=std::size(base))return;
    auto directory=std::filesystem::path(base)/L"NEngine"/L"Logs";
    std::error_code ec;
    std::filesystem::create_directories(directory,ec);
    if(ec)return;
    std::ofstream stream(directory/L"player.log",std::ios::binary|std::ios::app);
    if(stream)stream<<nengine::core::build_identity(current_game_build_version)
                    <<" "<<message<<"\n";
}
std::filesystem::path executable_directory() {
    wchar_t name[32768]{};
    const auto count = GetModuleFileNameW(
        nullptr, name, static_cast<DWORD>(std::size(name)));
    if (count == 0 || count >= std::size(name)) return {};
    return std::filesystem::path(name).parent_path();
}
void message(std::string_view text) {
    diagnostic(std::string("ERROR: ")+std::string(text));
    const auto wide = std::wstring(text.begin(), text.end());
    MessageBoxW(nullptr, wide.c_str(), L"NEngine Player",
                MB_OK | MB_ICONERROR);
}
int run() {
    const auto root = executable_directory();
    if (root.empty()) { message("Could not locate Player directory."); return 2; }
    nengine::player::PlayerRuntime runtime;
    std::string error;
    if (!runtime.load(root, &error)) { message(error); return 3; }
    current_game_build_version=runtime.manifest().game_version;
    diagnostic("START: Windows Player build identity");

    const auto gameplay_path =
        root / "Content" / "Managed" /
        (runtime.manifest().managed_assembly_name + ".dll");
    if (std::filesystem::is_regular_file(gameplay_path)) {
        std::string script_error;
        if (!runtime.initialize_managed(&script_error)) {
            message("C# gameplay host failed: " + script_error);
            return 8;
        }
        OutputDebugStringA("NEngine Player: managed C# gameplay initialized.\n");
    } else if (runtime.pending_script_count()) {
        OutputDebugStringA("NEngine Player: ScriptBehaviours present without"
                           " packaged managed assembly; managed gameplay disabled.\n");
    }
    const auto& config = runtime.manifest();
    nengine::platform::Window window({
        config.title, config.width, config.height, true});
    if (!window.open()) { message("Could not open Player window."); return 4; }

    nengine::render::VulkanContext renderer;
    if (!renderer.initialize_for_window(
            GetModuleHandleW(nullptr), window.native_handle(),
            window.client_width(), window.client_height())) {
        message(renderer.diagnostic()); return 5;
    }
    nengine::render::VulkanDiagnosticScene scene;
    if (!scene.initialize(renderer)) {
        message(scene.diagnostic()); return 6;
    }
    nengine::audio::AudioOutputDevice audio_device;
    std::string audio_error;
    bool audio_ready = audio_device.open(&audio_error);
    // A Null fallback reports open=true so headless tests can run; a
    // graphical Player must not silently masquerade it as audible output.
    const auto audio_info = audio_device.info();
    if (!audio_ready || audio_info.backend != "WASAPI") {
        const auto reason = !audio_ready ? audio_error : audio_info.diagnostic;
        const auto warning = "Audio playback is unavailable (silent mode): " +
                             reason;
        OutputDebugStringA(("NEngine Player: " + warning + "\n").c_str());
        MessageBoxW(static_cast<HWND>(window.native_handle()),
                    std::wstring(warning.begin(), warning.end()).c_str(),
                    L"NEngine Player - Audio warning", MB_OK | MB_ICONWARNING);
        audio_ready = false;
    }

    std::uint32_t old_width = window.client_width();
    std::uint32_t old_height = window.client_height();
    // The Scene holds Vulkan pipelines and GPU caches associated with the
    // old render pass. Shut it down before rebuilding swapchain resources.
    const auto recreate_swapchain = [&](std::uint32_t width,
                                        std::uint32_t height) {
        scene.shutdown();
        return renderer.resize(width, height) &&
            scene.initialize(renderer);
    };
    auto last_frame = std::chrono::steady_clock::now();
    double profile_ms=0.;std::size_t profile_frames=0;
    nengine::app::PlayerCpuProfiler cpu_profiler;
    while (window.poll_events()) {
        const auto now = std::chrono::steady_clock::now();
        const auto delta = std::chrono::duration<float>(now - last_frame).count();
        last_frame = now;
        // Sample held keys once per rendered frame, ahead of C# Update.
        // Win32 polling here is intentionally separate from Editor input.
        runtime.begin_input_frame();
        auto& input = runtime.input();
        const HWND native_window =
            static_cast<HWND>(window.native_handle());
        const bool focused = GetForegroundWindow() == native_window;
        // Player samples its own InputState (separate from Window input).
        // Release it on focus loss so the next focused cursor sample is a
        // fresh baseline rather than a large off-window mouse movement.
        if (!focused) input.release_all();
        const auto down = [focused](int virtual_key) {
            return focused && (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
        };
        for (int index = 0; index < 26; ++index)
            input.set_key(static_cast<nengine::input::Key>(
                static_cast<int>(nengine::input::Key::A) + index),
                down('A' + index));
        for (int index = 0; index < 10; ++index)
            input.set_key(static_cast<nengine::input::Key>(
                static_cast<int>(nengine::input::Key::Digit0) + index),
                down('0' + index));
        using Key = nengine::input::Key;
        constexpr std::array<std::pair<Key, int>, 18> mapped{{
            {Key::Space, VK_SPACE}, {Key::Enter, VK_RETURN},
            {Key::Escape, VK_ESCAPE}, {Key::Tab, VK_TAB},
            {Key::Backspace, VK_BACK}, {Key::Up, VK_UP},
            {Key::Down, VK_DOWN}, {Key::Left, VK_LEFT}, {Key::Right, VK_RIGHT},
            {Key::LeftShift, VK_LSHIFT}, {Key::RightShift, VK_RSHIFT},
            {Key::LeftControl, VK_LCONTROL}, {Key::RightControl, VK_RCONTROL},
            {Key::LeftAlt, VK_LMENU}, {Key::RightAlt, VK_RMENU},
            {Key::MouseLeft, VK_LBUTTON}, {Key::MouseRight, VK_RBUTTON},
            {Key::MouseMiddle, VK_MBUTTON}
        }};
        for (const auto [key, virtual_key] : mapped)
            input.set_key(key, down(virtual_key));
        // Window::poll_events already consumed WM_MOUSEWHEEL. Forward the
        // event delta to the Player's distinct managed-facing InputState.
        if (focused)
            input.add_wheel(window.input_state().pointer().wheel_y);
        POINT pointer{};
        if (focused && GetCursorPos(&pointer) &&
            ScreenToClient(native_window, &pointer))
            input.set_pointer_position(
                static_cast<float>(pointer.x),
                static_cast<float>(pointer.y));
        // Route UI before managed gameplay Update so UI.WasClicked observes
        // the current input-frame snapshot. No Canvas is valid as a no-op.
        runtime.update_ui(static_cast<float>(window.client_width()),
                          static_cast<float>(window.client_height()), focused);
        // The stand-alone Player World advances independently of Editor Play.
        // PlayerRuntime limits large frame stalls to eight fixed steps.
        const auto profile_begin=std::chrono::steady_clock::now();
        const auto profile_frame=runtime.advance(delta);
        profile_ms+=std::chrono::duration<double,std::milli>(
             std::chrono::steady_clock::now()-profile_begin).count();
        if(++profile_frames>=300){
            diagnostic("PROFILE avg_player_update_ms="+
                std::to_string(profile_ms/profile_frames)+
                " world_entities="+std::to_string(runtime.entity_count()));
            profile_ms=0.;profile_frames=0;
        }
        if (audio_ready) {
            const auto mix = nengine::audio::build_mix_snapshot(runtime.world());
            std::string pump_error;
            audio_device.pump(mix,
                [&runtime](nengine::assets::AssetGuid guid)
                    -> const nengine::audio::AudioClipData* {
                    return runtime.audio_clip(guid);
                }, &pump_error);
            if (!pump_error.empty()) {
                OutputDebugStringA(
                    ("NEngine Player: audio stream error: " +
                     pump_error + "\n").c_str());
                audio_device.close();
                audio_ready = false;
            }
        }
        const auto width = window.client_width();
        const auto height = window.client_height();
        if (width != old_width || height != old_height) {
            old_width = width;
            old_height = height;
            if (width && height && !recreate_swapchain(width, height)) {
                message("Vulkan resize failed: " + renderer.diagnostic());
                return 7;
            }
        }
        // Rasterize the current mutable runtime Canvas after C# Update.
        // The CPU RGBA image is composited in the same Vulkan render pass
        // after World geometry rather than overlaid with a Win32 child window.
        const auto overlay = runtime.ui().ready()
            ? runtime.ui().rasterize(width, height)
            : nengine::ui::UiRgbaImage{};
        const nengine::render::VulkanCanvasFrame overlay_view{
            overlay.width, overlay.height, overlay.pixels
        };
        const auto* overlay_ptr =
            overlay_view.valid() ? &overlay_view : nullptr;
        const auto render_start=std::chrono::steady_clock::now();
        if (width && height && !scene.present_world(
                renderer, runtime.world(),
                [&runtime](nengine::assets::AssetGuid guid) {
                    return runtime.assets().cooked_artifacts(guid);
                }, overlay_ptr)) {
            if (renderer.needs_resize()) {
                // Recover both swapchain resources and render-pass pipelines.
                if (!recreate_swapchain(width, height)) {
                    message("Vulkan swapchain recovery failed: " +
                            renderer.diagnostic());
                    return 7;
                }
                continue;
            }
            message(scene.diagnostic());
            return 7;
        }
        const auto render_end=std::chrono::steady_clock::now();
        const auto elapsed_ms=[](auto start,auto end){
            return std::chrono::duration<double,std::milli>(end-start).count();
        };
        cpu_profiler.sample(elapsed_ms(now,render_end),
            elapsed_ms(profile_begin,render_start),
            elapsed_ms(render_start,render_end),
            profile_frame,runtime.world().entities().size());
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    audio_device.close();
    scene.shutdown();
    renderer.shutdown();
    return 0;
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    try { return run(); }
    catch (...) { message("Unhandled C++ exception during Player startup."); return 20; }
}
