#include <windows.h>

#include <array>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

#include "nengine/platform/window.hpp"
#include "nengine/player/runtime.hpp"
#include "nengine/render/vulkan_context.hpp"
#include "nengine/render/vulkan_diagnostic_scene.hpp"
#include "nengine/audio/audio_device.hpp"
#include "nengine/audio/mix_snapshot.hpp"

namespace {
std::filesystem::path executable_directory() {
    wchar_t name[32768]{};
    const auto count = GetModuleFileNameW(
        nullptr, name, static_cast<DWORD>(std::size(name)));
    if (count == 0 || count >= std::size(name)) return {};
    return std::filesystem::path(name).parent_path();
}
void message(std::string_view text) {
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

    const auto gameplay_path =
        root / "Content" / "Managed" / "GameScripts.dll";
    if (std::filesystem::is_regular_file(gameplay_path)) {
        std::string script_error;
        if (!runtime.initialize_managed(&script_error)) {
            message("C# gameplay host failed: " + script_error);
            return 8;
        }
        OutputDebugStringA("NEngine Player: managed C# gameplay initialized.\n");
    } else if (runtime.pending_script_count()) {
        OutputDebugStringA("NEngine Player: ScriptBehaviours present without"
                           " packaged GameScripts.dll; managed gameplay disabled.\n");
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
    if (!audio_ready)
        OutputDebugStringA(
            ("NEngine Player: audio device unavailable: " +
             audio_error + "\n").c_str());

    std::uint32_t old_width = window.client_width();
    std::uint32_t old_height = window.client_height();
    auto last_frame = std::chrono::steady_clock::now();
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
        runtime.advance(delta);
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
            if (width && height) renderer.resize(width, height);
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
        if (width && height && !scene.present_world(
                renderer, runtime.world(),
                [&runtime](nengine::assets::AssetGuid guid) {
                    return runtime.assets().cooked_artifacts(guid);
                }, overlay_ptr)) {
            message(scene.diagnostic());
            return 7;
        }
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
