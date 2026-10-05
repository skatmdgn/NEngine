#include <windows.h>

#include <chrono>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <thread>

#include "nengine/editor/editor_model.hpp"
#include "nengine/platform/window.hpp"
#include "win32_editor_shell.hpp"

namespace {

std::filesystem::path log_path() {
    wchar_t local_app_data[32768]{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        local_app_data,
        static_cast<DWORD>(std::size(local_app_data)));

    std::filesystem::path base;
    if (length > 0 && length < std::size(local_app_data)) {
        base = std::filesystem::path{local_app_data};
    } else {
        std::error_code temp_error;
        base = std::filesystem::temp_directory_path(temp_error);
        if (temp_error) base = L".";
    }

    const auto directory = base / L"NEngine" / L"Logs";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    return directory / L"editor.log";
}

void write_log(std::string_view message) {
    std::error_code ec;
    const auto path = log_path();
    std::ofstream stream(path, std::ios::app | std::ios::binary);
    if (!stream) return;
    stream << message << '\n';
    stream.flush();
}

LONG WINAPI unhandled_exception_filter(EXCEPTION_POINTERS* info) {
    char buffer[256]{};
    const unsigned long code =
        info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0ul;
    std::snprintf(
        buffer,
        sizeof(buffer),
        "FATAL: unhandled Windows exception 0x%08lX",
        code);
    write_log(buffer);

    MessageBoxW(
        nullptr,
        L"NEngine encountered an unexpected Windows exception.\n\n"
        L"A diagnostic log was written to:\n"
        L"%LOCALAPPDATA%\\NEngine\\Logs\\editor.log",
        L"NEngine Editor - Crash",
        MB_OK | MB_ICONERROR);
    return EXCEPTION_EXECUTE_HANDLER;
}

void initialize_demo_world(nengine::editor::EditorModel& editor) {
    auto& world = editor.world();

    const auto camera = world.create("Main Camera");
    world.transform(camera)->local_position = {0.0f, 4.0f, -8.0f};

    const auto cube = world.create("Cube");
    world.transform(cube)->local_position = {0.0f, 0.0f, 0.0f};

    const auto child = world.create("Child Cube");
    world.transform(child)->local_position = {2.0f, 0.0f, 1.0f};
    world.set_parent(child, cube);

    editor.selection().set(cube);
}

int run_editor() {
    write_log("START: NEngine Editor 0.2.4-dev");
    write_log("STEP: constructing EditorModel");

    nengine::editor::EditorModel editor;
    initialize_demo_world(editor);

    write_log("STEP: creating native Win32 window");
    nengine::platform::Window window({"NEngine Editor 0.2.4-dev", 1440, 900, true});

    SetLastError(ERROR_SUCCESS);
    if (!window.open()) {
        const DWORD error = GetLastError();
        char line[128]{};
        std::snprintf(line, sizeof(line), "ERROR: native window creation failed; Win32=%lu", static_cast<unsigned long>(error));
        write_log(line);

        wchar_t message[512]{};
        swprintf_s(
            message,
            L"NEngine could not create its main window.\n\n"
            L"Win32 error: %lu\n\n"
            L"Log: %%LOCALAPPDATA%%\\NEngine\\Logs\\editor.log",
            static_cast<unsigned long>(error));
        MessageBoxW(nullptr, message, L"NEngine Editor - Startup Error", MB_OK | MB_ICONERROR);
        return 10;
    }

    write_log("OK: native Win32 window created");
    write_log("STEP: attaching Win32 editor shell");

    nengine::app::Win32EditorShell shell(editor);
    SetLastError(ERROR_SUCCESS);
    if (!shell.attach(window.native_handle())) {
        const DWORD error = GetLastError();
        char line[128]{};
        std::snprintf(line, sizeof(line), "ERROR: editor shell attachment failed; Win32=%lu", static_cast<unsigned long>(error));
        write_log(line);

        wchar_t message[640]{};
        swprintf_s(
            message,
            L"NEngine created the main window, but the editor UI failed to attach.\n\n"
            L"Win32 error: %lu\n\n"
            L"The base window will remain open for diagnosis.\n"
            L"Log: %%LOCALAPPDATA%%\\NEngine\\Logs\\editor.log",
            static_cast<unsigned long>(error));
        MessageBoxW(static_cast<HWND>(window.native_handle()), message, L"NEngine Editor - UI Error", MB_OK | MB_ICONERROR);
    } else {
        shell.refresh();
        write_log("OK: editor shell attached");
    }

    write_log("STEP: entering Win32 event loop");
    while (window.poll_events()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    write_log("END: Win32 event loop exited normally");
    return 0;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    SetUnhandledExceptionFilter(&unhandled_exception_filter);

    try {
        return run_editor();
    } catch (const std::exception& exception) {
        write_log(std::string{"FATAL: C++ exception: "} + exception.what());
        MessageBoxW(
            nullptr,
            L"NEngine stopped because of a C++ exception.\n\n"
            L"See %LOCALAPPDATA%\\NEngine\\Logs\\editor.log",
            L"NEngine Editor - Error",
            MB_OK | MB_ICONERROR);
        return 20;
    } catch (...) {
        write_log("FATAL: unknown C++ exception");
        MessageBoxW(
            nullptr,
            L"NEngine stopped because of an unknown exception.\n\n"
            L"See %LOCALAPPDATA%\\NEngine\\Logs\\editor.log",
            L"NEngine Editor - Error",
            MB_OK | MB_ICONERROR);
        return 21;
    }
}
