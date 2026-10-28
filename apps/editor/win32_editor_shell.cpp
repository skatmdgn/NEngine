#include "win32_editor_shell.hpp"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nengine/core/scene.hpp"
#include "nengine/editor/command.hpp"
#include "nengine/editor/presentation.hpp"
#include "nengine/editor/property_command.hpp"
#include "nengine/editor/property_text.hpp"
#include "nengine/editor/scene_interaction.hpp"
#include "nengine/scripting/dotnet_host.hpp"
#include "nengine/scripting/managed_build.hpp"
#include "nengine/scripting/managed_project.hpp"
#include "nengine/editor/ide_launch.hpp"
#include "nengine/editor/action_map_editor.hpp"
#include "nengine/editor/build_settings_editor.hpp"
#include "nengine/editor/performance_profiler.hpp"
#include "nengine/scripting/managed_runtime.hpp"
#include "nengine/render/vulkan_context.hpp"
#include "nengine/render/vulkan_diagnostic_scene.hpp"

namespace nengine::app {
namespace {

constexpr wchar_t kHostClassName[] = L"NEngine.EditorHost";
constexpr wchar_t kSceneClassName[] = L"NEngine.SceneView";
constexpr wchar_t kSplitterClassName[] = L"NEngine.Splitter";

constexpr int kToolbarHeight = 70;
constexpr int kSplitterSize = 6;
constexpr int kPadding = 8;

enum ControlId : int {
    IdOpen = 1001,
    IdSave,
    IdNewEntity,
    IdDeleteEntity,
    IdRefreshAssets,
    IdScripts,
    IdBuildScripts,
    IdUndo,
    IdRedo,
    IdPlay,
    IdPause,
    IdStep,
    IdStop,
    IdVulkanPreview,
    IdGameView,
    IdActionEditor,
    IdActionList,
    IdActionName,
    IdActionKey,
    IdActionAdd,
    IdActionRemove,
    IdActionSave,
    IdBuildSettings,
    IdBuildName,
    IdBuildWindows,
    IdBuildAndroid,
    IdBuildToolchain,
    IdBuildApply,
    IdAndroidSdkPath,
    IdAndroidNdkPath,
    IdAndroidJdkPath,
    IdAndroidPathsApply,
    IdPackages,
    IdPackageList,
    IdPackageId,
    IdPackageVersion,
    IdPackageAdd,
    IdPackageRemove,
    IdProfiler,
    IdCanvasEdit,
    IdCanvasName,
    IdCanvasText,
    IdCanvasApply,
    IdCanvasSave,
    IdCanvasAddButton,
    IdCanvasAddText,
    IdCanvasAddImage,
    IdCanvasImageGuid,
    IdCanvasAssignImage,
    IdCanvasClearImage,
    IdCanvasDelete,
    IdCanvasUndo,
    IdCanvasRedo,
    IdHierarchy,
    IdDuplicateHierarchy,
    IdParentHierarchy,
    IdName,
    IdActive,
    IdPosX,
    IdPosY,
    IdPosZ,
    IdRotX,
    IdRotY,
    IdRotZ,
    IdRotW,
    IdScaleX,
    IdScaleY,
    IdScaleZ,
    IdApplyTransform,
    IdGenericProperties,
    IdGenericValue,
    IdAssignAsset,
    IdApplyProperty,
    IdRemoveComponent,
    IdAddComponentList,
    IdAddComponent,
    IdAssets,
    IdConsole,
    IdSplitHierarchy,
    IdSplitInspector,
    IdSplitBottom,
    IdCanvasAnchorBase = 11000,
    IdCanvasColorBase = 11020,
    IdCanvasApplyGeometry = 11040,
    IdCanvasWidgetList = 11041,
    IdCanvasDuplicate = 11042,
    IdCanvasBack = 11043,
    IdCanvasFront = 11044,
    IdCanvasPreset = 11045,
    IdCanvasApplyPreset = 11046,
    IdCanvasParent = 11047,
    IdCanvasApplyParent = 11048,
};

struct GenericPropertyBinding {
    nengine::core::ComponentTypeId component{
        nengine::core::ComponentRegistry::invalid_type};
    std::string property{};
    nengine::core::PropertyKind kind{
        nengine::core::PropertyKind::String};
    nengine::core::PropertyValue value{};
    bool editable{false};
};

std::filesystem::path shell_log_path() {
    wchar_t local_app_data[32768]{};
    const DWORD length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        local_app_data,
        static_cast<DWORD>(std::size(local_app_data)));

    std::filesystem::path base;
    if (length > 0 && length < std::size(local_app_data)) {
        base = std::filesystem::path{local_app_data};
    } else {
        base = L".";
    }

    const auto directory = base / L"NEngine" / L"Logs";
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    return directory / L"editor.log";
}

void shell_log(std::string_view message) {
    std::ofstream stream(shell_log_path(), std::ios::app | std::ios::binary);
    if (!stream) return;
    stream << "SHELL: " << message << '\n';
    stream.flush();
}

std::wstring utf8_to_wide(std::string_view text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0);
    if (count <= 0) return {};

    std::wstring result(static_cast<std::size_t>(count), L'\0');
    MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        count);
    return result;
}

std::string wide_to_utf8(std::wstring_view text) {
    if (text.empty()) return {};
    const int count = WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    if (count <= 0) return {};

    std::string result(static_cast<std::size_t>(count), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        WC_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        count,
        nullptr,
        nullptr);
    return result;
}

std::string read_text(HWND control) {
    if (!control) return {};
    const int length = GetWindowTextLengthW(control);
    if (length <= 0) return {};

    std::wstring value(static_cast<std::size_t>(length) + 1u, L'\0');
    const int written = GetWindowTextW(control, value.data(), length + 1);
    if (written <= 0) return {};

    value.resize(static_cast<std::size_t>(written));
    return wide_to_utf8(value);
}

void set_text(HWND control, std::string_view text) {
    if (!control) return;
    const auto wide = utf8_to_wide(text);
    SetWindowTextW(control, wide.c_str());
}

void set_float(HWND control, float value) {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.4f", static_cast<double>(value));
    set_text(control, buffer);
}

bool read_float(HWND control, float& value) {
    const auto text = read_text(control);
    if (text.empty()) return false;

    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (!end || end == text.c_str() || *end != '\0' || !std::isfinite(parsed)) {
        return false;
    }

    value = parsed;
    return true;
}

HWND create_control(
    HWND parent,
    const wchar_t* klass,
    const wchar_t* text,
    DWORD style,
    int id = 0) {

    SetLastError(ERROR_SUCCESS);
    return CreateWindowExW(
        0,
        klass,
        text,
        WS_CHILD | WS_VISIBLE | style,
        0,
        0,
        10,
        10,
        parent,
        id == 0 ? nullptr : reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
}

bool register_basic_class(
    const wchar_t* name,
    WNDPROC procedure,
    HBRUSH background,
    HCURSOR cursor) {

    const HINSTANCE instance = GetModuleHandleW(nullptr);

    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(instance, name, &existing)) {
        return true;
    }

    WNDCLASSEXW klass{};
    klass.cbSize = sizeof(klass);
    klass.style = CS_HREDRAW | CS_VREDRAW;
    klass.lpfnWndProc = procedure;
    klass.hInstance = instance;
    klass.hCursor = cursor;
    klass.hbrBackground = background;
    klass.lpszClassName = name;

    SetLastError(ERROR_SUCCESS);
    return RegisterClassExW(&klass) != 0;
}

} // namespace

struct Win32EditorShell::Impl {
    explicit Impl(nengine::editor::EditorModel& value) : editor(value) {}

    nengine::editor::EditorModel& editor;

    HWND parent{nullptr};
    HWND host{nullptr};
    bool controls_ready{false};
    bool refreshing{false};
    int last_parent_width{-1};
    int last_parent_height{-1};

    HWND open_scene{nullptr};
    HWND save_scene{nullptr};
    HWND new_entity{nullptr};
    HWND delete_entity{nullptr};
    HWND refresh_assets{nullptr};
    HWND scripts{nullptr};
    HWND build_scripts{nullptr};
    HWND undo{nullptr};
    HWND redo{nullptr};
    HWND play{nullptr};
    HWND pause{nullptr};
    HWND step{nullptr};
    HWND stop{nullptr};
    HWND vulkan_preview{nullptr};
    HWND profiler_toggle{nullptr};
    HWND profiler_list{nullptr};
    bool profiler_visible{false};
    ULONGLONG next_profiler_refresh{0};
    nengine::editor::PerformanceProfiler profiler_samples{};
    HWND package_toggle{nullptr};
    HWND package_list{nullptr};
    HWND package_id{nullptr};
    HWND package_version{nullptr};
    HWND package_add{nullptr};
    HWND package_remove{nullptr};
    bool package_editor_visible{false};
    nengine::editor::ManagedPackageRows package_rows{};
    HWND build_toggle{nullptr};
    HWND build_name{nullptr};
    HWND build_windows{nullptr};
    HWND build_android{nullptr};
    HWND build_toolchain{nullptr};
    HWND build_apply{nullptr};
    HWND android_sdk_path{nullptr};
    HWND android_ndk_path{nullptr};
    HWND android_jdk_path{nullptr};
    HWND android_paths_apply{nullptr};
    bool build_settings_visible{false};
    HWND action_toggle{nullptr};
    HWND action_list{nullptr};
    HWND action_name{nullptr};
    HWND action_key{nullptr};
    HWND action_add{nullptr};
    HWND action_remove{nullptr};
    HWND action_save{nullptr};
    std::vector<std::string> action_names{};
    std::vector<nengine::input::Key> action_key_options{};
    nengine::editor::ActionMapEditor action_document{};
    bool action_editor_visible{false};
    HWND game_toggle{nullptr};
    HWND game_view{nullptr};
    bool game_view_visible{true};
    HWND canvas_edit_toggle{nullptr};
    HWND canvas_name{nullptr};
    HWND canvas_text{nullptr};
    HWND canvas_apply{nullptr};
    HWND canvas_save{nullptr};
    HWND canvas_add_button{nullptr};
    HWND canvas_add_text{nullptr};
    HWND canvas_add_image{nullptr};
    HWND canvas_image_guid{nullptr};
    HWND canvas_assign_image{nullptr};
    HWND canvas_clear_image{nullptr};
    HWND canvas_delete{nullptr};
    HWND canvas_undo{nullptr};
    HWND canvas_redo{nullptr};
    std::array<HWND, 8> canvas_anchor_inputs{};
    std::array<HWND, 4> canvas_color_inputs{};
    HWND canvas_anchor_label{nullptr};
    HWND canvas_offset_label{nullptr};
    HWND canvas_color_label{nullptr};
    HWND canvas_apply_geometry{nullptr};
    HWND canvas_widget_list{nullptr};
    HWND canvas_duplicate{nullptr};
    HWND canvas_to_back{nullptr};
    HWND canvas_to_front{nullptr};
    HWND canvas_preset{nullptr};
    HWND canvas_apply_preset{nullptr};
    HWND canvas_parent_combo{nullptr};
    HWND canvas_apply_parent{nullptr};
    std::vector<nengine::ui::WidgetId> canvas_parent_rows{};
    std::vector<nengine::ui::WidgetId> canvas_widget_rows{};
    bool canvas_mode{false};

    HWND hierarchy{nullptr};
    HWND duplicate_hierarchy{nullptr};
    HWND parent_hierarchy{nullptr};
    HWND scene{nullptr};

    HWND inspector_title{nullptr};
    HWND label_name{nullptr};
    HWND label_position{nullptr};
    HWND label_rotation{nullptr};
    HWND label_scale{nullptr};
    HWND name{nullptr};
    HWND active{nullptr};

    std::array<HWND, 3> position{};
    std::array<HWND, 4> rotation{};
    std::array<HWND, 3> scale{};
    HWND apply_transform{nullptr};

    HWND generic_title{nullptr};
    HWND generic_properties{nullptr};
    HWND generic_value{nullptr};
    HWND assign_asset{nullptr};
    HWND apply_property{nullptr};
    HWND remove_component{nullptr};
    HWND add_component_list{nullptr};
    HWND add_component{nullptr};
    std::vector<GenericPropertyBinding>
        generic_property_rows{};
    std::vector<nengine::core::ComponentTypeId>
        add_component_rows{};
    nengine::core::Entity generic_property_entity{
        nengine::core::Entity::invalid()};

    HWND assets_list{nullptr};
    HWND console{nullptr};

    HWND split_hierarchy{nullptr};
    HWND split_inspector{nullptr};
    HWND split_bottom{nullptr};
    std::vector<nengine::editor::HierarchyRow> hierarchy_rows{};
    std::vector<nengine::assets::AssetRecord> asset_rows{};
    std::filesystem::path current_scene_path{};
    ULONGLONG next_asset_poll_tick{0};
    ULONGLONG last_runtime_tick{0};
    ULONGLONG managed_build_generation{0};

    std::unique_ptr<
        nengine::render::VulkanContext>
        vulkan_context{};

    std::unique_ptr<
        nengine::render::VulkanDiagnosticScene>
        vulkan_diagnostic_scene{};

    int vulkan_scene_width{-1};
    int vulkan_scene_height{-1};
    bool vulkan_preview_enabled{false};

    bool ui_pointer_captured{false};
    bool ui_tab_down{false};
    bool ui_enter_down{false};
    bool ui_space_down{false};
    bool ui_escape_down{false};
    bool canvas_dragging{false};
    bool canvas_drag_resize{false};
    int canvas_drag_start_x{0};
    int canvas_drag_start_y{0};
    bool scene_dragging{false};
    nengine::editor::SceneGizmoAxis scene_drag_axis{
        nengine::editor::SceneGizmoAxis::None};
    nengine::core::Entity scene_drag_entity{
        nengine::core::Entity::invalid()};
    nengine::core::Transform scene_drag_original{};
    int scene_drag_start_x{0};
    int scene_drag_start_y{0};

    enum class LayoutDrag {
        None,
        Hierarchy,
        Inspector,
        Bottom,
    };

    LayoutDrag layout_drag{
        LayoutDrag::None};

    static LRESULT CALLBACK host_proc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) {

        auto* self = reinterpret_cast<Impl*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(self));
        }

        if (!self) {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }

        switch (message) {
        case WM_SIZE:
            if (self->controls_ready) {
                self->layout(LOWORD(lparam), HIWORD(lparam));
            }
            return 0;

        case WM_COMMAND:
            if (self->refreshing) {
                return 0;
            }
            if (self->controls_ready &&
                self->on_command(LOWORD(wparam), HIWORD(wparam))) {
                return 0;
            }
            break;

        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;

        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    static LRESULT CALLBACK scene_proc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) {

        auto* self = reinterpret_cast<Impl*>(
            GetWindowLongPtrW(hwnd, GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(self));
        }

        if (!self) {
            return DefWindowProcW(hwnd, message, wparam, lparam);
        }

        switch (message) {
        case WM_KEYDOWN:
        case WM_KEYUP:
            if (message == WM_KEYDOWN &&
                self->canvas_mode && self->editor.has_ui_canvas_draft() &&
                (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                bool processed = true;
                if (wparam == 'Z') self->editor.ui_edit_undo();
                else if (wparam == 'Y') self->editor.ui_edit_redo();
                else if (wparam == 'S') {
                    std::string error;
                    if (!self->editor.save_ui_canvas_edit(&error))
                        self->editor.console().warning("UI", error);
                    else self->editor.console().info("UI", "Canvas saved.");
                } else processed = false;
                if (processed) { self->refresh(); return 0; }
            }
            if (!self->vulkan_preview_enabled &&
                self->editor.play_session().is_playing() &&
                self->editor.has_ui_canvas() &&
                (wparam == VK_TAB || wparam == VK_RETURN ||
                 wparam == VK_SPACE || wparam == VK_ESCAPE)) {
                const bool down = message == WM_KEYDOWN;
                if (wparam == VK_TAB) self->ui_tab_down = down;
                if (wparam == VK_RETURN) self->ui_enter_down = down;
                if (wparam == VK_SPACE) self->ui_space_down = down;
                if (wparam == VK_ESCAPE) self->ui_escape_down = down;
                RECT bounds{};
                GetClientRect(hwnd, &bounds);
                const auto events = self->editor.route_ui_keyboard(
                    static_cast<float>(bounds.right),
                    static_cast<float>(bounds.bottom),
                    self->ui_tab_down, self->ui_enter_down,
                    (GetKeyState(VK_SHIFT) & 0x8000) != 0,
                    self->ui_space_down,
                    self->ui_escape_down);
                for (const auto& event : events)
                    if (event.type == nengine::ui::UiPointerEventType::Click)
                        self->editor.console().info(
                            "UI", "Keyboard Canvas click: " +
                                std::to_string(event.target));
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            break;

        case WM_KILLFOCUS:
            if (self->canvas_dragging) {
                self->canvas_dragging = false;
                self->editor.ui_edit_end_drag(false);
                if (GetCapture() == hwnd) ReleaseCapture();
            }
            self->ui_tab_down = false;
            self->ui_enter_down = false;
            self->ui_space_down = false;
            self->ui_escape_down = false;
            self->editor.reset_ui_keyboard();
            break;

        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            if (!self->vulkan_preview_enabled &&
                self->editor.play_session().is_playing() &&
                self->editor.has_ui_canvas()) {
                RECT bounds{};
                GetClientRect(hwnd, &bounds);
                const auto events = self->editor.route_ui_pointer(
                    static_cast<float>(bounds.right),
                    static_cast<float>(bounds.bottom),
                    {static_cast<float>(static_cast<short>(LOWORD(lparam))),
                     static_cast<float>(static_cast<short>(HIWORD(lparam)))},
                    true);
                for (const auto& event : events) {
                    if (event.type == nengine::ui::UiPointerEventType::Down) {
                        self->ui_pointer_captured = true;
                        SetCapture(hwnd);
                        return 0;
                    }
                }
            }
            self->begin_scene_pointer(
                hwnd,
                static_cast<int>(
                    static_cast<short>(LOWORD(lparam))),
                static_cast<int>(
                    static_cast<short>(HIWORD(lparam))));
            return 0;

        case WM_MOUSEMOVE:
            if (self->canvas_dragging) {
                const int x = static_cast<short>(LOWORD(lparam));
                const int y = static_cast<short>(HIWORD(lparam));
                self->editor.ui_edit_update_drag(
                    {static_cast<float>(x - self->canvas_drag_start_x),
                     static_cast<float>(y - self->canvas_drag_start_y)},
                    self->canvas_drag_resize);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (!self->vulkan_preview_enabled &&
                self->editor.play_session().is_playing() &&
                self->editor.has_ui_canvas() &&
                !self->scene_dragging) {
                RECT bounds{};
                GetClientRect(hwnd, &bounds);
                self->editor.route_ui_pointer(
                    static_cast<float>(bounds.right),
                    static_cast<float>(bounds.bottom),
                    {static_cast<float>(static_cast<short>(LOWORD(lparam))),
                     static_cast<float>(static_cast<short>(HIWORD(lparam)))},
                    (wparam & MK_LBUTTON) != 0);
            }
            if (self->scene_dragging) {
                self->update_scene_drag(
                    static_cast<int>(
                        static_cast<short>(LOWORD(lparam))),
                    static_cast<int>(
                        static_cast<short>(HIWORD(lparam))));
                return 0;
            }
            break;

        case WM_LBUTTONUP:
            if (self->canvas_dragging) {
                self->canvas_dragging = false;
                self->editor.ui_edit_end_drag(true);
                if (GetCapture() == hwnd) ReleaseCapture();
                self->refresh();
                return 0;
            }
            if (self->ui_pointer_captured) {
                RECT bounds{};
                GetClientRect(hwnd, &bounds);
                const auto events = self->editor.route_ui_pointer(
                    static_cast<float>(bounds.right),
                    static_cast<float>(bounds.bottom),
                    {static_cast<float>(static_cast<short>(LOWORD(lparam))),
                     static_cast<float>(static_cast<short>(HIWORD(lparam)))},
                    false);
                for (const auto& event : events)
                    if (event.type == nengine::ui::UiPointerEventType::Click)
                        self->editor.console().info(
                            "UI", "Canvas button clicked: " +
                                std::to_string(event.target));
                self->ui_pointer_captured = false;
                if (GetCapture() == hwnd) ReleaseCapture();
                return 0;
            }
            if (self->scene_dragging) {
                self->end_scene_drag();
                return 0;
            }
            break;

        case WM_CAPTURECHANGED:
            if (self->canvas_dragging &&
                reinterpret_cast<HWND>(lparam) != hwnd) {
                self->canvas_dragging = false;
                self->editor.ui_edit_end_drag(false);
                InvalidateRect(hwnd, nullptr, FALSE);
                return 0;
            }
            if (self->ui_pointer_captured &&
                reinterpret_cast<HWND>(lparam) != hwnd) {
                self->ui_pointer_captured = false;
                self->editor.cancel_ui_pointer_capture();
                return 0;
            }
            if (self->scene_dragging &&
                reinterpret_cast<HWND>(lparam) != hwnd) {
                self->cancel_scene_drag();
                return 0;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);

            if(hwnd==self->game_view || !self->vulkan_preview_enabled) {
                self->paint_scene(dc, hwnd);
            }

            EndPaint(hwnd, &paint);
            return 0;
        }

        case WM_NCDESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            break;

        default:
            break;
        }

        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    static LRESULT CALLBACK splitter_proc(
        HWND hwnd,
        UINT message,
        WPARAM,
        LPARAM lparam) {

        auto* self = reinterpret_cast<Impl*>(
            GetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA));

        if (message == WM_NCCREATE) {
            const auto* create =
                reinterpret_cast<CREATESTRUCTW*>(
                    lparam);

            self =
                static_cast<Impl*>(
                    create->lpCreateParams);

            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<LONG_PTR>(
                    self));
        }

        if (!self) {
            return DefWindowProcW(
                hwnd,
                message,
                0,
                lparam);
        }

        switch (message) {
        case WM_SETCURSOR: {
            const int id =
                GetDlgCtrlID(hwnd);

            SetCursor(
                LoadCursorW(
                    nullptr,
                    id == IdSplitBottom
                        ? IDC_SIZENS
                        : IDC_SIZEWE));

            return TRUE;
        }

        case WM_LBUTTONDOWN: {
            POINT point{};
            GetCursorPos(&point);
            ScreenToClient(
                self->host,
                &point);

            self->begin_layout_drag(
                GetDlgCtrlID(hwnd),
                point.x,
                point.y);

            SetCapture(hwnd);
            return 0;
        }

        case WM_MOUSEMOVE:
            if (self->layout_drag !=
                    LayoutDrag::None &&
                GetCapture() == hwnd) {

                POINT point{};
                GetCursorPos(&point);
                ScreenToClient(
                    self->host,
                    &point);

                self->update_layout_drag(
                    point.x,
                    point.y);

                return 0;
            }
            break;

        case WM_LBUTTONUP:
            if (self->layout_drag !=
                LayoutDrag::None) {

                self->end_layout_drag();

                if (GetCapture() == hwnd) {
                    ReleaseCapture();
                }

                return 0;
            }
            break;

        case WM_CAPTURECHANGED:
            if (self->layout_drag !=
                LayoutDrag::None) {
                self->end_layout_drag();
                return 0;
            }
            break;

        case WM_NCDESTROY:
            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                0);
            break;

        default:
            break;
        }

        return DefWindowProcW(
            hwnd,
            message,
            0,
            lparam);
    }

    bool attach(void* native) {
        shell_log("attach begin");

        parent = static_cast<HWND>(native);

        if (editor.project().is_open()) {
            current_scene_path =
                editor.project().startup_scene_path();
        }

        if (!parent || !IsWindow(parent)) {
            shell_log("attach failed: invalid parent HWND");
            SetLastError(ERROR_INVALID_WINDOW_HANDLE);
            return false;
        }

        shell_log("registering EditorHost window class");
        if (!register_basic_class(
                kHostClassName,
                &Impl::host_proc,
                reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1),
                LoadCursorW(nullptr, IDC_ARROW))) {
            shell_log("attach failed: RegisterClassExW(EditorHost)");
            return false;
        }

        shell_log("registering SceneView window class");
        if (!register_basic_class(
                kSceneClassName,
                &Impl::scene_proc,
                reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1),
                LoadCursorW(nullptr, IDC_CROSS))) {
            shell_log("attach failed: RegisterClassExW(SceneView)");
            return false;
        }

        shell_log("registering Splitter window class");
        if (!register_basic_class(
                kSplitterClassName,
                &Impl::splitter_proc,
                reinterpret_cast<HBRUSH>(COLOR_3DSHADOW + 1),
                LoadCursorW(nullptr, IDC_ARROW))) {
            shell_log("attach failed: RegisterClassExW(Splitter)");
            return false;
        }

        RECT parent_client{};
        if (!GetClientRect(parent, &parent_client)) {
            shell_log("attach failed: GetClientRect(parent)");
            return false;
        }

        const int parent_width = parent_client.right - parent_client.left;
        const int parent_height = parent_client.bottom - parent_client.top;

        shell_log("creating EditorHost child window");
        SetLastError(ERROR_SUCCESS);
        host = CreateWindowExW(
            0,
            kHostClassName,
            L"",
            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0,
            0,
            parent_width,
            parent_height,
            parent,
            nullptr,
            GetModuleHandleW(nullptr),
            this);

        if (!host) {
            shell_log("attach failed: CreateWindowExW(EditorHost)");
            return false;
        }

        last_parent_width = parent_width;
        last_parent_height = parent_height;

        shell_log("creating toolbar controls");
        open_scene = create_control(host, L"BUTTON", L"Open", BS_PUSHBUTTON, IdOpen);
        save_scene = create_control(host, L"BUTTON", L"Save", BS_PUSHBUTTON, IdSave);
        new_entity = create_control(host, L"BUTTON", L"New", BS_PUSHBUTTON, IdNewEntity);
        delete_entity = create_control(host, L"BUTTON", L"Delete", BS_PUSHBUTTON, IdDeleteEntity);
        refresh_assets = create_control(host, L"BUTTON", L"Assets", BS_PUSHBUTTON, IdRefreshAssets);
        scripts = create_control(host, L"BUTTON", L"Scripts", BS_PUSHBUTTON, IdScripts);
        build_scripts = create_control(host, L"BUTTON", L"Build C#", BS_PUSHBUTTON, IdBuildScripts);
        undo = create_control(host, L"BUTTON", L"Undo", BS_PUSHBUTTON, IdUndo);
        redo = create_control(host, L"BUTTON", L"Redo", BS_PUSHBUTTON, IdRedo);
        play = create_control(host, L"BUTTON", L"Play", BS_PUSHBUTTON, IdPlay);
        pause = create_control(host, L"BUTTON", L"Pause", BS_PUSHBUTTON, IdPause);
        step = create_control(host, L"BUTTON", L"Step", BS_PUSHBUTTON, IdStep);
        stop = create_control(host, L"BUTTON", L"Stop", BS_PUSHBUTTON, IdStop);
        game_toggle = create_control(host,L"BUTTON",L"Game View",BS_AUTOCHECKBOX | BS_PUSHLIKE,IdGameView);
        action_toggle = create_control(host,L"BUTTON",L"Input Actions",BS_AUTOCHECKBOX | BS_PUSHLIKE,IdActionEditor);
        build_toggle = create_control(host,L"BUTTON",L"Build Settings",BS_AUTOCHECKBOX | BS_PUSHLIKE,IdBuildSettings);
        package_toggle = create_control(host,L"BUTTON",L"Packages",BS_AUTOCHECKBOX | BS_PUSHLIKE,IdPackages);
        profiler_toggle = create_control(host,L"BUTTON",L"Profiler",BS_AUTOCHECKBOX | BS_PUSHLIKE,IdProfiler);
        vulkan_preview = create_control(
            host,
            L"BUTTON",
            L"VK Preview",
            BS_AUTOCHECKBOX | BS_PUSHLIKE,
            IdVulkanPreview);

        if (!package_toggle || !game_toggle || !action_toggle || !build_toggle || !profiler_toggle || !open_scene || !save_scene || !new_entity || !delete_entity ||
            !refresh_assets || !scripts || !build_scripts || !undo || !redo ||
            !play || !pause || !step || !stop ||
            !vulkan_preview) {
            shell_log("attach failed: toolbar control creation");
            return false;
        }

        shell_log("creating layout splitters");

        split_hierarchy = CreateWindowExW(
            0,
            kSplitterClassName,
            L"",
            WS_CHILD | WS_VISIBLE,
            0,
            0,
            kSplitterSize,
            10,
            host,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IdSplitHierarchy)),
            GetModuleHandleW(nullptr),
            this);

        split_inspector = CreateWindowExW(
            0,
            kSplitterClassName,
            L"",
            WS_CHILD | WS_VISIBLE,
            0,
            0,
            kSplitterSize,
            10,
            host,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IdSplitInspector)),
            GetModuleHandleW(nullptr),
            this);

        split_bottom = CreateWindowExW(
            0,
            kSplitterClassName,
            L"",
            WS_CHILD | WS_VISIBLE,
            10,
            10,
            10,
            kSplitterSize,
            host,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(
                    IdSplitBottom)),
            GetModuleHandleW(nullptr),
            this);

        if (!split_hierarchy ||
            !split_inspector ||
            !split_bottom) {
            shell_log(
                "attach failed: splitter creation");
            return false;
        }

        shell_log("creating hierarchy control");
        hierarchy = create_control(
            host,
            L"LISTBOX",
            L"",
            LBS_NOTIFY | LBS_EXTENDEDSEL | WS_BORDER | WS_VSCROLL,
            IdHierarchy);
        duplicate_hierarchy=create_control(host,L"BUTTON",L"Duplicate",BS_PUSHBUTTON,IdDuplicateHierarchy);
        parent_hierarchy=create_control(host,L"BUTTON",L"Parent to active",BS_PUSHBUTTON,IdParentHierarchy);

        if (!hierarchy||!duplicate_hierarchy||!parent_hierarchy) {
            shell_log("attach failed: hierarchy control creation");
            return false;
        }

        shell_log("creating SceneView child window");
        SetLastError(ERROR_SUCCESS);
        scene = CreateWindowExW(
            0,
            kSceneClassName,
            L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            0,
            0,
            10,
            10,
            host,
            nullptr,
            GetModuleHandleW(nullptr),
            this);

        if (!scene) {
            shell_log("attach failed: SceneView creation");
            return false;
        }

        game_view = CreateWindowExW(0,kSceneClassName,L"Game View",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            0,0,10,10,host,nullptr,GetModuleHandleW(nullptr),this);
        if(!game_view) {shell_log("attach failed: Game View window");return false;}
        SendMessageW(game_toggle,BM_SETCHECK,BST_CHECKED,0);
        shell_log("creating inspector controls");
        inspector_title = create_control(host, L"STATIC", L"Inspector", SS_LEFT);
        label_name = create_control(host, L"STATIC", L"Name", SS_LEFT);
        name = create_control(host, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, IdName);
        active = create_control(host, L"BUTTON", L"Active", BS_AUTOCHECKBOX, IdActive);

        label_position = create_control(host, L"STATIC", L"Position X / Y / Z", SS_LEFT);
        position[0] = create_control(host, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdPosX);
        position[1] = create_control(host, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdPosY);
        position[2] = create_control(host, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdPosZ);

        label_rotation = create_control(host, L"STATIC", L"Rotation X / Y / Z / W", SS_LEFT);
        rotation[0] = create_control(host, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdRotX);
        rotation[1] = create_control(host, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdRotY);
        rotation[2] = create_control(host, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdRotZ);
        rotation[3] = create_control(host, L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL, IdRotW);

        label_scale = create_control(host, L"STATIC", L"Scale X / Y / Z", SS_LEFT);
        scale[0] = create_control(host, L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL, IdScaleX);
        scale[1] = create_control(host, L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL, IdScaleY);
        scale[2] = create_control(host, L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL, IdScaleZ);

        apply_transform = create_control(
            host,
            L"BUTTON",
            L"Apply Transform",
            BS_PUSHBUTTON,
            IdApplyTransform);

        generic_title = create_control(
            host,
            L"STATIC",
            L"Reflection Properties",
            SS_LEFT);

        generic_properties = create_control(
            host,
            L"LISTBOX",
            L"",
            LBS_NOTIFY | WS_BORDER |
                WS_VSCROLL | WS_HSCROLL,
            IdGenericProperties);

        generic_value = create_control(
            host,
            L"EDIT",
            L"",
            WS_BORDER | ES_AUTOHSCROLL,
            IdGenericValue);

        assign_asset = create_control(
            host,
            L"BUTTON",
            L"Assign Asset",
            BS_PUSHBUTTON,
            IdAssignAsset);

        apply_property = create_control(
            host,
            L"BUTTON",
            L"Apply Property",
            BS_PUSHBUTTON,
            IdApplyProperty);

        remove_component=create_control(host,L"BUTTON",L"Remove Component",
            BS_PUSHBUTTON,IdRemoveComponent);

        add_component_list = create_control(
            host,
            L"COMBOBOX",
            L"",
            CBS_DROPDOWNLIST | WS_VSCROLL,
            IdAddComponentList);

        add_component = create_control(
            host,
            L"BUTTON",
            L"Add Component",
            BS_PUSHBUTTON,
            IdAddComponent);

        if (!inspector_title || !label_name || !name || !active ||
            !label_position || !position[0] || !position[1] || !position[2] ||
            !label_rotation || !rotation[0] || !rotation[1] ||
            !rotation[2] || !rotation[3] ||
            !label_scale || !scale[0] || !scale[1] || !scale[2] ||
            !apply_transform ||
            !generic_title ||
            !generic_properties ||
            !generic_value ||
            !assign_asset ||
            !apply_property || !remove_component ||
            !add_component_list ||
            !add_component) {
            shell_log("attach failed: inspector control creation");
            return false;
        }

        canvas_edit_toggle = create_control(
            host, L"BUTTON", L"Canvas Edit", BS_PUSHBUTTON, IdCanvasEdit);
        canvas_name = create_control(
            host, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, IdCanvasName);
        canvas_text = create_control(
            host, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, IdCanvasText);
        canvas_apply = create_control(
            host, L"BUTTON", L"Apply Name + Text", BS_PUSHBUTTON, IdCanvasApply);
        canvas_save = create_control(
            host, L"BUTTON", L"Save Canvas", BS_PUSHBUTTON, IdCanvasSave);
        canvas_add_button = create_control(
            host, L"BUTTON", L"+ Button", BS_PUSHBUTTON, IdCanvasAddButton);
        canvas_add_text = create_control(
            host, L"BUTTON", L"+ Text", BS_PUSHBUTTON, IdCanvasAddText);
        canvas_add_image = create_control(
            host, L"BUTTON", L"+ Image", BS_PUSHBUTTON, IdCanvasAddImage);
        canvas_image_guid = create_control(
            host, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL | ES_READONLY,
            IdCanvasImageGuid);
        canvas_assign_image = create_control(
            host, L"BUTTON", L"Use Selected Texture",
            BS_PUSHBUTTON, IdCanvasAssignImage);
        canvas_clear_image = create_control(
            host, L"BUTTON", L"Clear Image", BS_PUSHBUTTON, IdCanvasClearImage);
        canvas_delete = create_control(
            host, L"BUTTON", L"Delete UI", BS_PUSHBUTTON, IdCanvasDelete);
        canvas_undo = create_control(
            host, L"BUTTON", L"UI Undo", BS_PUSHBUTTON, IdCanvasUndo);
        canvas_redo = create_control(
            host, L"BUTTON", L"UI Redo", BS_PUSHBUTTON, IdCanvasRedo);

        canvas_widget_list = create_control(
            host, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
            IdCanvasWidgetList);
        canvas_duplicate = create_control(
            host, L"BUTTON", L"Duplicate", BS_PUSHBUTTON, IdCanvasDuplicate);
        canvas_to_back = create_control(
            host, L"BUTTON", L"To Back", BS_PUSHBUTTON, IdCanvasBack);
        canvas_to_front = create_control(
            host, L"BUTTON", L"To Front", BS_PUSHBUTTON, IdCanvasFront);
        canvas_parent_combo = create_control(
            host, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
            IdCanvasParent);
        canvas_apply_parent = create_control(
            host, L"BUTTON", L"Reparent", BS_PUSHBUTTON,
            IdCanvasApplyParent);
        canvas_preset = create_control(
            host, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_VSCROLL,
            IdCanvasPreset);
        canvas_apply_preset = create_control(
            host, L"BUTTON", L"Apply Preset", BS_PUSHBUTTON,
            IdCanvasApplyPreset);
        for (const wchar_t* label : {
                 L"Top Left", L"Top Center", L"Top Right",
                 L"Middle Left", L"Middle Center", L"Middle Right",
                 L"Bottom Left", L"Bottom Center", L"Bottom Right",
                 L"Stretch Horizontal", L"Stretch Vertical", L"Stretch All"})
            SendMessageW(canvas_preset, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(label));
        SendMessageW(canvas_preset, CB_SETCURSEL, 0, 0);
        canvas_anchor_label = create_control(
            host, L"STATIC", L"Anchor min XY / max XY", 0);
        canvas_offset_label = create_control(
            host, L"STATIC", L"Offsets left top right bottom", 0);
        canvas_color_label = create_control(
            host, L"STATIC", L"Color R G B A (0..1)", 0);
        for (std::size_t i = 0; i < canvas_anchor_inputs.size(); ++i)
            canvas_anchor_inputs[i] = create_control(
                host, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                IdCanvasAnchorBase + static_cast<int>(i));
        for (std::size_t i = 0; i < canvas_color_inputs.size(); ++i)
            canvas_color_inputs[i] = create_control(
                host, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL,
                IdCanvasColorBase + static_cast<int>(i));
        canvas_apply_geometry = create_control(
            host, L"BUTTON", L"Apply Anchors + RGBA",
            BS_PUSHBUTTON, IdCanvasApplyGeometry);

        if (!canvas_parent_combo || !canvas_apply_parent ||
            !canvas_preset || !canvas_apply_preset ||
            !canvas_widget_list || !canvas_duplicate ||
            !canvas_to_back || !canvas_to_front ||
            !canvas_anchor_label || !canvas_offset_label ||
            !canvas_color_label || !canvas_apply_geometry ||
            std::any_of(canvas_anchor_inputs.begin(),
                        canvas_anchor_inputs.end(),
                        [](HWND control) { return !control; }) ||
            std::any_of(canvas_color_inputs.begin(),
                        canvas_color_inputs.end(),
                        [](HWND control) { return !control; }) ||
            !canvas_edit_toggle || !canvas_name || !canvas_text ||
            !canvas_apply || !canvas_save || !canvas_add_button ||
            !canvas_add_text || !canvas_add_image || !canvas_image_guid ||
            !canvas_assign_image || !canvas_clear_image ||
            !canvas_delete || !canvas_undo ||
            !canvas_redo) {
            shell_log("attach failed: Canvas authoring controls");
            return false;
        }

        shell_log("creating Assets and Console controls");

        assets_list = create_control(
            host,
            L"LISTBOX",
            L"",
            LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_HSCROLL,
            IdAssets);

        console = create_control(
            host,
            L"LISTBOX",
            L"",
            LBS_NOTIFY | WS_BORDER | WS_VSCROLL | WS_HSCROLL,
            IdConsole);

        action_list = create_control(host,L"LISTBOX",L"",
            LBS_NOTIFY | WS_BORDER | WS_VSCROLL,IdActionList);
        action_name = create_control(host,L"EDIT",L"",
            WS_BORDER | ES_AUTOHSCROLL,IdActionName);
        action_key = create_control(host,L"COMBOBOX",L"",
            CBS_DROPDOWNLIST | WS_VSCROLL,IdActionKey);
        action_add = create_control(host,L"BUTTON",L"Add Binding",BS_PUSHBUTTON,IdActionAdd);
        action_remove = create_control(host,L"BUTTON",L"Remove Action",BS_PUSHBUTTON,IdActionRemove);
        action_save = create_control(host,L"BUTTON",L"Save Actions",BS_PUSHBUTTON,IdActionSave);
        action_key_options.push_back(nengine::input::Key::Space);
        SendMessageW(action_key,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Space"));
        action_key_options.push_back(nengine::input::Key::Enter);
        SendMessageW(action_key,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"Enter"));
        for(int i=0;i<26;++i) {
            action_key_options.push_back(static_cast<nengine::input::Key>(
                static_cast<int>(nengine::input::Key::A)+i));
            wchar_t label[2]{static_cast<wchar_t>(L'A'+i),0};
            SendMessageW(action_key,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        }
        SendMessageW(action_key,CB_SETCURSEL,0,0);
        profiler_list=create_control(host,L"LISTBOX",L"",
            WS_BORDER | WS_VSCROLL | WS_HSCROLL,0);
        build_name=create_control(host,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,IdBuildName);
        build_windows=create_control(host,L"BUTTON",L"Windows Player",BS_AUTOCHECKBOX,IdBuildWindows);
        build_android=create_control(host,L"BUTTON",L"Android (toolchain profile)",BS_AUTOCHECKBOX,IdBuildAndroid);
        build_toolchain=create_control(host,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,IdBuildToolchain);
        build_apply=create_control(host,L"BUTTON",L"Save Target Settings",BS_PUSHBUTTON,IdBuildApply);
        android_sdk_path=create_control(host,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,IdAndroidSdkPath);
        android_ndk_path=create_control(host,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,IdAndroidNdkPath);
        android_jdk_path=create_control(host,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,IdAndroidJdkPath);
        android_paths_apply=create_control(host,L"BUTTON",L"Validate and Save SDK / NDK / JDK",BS_PUSHBUTTON,IdAndroidPathsApply);
        package_list=create_control(host,L"LISTBOX",L"",WS_BORDER|LBS_NOTIFY|WS_VSCROLL,IdPackageList);
        package_id=create_control(host,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,IdPackageId);
        package_version=create_control(host,L"EDIT",L"",WS_BORDER|ES_AUTOHSCROLL,IdPackageVersion);
        package_add=create_control(host,L"BUTTON",L"Install / Update",BS_PUSHBUTTON,IdPackageAdd);
        package_remove=create_control(host,L"BUTTON",L"Remove",BS_PUSHBUTTON,IdPackageRemove);
        if(!package_list||!package_id||!package_version||!package_add||!package_remove||
            !android_sdk_path||!android_ndk_path||!android_jdk_path||!android_paths_apply||
            !profiler_list||!build_name||!build_windows||!build_android||!build_toolchain||!build_apply||
            !action_list || !action_name || !action_key || !action_add ||
            !action_remove || !action_save ||
            !assets_list || !console) {
            shell_log("attach failed: bottom panel control creation");
            return false;
        }

        controls_ready = true;

        shell_log("performing initial layout");
        RECT host_client{};
        if (!GetClientRect(host, &host_client)) {
            shell_log("attach failed: GetClientRect(host)");
            return false;
        }

        layout(
            host_client.right - host_client.left,
            host_client.bottom - host_client.top);

        editor.console().info(
            "Editor",
            "Editor host attached.");

        initialize_vulkan_scene_context();

        refresh_assets_panel();
        refresh_console();

        shell_log("attach complete");
        return true;
    }

    void initialize_vulkan_scene_context() {
        if (!scene ||
            !IsWindow(scene)) {
            return;
        }

        RECT rect{};

        if (!GetClientRect(
                scene,
                &rect)) {
            return;
        }

        const int width =
            std::max(
                1,
                static_cast<int>(
                    rect.right -
                    rect.left));

        const int height =
            std::max(
                1,
                static_cast<int>(
                    rect.bottom -
                    rect.top));

        auto context =
            std::make_unique<
                nengine::render::
                    VulkanContext>();

        std::unique_ptr<
            nengine::render::
                VulkanDiagnosticScene>
            diagnostic_scene;

        if (context->initialize_for_window(
                GetModuleHandleW(nullptr),
                scene,
                static_cast<std::uint32_t>(
                    width),
                static_cast<std::uint32_t>(
                    height),
                true)) {

            vulkan_scene_width =
                width;

            vulkan_scene_height =
                height;

            editor.console().info(
                "Renderer",
                "Vulkan bootstrap ready: " +
                    std::to_string(width) +
                    "x" +
                    std::to_string(height) +
                    ", " +
                    std::to_string(
                        context
                            ->swapchain()
                            .images()
                            .size()) +
                    " swapchain image(s).");

            diagnostic_scene =
                std::make_unique<
                    nengine::render::
                        VulkanDiagnosticScene>();

            if (diagnostic_scene->initialize(
                    *context)) {

                editor.console().info(
                    "Renderer",
                    "Vulkan diagnostic SPIR-V, GPU mesh and indexed graphics pipeline are ready. GDI Scene View presentation remains active for interaction.");

            } else {
                editor.console().warning(
                    "Renderer",
                    "Vulkan window context is ready, but diagnostic draw resources failed: " +
                        diagnostic_scene
                            ->diagnostic());
            }
        } else {
            editor.console().info(
                "Renderer",
                "Vulkan bootstrap unavailable; using GDI Scene View: " +
                    context->diagnostic());
        }

        vulkan_context =
            std::move(context);

        vulkan_diagnostic_scene =
            std::move(
                diagnostic_scene);
    }

    void poll_vulkan_scene_resize() {
        if (!vulkan_context ||
            !vulkan_context
                ->instance()
                .valid() ||
            !vulkan_context
                ->device()
                .valid() ||
            !scene ||
            !IsWindow(scene)) {
            return;
        }

        RECT rect{};

        if (!GetClientRect(
                scene,
                &rect)) {
            return;
        }

        const int width =
            rect.right -
            rect.left;

        const int height =
            rect.bottom -
            rect.top;

        if (width ==
                vulkan_scene_width &&
            height ==
                vulkan_scene_height) {
            return;
        }

        vulkan_scene_width =
            width;

        vulkan_scene_height =
            height;

        if (vulkan_diagnostic_scene) {
            vulkan_diagnostic_scene
                ->shutdown();
        }

        if (!vulkan_context->resize(
                static_cast<std::uint32_t>(
                    std::max(0, width)),
                static_cast<std::uint32_t>(
                    std::max(0, height)))) {

            editor.console().warning(
                "Renderer",
                "Vulkan swapchain resize failed: " +
                    vulkan_context
                        ->diagnostic());

            refresh_console();
            return;
        }

        if (width > 0 &&
            height > 0 &&
            vulkan_diagnostic_scene &&
            !vulkan_diagnostic_scene
                ->initialize(
                    *vulkan_context)) {

            vulkan_preview_enabled =
                false;

            if (vulkan_preview) {
                SendMessageW(
                    vulkan_preview,
                    BM_SETCHECK,
                    BST_UNCHECKED,
                    0);
            }

            editor.console().warning(
                "Renderer",
                "Vulkan diagnostic resources failed after resize: " +
                    vulkan_diagnostic_scene
                        ->diagnostic());

            refresh_console();
        }
    }

    void tick() {
        if (!parent || !host || !IsWindow(parent) || !IsWindow(host)) {
            return;
        }

        const ULONGLONG now =
            GetTickCount64();

        double runtime_elapsed_seconds =
            0.0;

        if (last_runtime_tick != 0ull &&
            now >= last_runtime_tick) {

            runtime_elapsed_seconds =
                static_cast<double>(
                    now -
                    last_runtime_tick) /
                1000.0;
        }

        last_runtime_tick =
            now;

        const auto profile_start=std::chrono::steady_clock::now();
        editor.tick_runtime(runtime_elapsed_seconds);
        const double simulation_ms=
            std::chrono::duration<double,std::milli>(
                std::chrono::steady_clock::now()-profile_start).count();
        profiler_samples.sample({
            runtime_elapsed_seconds*1000.0,simulation_ms,
            editor.presentation_world().entities().size(),
            editor.play_session().is_playing()});
        if(game_view&&game_view_visible)
            InvalidateRect(game_view,nullptr,FALSE);
        if(profiler_visible&&now>=next_profiler_refresh){
            next_profiler_refresh=now+250;
            refresh_profiler();
        }

        RECT rect{};
        if (!GetClientRect(parent, &rect)) {
            return;
        }

        const int width = rect.right - rect.left;
        const int height = rect.bottom - rect.top;

        if (width != last_parent_width || height != last_parent_height) {
            last_parent_width = width;
            last_parent_height = height;
            MoveWindow(host, 0, 0, width, height, TRUE);
        }

        poll_vulkan_scene_resize();

        if (vulkan_preview_enabled &&
            vulkan_context &&
            vulkan_context->ready() &&
            vulkan_diagnostic_scene &&
            vulkan_diagnostic_scene->ready()) {

            if (!vulkan_diagnostic_scene
                    ->present_world(
                        *vulkan_context,
                        editor.presentation_world(),
                        [this](
                            nengine::assets::AssetGuid guid) {
                            return editor.project()
                                .cached_artifacts(
                                    guid);
                        })) {

                vulkan_preview_enabled =
                    false;

                if (vulkan_preview) {
                    SendMessageW(
                        vulkan_preview,
                        BM_SETCHECK,
                        BST_UNCHECKED,
                        0);
                }

                editor.console().warning(
                    "Renderer",
                    "Vulkan Preview stopped after present failure: " +
                        vulkan_diagnostic_scene
                            ->diagnostic());

                if (scene) {
                    InvalidateRect(
                        scene,
                        nullptr,
                        TRUE);
                }

                refresh_console();
            }
        }

        if (now >= next_asset_poll_tick) {
            next_asset_poll_tick = now + 1000ull;

            const auto result =
                editor.project().poll_assets();

            if (!result.changes.empty()) {
                if (vulkan_diagnostic_scene) {
                    vulkan_diagnostic_scene
                        ->invalidate_imported_assets();
                }

                editor.invalidate_runtime_asset_caches();

                editor.console().info(
                    "Assets",
                    "Detected " +
                        std::to_string(result.changes.size()) +
                        " filesystem change(s).");

                for (const auto& message : result.scan.messages) {
                    const auto path =
                        wide_to_utf8(
                            message.path.wstring());

                    switch (message.severity) {
                    case nengine::assets::AssetMessageSeverity::Info:
                        editor.console().info(
                            "Assets",
                            path + ": " + message.message);
                        break;
                    case nengine::assets::AssetMessageSeverity::Warning:
                        editor.console().warning(
                            "Assets",
                            path + ": " + message.message);
                        break;
                    case nengine::assets::AssetMessageSeverity::Error:
                        editor.console().error(
                            "Assets",
                            path + ": " + message.message);
                        break;
                    }
                }

                if (result.imports.attempted != 0 ||
                    result.imports.unsupported != 0) {
                    editor.console().info(
                        "Assets",
                        "Auto-import: " +
                            std::to_string(
                                result.imports.imported) +
                            " imported, " +
                            std::to_string(
                                result.imports.cache_hits) +
                            " cache hit(s), " +
                            std::to_string(
                                result.imports.failed) +
                            " failed, " +
                            std::to_string(
                                result.imports.unsupported) +
                            " unsupported.");
                }

                refresh_assets_panel();
                refresh_console();
            }
        }
    }

    void begin_layout_drag(
        int control_id,
        int,
        int) {

        switch (control_id) {
        case IdSplitHierarchy:
            layout_drag =
                LayoutDrag::Hierarchy;
            break;
        case IdSplitInspector:
            layout_drag =
                LayoutDrag::Inspector;
            break;
        case IdSplitBottom:
            layout_drag =
                LayoutDrag::Bottom;
            break;
        default:
            layout_drag =
                LayoutDrag::None;
            break;
        }
    }

    void update_layout_drag(
        int x,
        int y) {

        if (layout_drag ==
            LayoutDrag::None) {
            return;
        }

        RECT rect{};
        if (!GetClientRect(
                host,
                &rect)) {
            return;
        }

        const int width =
            rect.right - rect.left;

        const int height =
            rect.bottom - rect.top;

        auto& state =
            editor.layout();

        switch (layout_drag) {
        case LayoutDrag::Hierarchy:
            state.hierarchy_width = x;
            break;

        case LayoutDrag::Inspector:
            state.inspector_width =
                width - x;
            break;

        case LayoutDrag::Bottom:
            state.bottom_height =
                height - y;
            break;

        case LayoutDrag::None:
            break;
        }

        state.clamp(
            width,
            height);

        layout(
            width,
            height);
    }

    void end_layout_drag() {
        if (layout_drag ==
            LayoutDrag::None) {
            return;
        }

        layout_drag =
            LayoutDrag::None;

        if (editor.project().is_open()) {
            const auto path =
                editor.project().root() /
                "ProjectSettings" /
                "EditorLayout.layout";

            std::string error;

            if (!nengine::editor::
                    EditorLayoutSerializer::save(
                        editor.layout(),
                        path,
                        &error)) {

                editor.console().warning(
                    "Layout",
                    "Could not save editor layout: " +
                        error);
            } else {
                editor.console().info(
                    "Layout",
                    "Editor pane layout saved.");
            }

            refresh_console();
        }
    }

    void layout(int width, int height) {
        if (!controls_ready) return;

        width =
            (width < 900)
                ? 900
                : width;

        height =
            (height < 600)
                ? 600
                : height;

        auto& state =
            editor.layout();

        state.clamp(
            width,
            height);

        const int hierarchy_width =
            state.hierarchy_width;

        const int inspector_width =
            state.inspector_width;

        const int bottom_height =
            state.bottom_height;

        int x = kPadding;

        constexpr int button_width = 72;
        constexpr int button_height = 26;

        MoveWindow(
            open_scene,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            save_scene,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            new_entity,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            delete_entity,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            refresh_assets,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            scripts,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            build_scripts,
            x,
            6,
            84,
            button_height,
            TRUE);
        x += 84 + 16;

        MoveWindow(
            undo,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            redo,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 16;

        MoveWindow(
            play,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            pause,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            step,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 4;

        MoveWindow(
            stop,
            x,
            6,
            button_width,
            button_height,
            TRUE);
        x += button_width + 12;

        MoveWindow(
            vulkan_preview,
            x,
            6,
            88,
            button_height,
            TRUE);

        MoveWindow(game_toggle,kPadding,38,110,26,TRUE);
        MoveWindow(action_toggle,kPadding+116,38,120,26,TRUE);
        MoveWindow(build_toggle,kPadding+242,38,122,26,TRUE);
        MoveWindow(profiler_toggle,kPadding+370,38,110,26,TRUE);
        MoveWindow(package_toggle,kPadding+486,38,110,26,TRUE);
        const int content_top =
            kToolbarHeight;

        const int bottom_top =
            height - bottom_height;

        const int bottom_split_y =
            bottom_top -
            kSplitterSize / 2;

        const int content_height =
            bottom_split_y -
            content_top;

        const int left_split_x =
            hierarchy_width -
            kSplitterSize / 2;

        const int inspector_x =
            width - inspector_width;

        const int right_split_x =
            inspector_x -
            kSplitterSize / 2;

        MoveWindow(
            split_hierarchy,
            left_split_x,
            content_top,
            kSplitterSize,
            content_height,
            TRUE);

        MoveWindow(
            split_inspector,
            right_split_x,
            content_top,
            kSplitterSize,
            content_height,
            TRUE);

        MoveWindow(
            split_bottom,
            kPadding,
            bottom_split_y,
            width - 2 * kPadding,
            kSplitterSize,
            TRUE);

        MoveWindow(
            hierarchy,
            kPadding,
            content_top,
            std::max(
                10,
                left_split_x -
                    2 * kPadding),
            std::max(10,content_height-32),
            TRUE);
        const int hierarchy_buttons_width=std::max(40,left_split_x-2*kPadding);
        MoveWindow(duplicate_hierarchy,kPadding,bottom_split_y-30,
            std::max(20,hierarchy_buttons_width/2-2),26,TRUE);
        MoveWindow(parent_hierarchy,kPadding+hierarchy_buttons_width/2+2,
            bottom_split_y-30,std::max(20,hierarchy_buttons_width/2-2),26,TRUE);

        const int scene_x =
            left_split_x +
            kSplitterSize;

        const int available_scene_width = std::max(20,right_split_x-scene_x);
        const int edit_width = game_view_visible ?
            std::max(10,(available_scene_width-6)/2):available_scene_width;
        MoveWindow(scene,scene_x,content_top,edit_width,content_height,TRUE);
        ShowWindow(game_view,game_view_visible?SW_SHOW:SW_HIDE);
        if(game_view_visible)
            MoveWindow(game_view,scene_x+edit_width+6,content_top,
                std::max(10,available_scene_width-edit_width-6),content_height,TRUE);

        int iy =
            content_top + 8;

        const int ix =
            inspector_x + 10;

        const int iw =
            inspector_width - 20;

        MoveWindow(canvas_edit_toggle, ix, iy, iw, 26, TRUE);
        iy += 32;
        const bool ui_authoring = canvas_mode && editor.has_ui_canvas_draft();
        const std::array<HWND, 13> ui_controls{
            canvas_name, canvas_text, canvas_apply, canvas_save,
            canvas_add_button, canvas_add_text, canvas_add_image,
            canvas_image_guid, canvas_assign_image, canvas_clear_image,
            canvas_delete,
            canvas_undo, canvas_redo};
        for (HWND control : ui_controls)
            ShowWindow(control, ui_authoring ? SW_SHOW : SW_HIDE);
        for (HWND control : std::array<HWND, 4>{
                canvas_widget_list, canvas_duplicate,
                canvas_to_back, canvas_to_front})
            ShowWindow(control, ui_authoring ? SW_SHOW : SW_HIDE);
        ShowWindow(canvas_parent_combo, ui_authoring ? SW_SHOW : SW_HIDE);
        ShowWindow(canvas_apply_parent, ui_authoring ? SW_SHOW : SW_HIDE);
        ShowWindow(canvas_preset, ui_authoring ? SW_SHOW : SW_HIDE);
        ShowWindow(canvas_apply_preset, ui_authoring ? SW_SHOW : SW_HIDE);
        for (HWND control : canvas_anchor_inputs)
            ShowWindow(control, ui_authoring ? SW_SHOW : SW_HIDE);
        for (HWND control : canvas_color_inputs)
            ShowWindow(control, ui_authoring ? SW_SHOW : SW_HIDE);
        for (HWND control : std::array<HWND, 4>{
                canvas_anchor_label, canvas_offset_label,
                canvas_color_label, canvas_apply_geometry})
            ShowWindow(control, ui_authoring ? SW_SHOW : SW_HIDE);
        if (ui_authoring) {
            MoveWindow(canvas_widget_list, ix, iy, iw, 220, TRUE);
            iy += 32;
            MoveWindow(canvas_parent_combo, ix, iy, iw - 95, 200, TRUE);
            MoveWindow(canvas_apply_parent, ix + iw - 91, iy, 91, 24, TRUE);
            iy += 30;
            MoveWindow(canvas_name, ix, iy, iw, 24, TRUE); iy += 29;
            MoveWindow(canvas_text, ix, iy, iw, 24, TRUE); iy += 29;
            MoveWindow(canvas_apply, ix, iy, iw, 24, TRUE); iy += 28;
            MoveWindow(canvas_save, ix, iy, iw, 24, TRUE); iy += 29;
            const int half = (iw - 4) / 2;
            const int third = (iw - 8) / 3;
            MoveWindow(canvas_add_button, ix, iy, third, 24, TRUE);
            MoveWindow(canvas_add_text, ix + third + 4, iy, third, 24, TRUE);
            MoveWindow(canvas_add_image, ix + (third + 4) * 2, iy,
                       third, 24, TRUE);
            iy += 28;
            MoveWindow(canvas_image_guid, ix, iy, iw, 24, TRUE);
            iy += 28;
            MoveWindow(canvas_assign_image, ix, iy, half, 24, TRUE);
            MoveWindow(canvas_clear_image, ix + half + 4, iy, half, 24, TRUE);
            iy += 28;
            const int third_width = (iw - 8) / 3;
            MoveWindow(canvas_duplicate, ix, iy, third_width, 24, TRUE);
            MoveWindow(canvas_to_back, ix + third_width + 4, iy,
                       third_width, 24, TRUE);
            MoveWindow(canvas_to_front, ix + (third_width + 4) * 2,
                       iy, third_width, 24, TRUE);
            iy += 28;
            MoveWindow(canvas_delete, ix, iy, iw, 24, TRUE); iy += 28;
            MoveWindow(canvas_undo, ix, iy, half, 24, TRUE);
            MoveWindow(canvas_redo, ix + half + 4, iy, half, 24, TRUE);
            iy += 30;
            const int quarter_width = (iw - 12) / 4;
            const auto place_quad = [&](const auto& fields, int first) {
                for (int i = 0; i < 4; ++i)
                    MoveWindow(fields[static_cast<std::size_t>(first + i)],
                        ix + i * (quarter_width + 4), iy,
                        quarter_width, 24, TRUE);
                iy += 30;
            };
            MoveWindow(canvas_preset, ix, iy, iw - 95, 200, TRUE);
            MoveWindow(canvas_apply_preset, ix + iw - 91, iy, 91, 24, TRUE);
            iy += 30;
            MoveWindow(canvas_anchor_label, ix, iy, iw, 20, TRUE); iy += 20;
            place_quad(canvas_anchor_inputs, 0);
            MoveWindow(canvas_offset_label, ix, iy, iw, 20, TRUE); iy += 20;
            place_quad(canvas_anchor_inputs, 4);
            MoveWindow(canvas_color_label, ix, iy, iw, 20, TRUE); iy += 20;
            place_quad(canvas_color_inputs, 0);
            MoveWindow(canvas_apply_geometry, ix, iy, iw, 26, TRUE); iy += 32;
        }

        const std::array<HWND, 18> native_inspector{
            inspector_title, label_name, name, active, label_position,
            label_rotation, label_scale, apply_transform, generic_title,
            generic_properties, generic_value, assign_asset, apply_property,
            remove_component,add_component_list, add_component, position[0], position[1]};
        for (HWND control : native_inspector)
            ShowWindow(control, ui_authoring ? SW_HIDE : SW_SHOW);
        for (HWND control : position)
            ShowWindow(control, ui_authoring ? SW_HIDE : SW_SHOW);
        for (HWND control : rotation)
            ShowWindow(control, ui_authoring ? SW_HIDE : SW_SHOW);
        for (HWND control : scale)
            ShowWindow(control, ui_authoring ? SW_HIDE : SW_SHOW);
        if (!ui_authoring) {
        MoveWindow(
            inspector_title,
            ix,
            iy,
            iw,
            20,
            TRUE);
        iy += 28;

        MoveWindow(
            label_name,
            ix,
            iy + 4,
            70,
            20,
            TRUE);

        MoveWindow(
            name,
            ix + 72,
            iy,
            iw - 72,
            24,
            TRUE);
        iy += 32;

        MoveWindow(
            active,
            ix,
            iy,
            100,
            24,
            TRUE);
        iy += 34;

        MoveWindow(
            label_position,
            ix,
            iy + 4,
            iw,
            20,
            TRUE);
        iy += 24;

        const int third =
            (iw - 8) / 3;

        for (int i = 0; i < 3; ++i) {
            MoveWindow(
                position[
                    static_cast<std::size_t>(i)],
                ix + i * (third + 4),
                iy,
                third,
                24,
                TRUE);
        }
        iy += 34;

        MoveWindow(
            label_rotation,
            ix,
            iy + 4,
            iw,
            20,
            TRUE);
        iy += 24;

        const int quarter =
            (iw - 12) / 4;

        for (int i = 0; i < 4; ++i) {
            MoveWindow(
                rotation[
                    static_cast<std::size_t>(i)],
                ix + i * (quarter + 4),
                iy,
                quarter,
                24,
                TRUE);
        }
        iy += 34;

        MoveWindow(
            label_scale,
            ix,
            iy + 4,
            iw,
            20,
            TRUE);
        iy += 24;

        for (int i = 0; i < 3; ++i) {
            MoveWindow(
                scale[
                    static_cast<std::size_t>(i)],
                ix + i * (third + 4),
                iy,
                third,
                24,
                TRUE);
        }
        iy += 34;

        MoveWindow(
            apply_transform,
            ix,
            iy,
            iw,
            27,
            TRUE);
        iy += 36;

        MoveWindow(
            generic_title,
            ix,
            iy,
            iw,
            20,
            TRUE);
        iy += 22;

        const int inspector_bottom =
            bottom_split_y - kPadding;

        const int generic_list_height =
            std::max(
                60,
                inspector_bottom -
                    iy -
                    96);

        MoveWindow(
            generic_properties,
            ix,
            iy,
            iw,
            generic_list_height,
            TRUE);
        iy += generic_list_height + 4;

        const int assign_asset_width =
            std::min(
                96,
                iw / 3);

        const int generic_value_width =
            std::max(
                40,
                iw -
                    assign_asset_width -
                    4);

        MoveWindow(
            generic_value,
            ix,
            iy,
            generic_value_width,
            24,
            TRUE);

        MoveWindow(
            assign_asset,
            ix +
                generic_value_width +
                4,
            iy,
            assign_asset_width,
            24,
            TRUE);
        iy += 28;

        MoveWindow(apply_property,ix,iy,std::max(45,iw/2-2),27,TRUE);
        MoveWindow(remove_component,ix+iw/2+2,iy,
            std::max(45,iw/2-2),27,TRUE);
        iy += 34;

        const int add_button_width =
            std::min(
                110,
                iw / 3);

        const int add_combo_width =
            std::max(
                40,
                iw -
                    add_button_width -
                    4);

        MoveWindow(
            add_component_list,
            ix,
            iy,
            add_combo_width,
            120,
            TRUE);

        MoveWindow(
            add_component,
            ix +
                add_combo_width +
                4,
            iy,
            add_button_width,
            24,
            TRUE);

        }
        const int bottom_y =
            bottom_split_y +
            kSplitterSize +
            kPadding / 2;

        const int bottom_h =
            std::max(
                30,
                height -
                    bottom_y -
                    kPadding);

        const int assets_w =
            (width -
             3 * kPadding) *
            2 / 5;

        const int console_x =
            kPadding +
            assets_w +
            kPadding;

        MoveWindow(
            assets_list,
            kPadding,
            bottom_y,
            assets_w,
            bottom_h,
            TRUE);

        MoveWindow(
            console,
            console_x,
            bottom_y,
            width -
                console_x -
                kPadding,
            bottom_h,
            TRUE);
        const bool tool_open=action_editor_visible||build_settings_visible||profiler_visible||package_editor_visible;
        ShowWindow(assets_list,tool_open?SW_HIDE:SW_SHOW);
        ShowWindow(console,tool_open?SW_HIDE:SW_SHOW);
        const auto action_controls=std::array<HWND,6>{
            action_list,action_name,action_key,action_add,action_remove,action_save};
        for(auto ctrl:action_controls)ShowWindow(ctrl,action_editor_visible?SW_SHOW:SW_HIDE);
        const auto build_controls=std::array<HWND,9>{
            build_name,build_windows,build_android,build_toolchain,build_apply,
            android_sdk_path,android_ndk_path,android_jdk_path,android_paths_apply};
        for(auto ctrl:build_controls)ShowWindow(ctrl,build_settings_visible?SW_SHOW:SW_HIDE);
        const auto package_controls=std::array<HWND,5>{
            package_list,package_id,package_version,package_add,package_remove};
        for(auto ctrl:package_controls)ShowWindow(ctrl,package_editor_visible?SW_SHOW:SW_HIDE);
        if(package_editor_visible){
            const int pw=std::max(380,width-3*kPadding), half=pw/2;
            MoveWindow(package_list,kPadding,bottom_y,half-12,bottom_h,TRUE);
            MoveWindow(package_id,kPadding+half,bottom_y,pw-half,25,TRUE);
            MoveWindow(package_version,kPadding+half,bottom_y+34,pw-half,25,TRUE);
            MoveWindow(package_add,kPadding+half,bottom_y+68,(pw-half)/2-4,26,TRUE);
            MoveWindow(package_remove,kPadding+half+(pw-half)/2,bottom_y+68,(pw-half)/2,26,TRUE);
        }
        ShowWindow(profiler_list,profiler_visible?SW_SHOW:SW_HIDE);
        if(profiler_visible)
            MoveWindow(profiler_list,kPadding,bottom_y,width-2*kPadding,bottom_h,TRUE);
        if(build_settings_visible){
            const int w=std::max(160,width-3*kPadding);
            MoveWindow(build_name,kPadding,bottom_y,w/2,24,TRUE);
            MoveWindow(build_windows,kPadding+w/2+8,bottom_y,w/4,24,TRUE);
            MoveWindow(build_android,kPadding+w*3/4,bottom_y,w/4,24,TRUE);
            MoveWindow(build_toolchain,kPadding,bottom_y+33,w*2/3,25,TRUE);
            MoveWindow(build_apply,kPadding+w*2/3+8,bottom_y+33,w/3-8,25,TRUE);
            MoveWindow(android_sdk_path,kPadding,bottom_y+66,w,24,TRUE);
            MoveWindow(android_ndk_path,kPadding,bottom_y+96,w,24,TRUE);
            MoveWindow(android_jdk_path,kPadding,bottom_y+126,w,24,TRUE);
            MoveWindow(android_paths_apply,kPadding,bottom_y+156,w,24,TRUE);
        }
        if(action_editor_visible){
            const int x0=kPadding, w0=std::max(120,width/2-16);
            MoveWindow(action_list,x0,bottom_y,w0,bottom_h,TRUE);
            const int x1=x0+w0+8, w1=std::max(140,width-x1-kPadding);
            MoveWindow(action_name,x1,bottom_y,w1/2,25,TRUE);
            MoveWindow(action_key,x1+w1/2+4,bottom_y,w1/2-4,220,TRUE);
            MoveWindow(action_add,x1,bottom_y+29,w1/2-2,25,TRUE);
            MoveWindow(action_remove,x1+w1/2+4,bottom_y+29,w1/2-4,25,TRUE);
            MoveWindow(action_save,x1,bottom_y+61,w1,25,TRUE);
        }
    }

    void log_line(std::wstring_view message) {
        editor.console().info(
            "Editor",
            wide_to_utf8(message));

        if (controls_ready) {
            refresh_console();
        }
    }

    void refresh_console() {
        if (!console) return;

        SendMessageW(
            console,
            LB_RESETCONTENT,
            0,
            0);

        for (const auto& entry :
             editor.console().entries()) {

            std::string severity;
            switch (entry.severity) {
            case nengine::editor::LogSeverity::Trace:
                severity = "TRACE";
                break;
            case nengine::editor::LogSeverity::Info:
                severity = "INFO";
                break;
            case nengine::editor::LogSeverity::Warning:
                severity = "WARN";
                break;
            case nengine::editor::LogSeverity::Error:
                severity = "ERROR";
                break;
            }

            std::string line =
                "[" + severity + "] ";

            if (!entry.source.empty()) {
                line += entry.source + ": ";
            }

            line += entry.message;
            if(!entry.source_file.empty()){
                line+=" ["+wide_to_utf8(entry.source_file.wstring())+
                    "("+std::to_string(entry.source_line)+","+
                    std::to_string(entry.source_column)+")]";
            }

            if (entry.repeat_count > 1) {
                line += " (x" +
                    std::to_string(
                        entry.repeat_count) +
                    ")";
            }

            const auto wide =
                utf8_to_wide(line);

            SendMessageW(
                console,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    wide.c_str()));
        }

        const auto count =
            SendMessageW(
                console,
                LB_GETCOUNT,
                0,
                0);

        if (count > 0) {
            SendMessageW(
                console,
                LB_SETTOPINDEX,
                static_cast<WPARAM>(
                    count - 1),
                0);
        }
    }

    void refresh_profiler() {
        if(!profiler_list)return;
        const auto stats=profiler_samples.summary();
        SendMessageW(profiler_list,LB_RESETCONTENT,0,0);
        std::vector<std::string> rows={
            std::string("Execution: ")+(stats.playing?"Game (Play Mode)":"Editor (Edit Mode)"),
            "CPU frame average: "+std::to_string(stats.average_ms)+" ms",
            "CPU frame peak: "+std::to_string(stats.peak_ms)+" ms",
            "Simulation CPU average: "+std::to_string(stats.average_simulation_ms)+" ms",
            "Scene entities: "+std::to_string(stats.entity_count),
            "Samples: "+std::to_string(stats.samples)+" (ring cap 240)",
            "GPU time/VRAM not instrumented; standalone Player hardware unverified"
        };
        for(const auto& row:rows){
            const auto label=utf8_to_wide(row);
            SendMessageW(profiler_list,LB_ADDSTRING,0,
                         reinterpret_cast<LPARAM>(label.c_str()));
        }
    }

    void refresh_action_editor() {
        if(!action_list)return;
        action_names=action_document.actions();
        SendMessageW(action_list,LB_RESETCONTENT,0,0);
        for(const auto& name:action_names){
            auto display=name;
            if(const auto* keys=action_document.map().bindings(name))
                display+=" ("+std::to_string(keys->size())+" bindings)";
            const auto wide=utf8_to_wide(display);
            SendMessageW(action_list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(wide.c_str()));
        }
    }

    void refresh_assets_panel() {
        if (!assets_list) return;

        asset_rows =
            editor.project().assets().records();

        SendMessageW(
            assets_list,
            LB_RESETCONTENT,
            0,
            0);

        for (const auto& asset :
             asset_rows) {

            const auto relative =
                wide_to_utf8(
                    asset.relative_path.wstring());

            std::string line =
                "[" + asset.importer_id + "] " +
                relative;

            const auto wide =
                utf8_to_wide(line);

            SendMessageW(
                assets_list,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    wide.c_str()));
        }
    }

    bool choose_scene_path(bool save, std::filesystem::path& path) {
        wchar_t buffer[32768]{};

        if (!path.empty()) {
            const auto existing = path.wstring();
            wcsncpy_s(
                buffer,
                std::size(buffer),
                existing.c_str(),
                _TRUNCATE);
        }

        constexpr wchar_t filter[] =
            L"NEngine Scene (*.nscene)\0*.nscene\0"
            L"All Files (*.*)\0*.*\0\0";

        OPENFILENAMEW dialog{};
        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = host;
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = buffer;
        dialog.nMaxFile = static_cast<DWORD>(std::size(buffer));
        dialog.lpstrDefExt = L"nscene";
        dialog.Flags =
            OFN_EXPLORER |
            OFN_PATHMUSTEXIST |
            (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);

        const BOOL ok =
            save
                ? GetSaveFileNameW(&dialog)
                : GetOpenFileNameW(&dialog);

        if (!ok) return false;

        path = std::filesystem::path{buffer};
        return true;
    }

    bool load_scene_path(
        const std::filesystem::path& path) {

        if (!editor.can_edit()) return false;

        nengine::core::SceneData data;
        std::string error;

        if (!nengine::core::SceneSerializer::load_file(
                path,
                data,
                &error)) {
            log_line(L"Scene load failed.");
            return false;
        }

        if (!nengine::core::SceneSerializer::instantiate(
                data,
                editor.world(),
                &error,
                &editor.component_serialization())) {
            log_line(L"Scene instantiate failed.");
            return false;
        }

        current_scene_path = path;
        editor.selection().clear();
        editor.commands().clear();
        editor.mark_scene_saved();

        editor.console().info(
            "Scene",
            "Loaded " +
                wide_to_utf8(
                    path.wstring()));

        refresh_console();
        return true;
    }

    bool open_scene_file() {
        if (!editor.can_edit()) return false;

        std::filesystem::path path =
            current_scene_path;

        if (!choose_scene_path(
                false,
                path)) {
            return false;
        }

        return load_scene_path(path);
    }

    bool save_scene_file(bool save_as = false) {
        if (!editor.can_edit()) return false;

        std::filesystem::path path = current_scene_path;
        if (save_as || path.empty()) {
            if (!choose_scene_path(true, path)) {
                return false;
            }
        }

        auto scene_name = wide_to_utf8(path.stem().wstring());
        if (scene_name.empty()) {
            scene_name = "Untitled";
        }

        nengine::core::SceneData data;
        std::string error;
        if (!nengine::core::SceneSerializer::capture_checked(
                editor.world(), data, scene_name, &error,
                &editor.component_serialization())) {
            editor.console().warning("Scene", "Save blocked: " + error);
            refresh_console();
            return false;
        }
        if (!nengine::core::SceneSerializer::save_file(
                data,
                path,
                &error)) {
            log_line(L"Scene save failed.");
            return false;
        }

        current_scene_path = std::move(path);
        editor.mark_scene_saved();

        editor.console().info(
            "Scene",
            "Saved " +
                wide_to_utf8(
                    current_scene_path.wstring()));

        editor.project().refresh_assets();
        refresh_assets_panel();
        refresh_console();
        return true;
    }

    void refresh_window_title() {
        if (!parent) return;

        std::string title =
            "NEngine Editor";

        if (editor.project().is_open()) {
            title += " - " +
                editor.project().manifest().name;
        }

        if (!current_scene_path.empty()) {
            title += " - " +
                wide_to_utf8(
                    current_scene_path
                        .filename()
                        .wstring());
        }

        if (editor.scene_dirty()) {
            title += " *";
        }

        const auto wide =
            utf8_to_wide(title);

        SetWindowTextW(
            parent,
            wide.c_str());
    }

    void refresh() {
        if (!controls_ready || refreshing) return;

        refreshing = true;
        shell_log("refresh begin");

        shell_log("refresh toolbar");
        refresh_toolbar();

        shell_log("refresh hierarchy");
        refresh_hierarchy();

        shell_log("refresh inspector");
        refresh_inspector();
        refresh_canvas_controls();

        refresh_assets_panel();
        refresh_console();
        refresh_window_title();

        shell_log("refresh scene invalidate");
        if(scene)InvalidateRect(scene,nullptr,TRUE);
        if(game_view&&game_view_visible)InvalidateRect(game_view,nullptr,TRUE);

        shell_log("refresh complete");
        refreshing = false;
    }

    void refresh_canvas_controls() {
        if (!canvas_edit_toggle) return;
        const bool active = canvas_mode && editor.has_ui_canvas_draft();
        EnableWindow(canvas_edit_toggle, editor.can_edit() &&
                     editor.ui_canvas_asset().valid());
        const auto selected_count = editor.selected_ui_widgets().size();
        const std::wstring label = active && selected_count > 1
            ? L"Canvas Edit (" + std::to_wstring(selected_count) + L" selected)"
            : (active ? L"Close Canvas Edit" : L"Canvas Edit");
        SetWindowTextW(canvas_edit_toggle, label.c_str());
        if (!active) return;
        canvas_widget_rows.clear();
        SendMessageW(canvas_widget_list, CB_RESETCONTENT, 0, 0);
        int selected_index = -1;
        for (const auto& entry : editor.ui_edit_widgets()) {
            std::size_t depth = 0;
            auto parent = entry.parent;
            while (parent != nengine::ui::invalid_widget && depth < 12) {
                ++depth;
                const auto& nodes = editor.ui_edit_widgets();
                const auto found = std::find_if(
                    nodes.begin(), nodes.end(),
                    [parent](const nengine::ui::Widget& node) {
                        return node.id == parent;
                    });
                if (found == nodes.end()) break;
                parent = found->parent;
            }
            std::string label(depth * 2, ' ');
            label += "#" + std::to_string(entry.id) + " ";
            label += entry.name.empty() ? entry.text : entry.name;
            if (label.size() <= depth * 2 + 4) label += "(unnamed)";
            const auto wide = utf8_to_wide(label);
            SendMessageW(canvas_widget_list, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(wide.c_str()));
            if (entry.id == editor.selected_ui_widget())
                selected_index = static_cast<int>(canvas_widget_rows.size());
            canvas_widget_rows.push_back(entry.id);
        }
        SendMessageW(canvas_widget_list, CB_SETCURSEL, selected_index, 0);
        const auto* widget = editor.ui_edit_widget();
        canvas_parent_rows.clear();
        SendMessageW(canvas_parent_combo, CB_RESETCONTENT, 0, 0);
        SendMessageW(canvas_parent_combo, CB_ADDSTRING, 0,
                     reinterpret_cast<LPARAM>(L"(Canvas root)"));
        canvas_parent_rows.push_back(nengine::ui::invalid_widget);
        int parent_index = 0;
        for (const auto& node : editor.ui_edit_widgets()) {
            if (widget && node.id == widget->id) continue;
            const auto label = utf8_to_wide(
                "#" + std::to_string(node.id) + " " +
                (node.name.empty() ? node.text : node.name));
            SendMessageW(canvas_parent_combo, CB_ADDSTRING, 0,
                         reinterpret_cast<LPARAM>(label.c_str()));
            if (widget && node.id == widget->parent)
                parent_index = static_cast<int>(canvas_parent_rows.size());
            canvas_parent_rows.push_back(node.id);
        }
        SendMessageW(canvas_parent_combo, CB_SETCURSEL, parent_index, 0);
        set_text(canvas_name, widget ? widget->name : "");
        set_text(canvas_text, widget ? widget->text : "");
        const bool is_image = widget && widget->kind == nengine::ui::WidgetKind::Image;
        set_text(canvas_image_guid, is_image && widget->image_texture.valid()
            ? widget->image_texture.to_string() : "");
        EnableWindow(canvas_image_guid, is_image);
        EnableWindow(canvas_assign_image, is_image);
        EnableWindow(canvas_clear_image, is_image && widget->image_texture.valid());
        EnableWindow(canvas_name, widget != nullptr);
        EnableWindow(canvas_text, widget != nullptr);
        EnableWindow(canvas_apply, widget != nullptr);
        EnableWindow(canvas_delete, widget != nullptr);
        EnableWindow(canvas_save, editor.ui_edit_dirty());
        EnableWindow(canvas_undo, editor.ui_edit_undo_count() != 0);
        EnableWindow(canvas_redo, editor.ui_edit_redo_count() != 0);
        EnableWindow(canvas_add_button, true);
        EnableWindow(canvas_add_text, true);
        EnableWindow(canvas_add_image, true);
        EnableWindow(canvas_duplicate, widget != nullptr);
        EnableWindow(canvas_to_back, widget != nullptr);
        EnableWindow(canvas_to_front, widget != nullptr);
        const auto values = widget
            ? std::array<float, 8>{
                widget->anchors.anchor_min.x, widget->anchors.anchor_min.y,
                widget->anchors.anchor_max.x, widget->anchors.anchor_max.y,
                widget->anchors.offset_min.x, widget->anchors.offset_min.y,
                widget->anchors.offset_max.x, widget->anchors.offset_max.y}
            : std::array<float, 8>{};
        for (std::size_t i = 0; i < canvas_anchor_inputs.size(); ++i) {
            set_float(canvas_anchor_inputs[i], values[i]);
            EnableWindow(canvas_anchor_inputs[i], widget != nullptr);
        }
        const auto colors = widget
            ? std::array<float, 4>{widget->color.r, widget->color.g,
                                    widget->color.b, widget->color.a}
            : std::array<float, 4>{};
        for (std::size_t i = 0; i < canvas_color_inputs.size(); ++i) {
            set_float(canvas_color_inputs[i], colors[i]);
            EnableWindow(canvas_color_inputs[i], widget != nullptr);
        }
        EnableWindow(canvas_apply_geometry, widget != nullptr);
        EnableWindow(canvas_preset, widget != nullptr);
        EnableWindow(canvas_apply_preset, widget != nullptr);
        EnableWindow(canvas_parent_combo, widget != nullptr);
        EnableWindow(canvas_apply_parent, widget != nullptr);
    }

    void refresh_toolbar() {
        const auto state =
            nengine::editor::build_toolbar(editor);

        EnableWindow(open_scene, editor.can_edit());
        EnableWindow(save_scene, editor.can_edit());
        EnableWindow(new_entity, editor.can_edit());
        EnableWindow(
            delete_entity,
            editor.can_edit() &&
            editor.presentation_world().is_alive(
                editor.selection().active()));
        EnableWindow(refresh_assets, editor.project().is_open());
        EnableWindow(scripts, editor.project().is_open());
        EnableWindow(
            build_scripts,
            editor.project().is_open() &&
                editor.can_edit());
        EnableWindow(undo, state.can_undo);
        EnableWindow(redo, state.can_redo);
        EnableWindow(play, state.can_play);
        EnableWindow(stop, state.can_stop);
        EnableWindow(
            pause,
            state.can_pause || state.can_resume);
        EnableWindow(step, state.can_step);

        SetWindowTextW(
            undo,
            utf8_to_wide(state.undo_label).c_str());

        SetWindowTextW(
            redo,
            utf8_to_wide(state.redo_label).c_str());

        SetWindowTextW(
            pause,
            state.can_resume ? L"Resume" : L"Pause");
    }

    void refresh_hierarchy() {
        hierarchy_rows =
            nengine::editor::build_hierarchy(editor);

        SendMessageW(
            hierarchy,
            LB_RESETCONTENT,
            0,
            0);

        int selected_index = -1;

        for (std::size_t i = 0;
             i < hierarchy_rows.size();
             ++i) {

            const auto& row = hierarchy_rows[i];

            std::string text(row.depth * 3u, ' ');
            if (!row.active) {
                text += "[off] ";
            }
            text += row.name;

            const auto wide = utf8_to_wide(text);
            SendMessageW(
                hierarchy,
                LB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(wide.c_str()));

            if (row.selected) {
                selected_index = static_cast<int>(i);
                SendMessageW(hierarchy,LB_SETSEL,TRUE,static_cast<LPARAM>(i));
            }
        }

        if (selected_index >= 0) {
            SendMessageW(
                hierarchy,
                LB_SETCARETINDEX,
                selected_index,
                0);
        }
    }

    void refresh_generic_property_editor(
        const nengine::editor::InspectorSnapshot& snapshot) {

        // Full Inspector refreshes also happen after Undo/Redo and edits.
        // Preserve the selected property by identity, not by the old row
        // index, which can move when component fields are rebuilt.
        std::optional<GenericPropertyBinding> previous_selection;

        if (generic_properties &&
            generic_property_entity == snapshot.entity) {

            const int old_index = static_cast<int>(
                SendMessageW(
                    generic_properties,
                    LB_GETCURSEL,
                    0,
                    0));

            if (old_index >= 0 &&
                static_cast<std::size_t>(old_index) <
                    generic_property_rows.size()) {

                previous_selection = generic_property_rows[
                    static_cast<std::size_t>(old_index)];
            }
        }

        generic_property_entity = snapshot.entity;
        generic_property_rows.clear();

        if (!generic_properties) {
            return;
        }

        SendMessageW(
            generic_properties,
            LB_RESETCONTENT,
            0,
            0);

        for (const auto& component :
             snapshot.components) {

            if (component.type ==
                nengine::core::World::transform_type) {
                continue;
            }

            for (const auto& field :
                 component.fields) {

                GenericPropertyBinding binding;
                binding.component =
                    component.type;
                binding.property =
                    field.property_path;
                binding.kind =
                    field.kind;
                binding.value =
                    field.value;
                binding.editable =
                    field.editable;

                generic_property_rows.push_back(
                    binding);

                std::string line =
                    component.name +
                    "." +
                    field.label +
                    " = " +
                    nengine::editor::
                        format_property_value(
                            field.value);

                if (!field.editable) {
                    line += " [read-only]";
                }

                const auto wide =
                    utf8_to_wide(line);

                SendMessageW(
                    generic_properties,
                    LB_ADDSTRING,
                    0,
                    reinterpret_cast<LPARAM>(
                        wide.c_str()));
            }
        }

        if (generic_property_rows.empty()) {
            set_text(
                generic_value,
                "");

            EnableWindow(
                generic_value,
                FALSE);

            EnableWindow(
                apply_property,
                FALSE);

            EnableWindow(
                assign_asset,
                FALSE);

            return;
        }

        std::size_t selected_index = 0;

        if (previous_selection) {
            const auto retained =
                nengine::editor::find_inspector_property_row(
                    snapshot,
                    previous_selection->component,
                    previous_selection->property);

            if (retained &&
                *retained < generic_property_rows.size()) {
                selected_index = *retained;
            }
        }

        SendMessageW(
            generic_properties,
            LB_SETCURSEL,
            static_cast<WPARAM>(selected_index),
            0);

        refresh_generic_property_value();
    }

    void refresh_generic_property_value() {
        if (!generic_properties ||
            generic_property_rows.empty()) {
            return;
        }

        const int index =
            static_cast<int>(
                SendMessageW(
                    generic_properties,
                    LB_GETCURSEL,
                    0,
                    0));

        if (index < 0 ||
            static_cast<std::size_t>(index) >=
                generic_property_rows.size()) {
            return;
        }

        const auto& binding =
            generic_property_rows[
                static_cast<std::size_t>(
                    index)];

        set_text(
            generic_value,
            nengine::editor::
                format_property_value(
                    binding.value));

        const bool editable =
            editor.can_edit() &&
            binding.editable;

        EnableWindow(
            generic_value,
            editable);

        EnableWindow(
            apply_property,
            editable);

        const int asset_index =
            assets_list
                ? static_cast<int>(
                    SendMessageW(
                        assets_list,
                        LB_GETCURSEL,
                        0,
                        0))
                : -1;

        const bool can_assign_asset =
            editable &&
            binding.kind ==
                nengine::core::
                    PropertyKind::
                        AssetReference &&
            asset_index >= 0 &&
            static_cast<std::size_t>(
                asset_index) <
                asset_rows.size();

        EnableWindow(
            assign_asset,
            can_assign_asset);
    }

    bool apply_generic_property_edit() {
        if (!editor.can_edit()) {
            return false;
        }

        const int index =
            static_cast<int>(
                SendMessageW(
                    generic_properties,
                    LB_GETCURSEL,
                    0,
                    0));

        if (index < 0 ||
            static_cast<std::size_t>(index) >=
                generic_property_rows.size()) {
            return false;
        }

        const auto binding =
            generic_property_rows[
                static_cast<std::size_t>(
                    index)];

        if (!binding.editable) {
            return false;
        }

        nengine::core::PropertyValue parsed;
        std::string error;

        if (!nengine::editor::
                parse_property_value(
                    binding.kind,
                    read_text(
                        generic_value),
                    parsed,
                    &error)) {

            editor.console().warning(
                "Inspector",
                "Property parse failed: " +
                    error);

            refresh_console();
            return false;
        }

        const auto entity =
            editor.selection().active();

        if (!editor.world().is_alive(
                entity)) {
            return false;
        }

        const bool applied =
            editor.commands().execute(
                editor.world(),
                std::make_unique<
                    nengine::editor::
                        SetPropertyCommand>(
                            &editor.property_access(),
                            entity,
                            binding.component,
                            binding.property,
                            parsed));

        if (!applied) {
            editor.console().warning(
                "Inspector",
                "Property edit could not be applied.");

            refresh_console();
            return false;
        }

        editor.console().info(
            "Inspector",
            "Updated " +
                binding.property);

        return true;
    }

    bool assign_selected_asset_to_property() {
        if (!editor.can_edit() ||
            !generic_properties ||
            !assets_list) {
            return false;
        }

        const int property_index =
            static_cast<int>(
                SendMessageW(
                    generic_properties,
                    LB_GETCURSEL,
                    0,
                    0));

        const int asset_index =
            static_cast<int>(
                SendMessageW(
                    assets_list,
                    LB_GETCURSEL,
                    0,
                    0));

        if (property_index < 0 ||
            asset_index < 0 ||
            static_cast<std::size_t>(
                property_index) >=
                generic_property_rows
                    .size() ||
            static_cast<std::size_t>(
                asset_index) >=
                asset_rows.size()) {
            return false;
        }

        const auto binding =
            generic_property_rows[
                static_cast<std::size_t>(
                    property_index)];

        if (!binding.editable ||
            binding.kind !=
                nengine::core::
                    PropertyKind::
                        AssetReference) {
            return false;
        }

        const auto entity =
            editor.selection()
                .active();

        if (!editor.world()
                .is_alive(
                    entity)) {
            return false;
        }

        const auto& asset =
            asset_rows[
                static_cast<std::size_t>(
                    asset_index)];

        const bool assigned =
            editor.commands().execute(
                editor.world(),
                std::make_unique<
                    nengine::editor::
                        SetPropertyCommand>(
                            &editor
                                .property_access(),
                            entity,
                            binding.component,
                            binding.property,
                            nengine::core::
                                PropertyValue{
                                    asset.guid
                                        .to_string()}));

        if (assigned) {
            editor.console().info(
                "Inspector",
                "Assigned " +
                    asset.relative_path
                        .generic_string() +
                    " to " +
                    binding.property);
        } else {
            editor.console().warning(
                "Inspector",
                "Selected asset could not be assigned.");
        }

        return assigned;
    }

    void refresh_add_component_options(
        const nengine::editor::InspectorSnapshot& snapshot) {

        add_component_rows.clear();

        if (!add_component_list ||
            !add_component) {
            return;
        }

        SendMessageW(
            add_component_list,
            CB_RESETCONTENT,
            0,
            0);

        const bool editable =
            snapshot.valid &&
            editor.can_edit() &&
            editor.world().is_alive(
                snapshot.entity);

        if (!editable) {
            EnableWindow(
                add_component_list,
                FALSE);

            EnableWindow(
                add_component,
                FALSE);

            return;
        }

        for (const auto type :
             editor.available_component_types()) {
            const auto* descriptor=editor.component_registry().find(type);
            if(!descriptor)continue;
            add_component_rows.push_back(type);
            auto display=descriptor->name;

            constexpr std::string_view
                prefix{"NEngine."};

            if (display.rfind(
                    prefix,
                    0) == 0u) {
                display.erase(
                    0,
                    prefix.size());
            }

            const auto wide =
                utf8_to_wide(
                    display);

            SendMessageW(
                add_component_list,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    wide.c_str()));
        }

        const bool has_options =
            !add_component_rows.empty();

        if (has_options) {
            SendMessageW(
                add_component_list,
                CB_SETCURSEL,
                0,
                0);
        }

        EnableWindow(
            add_component_list,
            has_options);

        EnableWindow(
            add_component,
            has_options);
    }

    void refresh_inspector() {
        const auto snapshot =
            nengine::editor::build_inspector(editor);

        const bool enabled =
            snapshot.valid && editor.can_edit();

        EnableWindow(name, enabled);
        EnableWindow(active, enabled);
        EnableWindow(apply_transform, enabled);

        EnableWindow(
            generic_properties,
            snapshot.valid);

        for (auto handle : position) {
            EnableWindow(handle, enabled);
        }
        for (auto handle : rotation) {
            EnableWindow(handle, enabled);
        }
        for (auto handle : scale) {
            EnableWindow(handle, enabled);
        }

        if (!snapshot.valid) {
            set_text(name, "");

            SendMessageW(
                active,
                BM_SETCHECK,
                BST_UNCHECKED,
                0);

            SendMessageW(
                generic_properties,
                LB_RESETCONTENT,
                0,
                0);

            generic_property_rows.clear();
            generic_property_entity =
                nengine::core::Entity::invalid();

            set_text(
                generic_value,
                "");

            EnableWindow(
                generic_value,
                FALSE);

            EnableWindow(
                apply_property,
                FALSE);

            EnableWindow(
                assign_asset,
                FALSE);

            refresh_add_component_options(
                snapshot);

            return;
        }

        set_text(name, snapshot.name);

        SendMessageW(
            active,
            BM_SETCHECK,
            snapshot.active
                ? BST_CHECKED
                : BST_UNCHECKED,
            0);

        const auto& world =
            editor.presentation_world();

        const auto* transform =
            world.transform(snapshot.entity);

        if (!transform) return;

        set_float(
            position[0],
            transform->local_position.x);
        set_float(
            position[1],
            transform->local_position.y);
        set_float(
            position[2],
            transform->local_position.z);

        set_float(
            rotation[0],
            transform->local_rotation.x);
        set_float(
            rotation[1],
            transform->local_rotation.y);
        set_float(
            rotation[2],
            transform->local_rotation.z);
        set_float(
            rotation[3],
            transform->local_rotation.w);

        set_float(
            scale[0],
            transform->local_scale.x);
        set_float(
            scale[1],
            transform->local_scale.y);
        set_float(
            scale[2],
            transform->local_scale.z);

        refresh_generic_property_editor(
            snapshot);

        refresh_add_component_options(
            snapshot);
    }

    bool apply_transform_edit() {
        if (!editor.can_edit()) {
            return false;
        }

        const auto entity =
            editor.selection().active();

        const auto* current =
            editor.world().transform(entity);

        if (!current) {
            return false;
        }

        auto value = *current;

        if (!read_float(
                position[0],
                value.local_position.x) ||
            !read_float(
                position[1],
                value.local_position.y) ||
            !read_float(
                position[2],
                value.local_position.z) ||
            !read_float(
                rotation[0],
                value.local_rotation.x) ||
            !read_float(
                rotation[1],
                value.local_rotation.y) ||
            !read_float(
                rotation[2],
                value.local_rotation.z) ||
            !read_float(
                rotation[3],
                value.local_rotation.w) ||
            !read_float(
                scale[0],
                value.local_scale.x) ||
            !read_float(
                scale[1],
                value.local_scale.y) ||
            !read_float(
                scale[2],
                value.local_scale.z)) {

            log_line(
                L"Transform edit rejected: invalid number.");
            return false;
        }

        return editor.commands().execute(
            editor.world(),
            std::make_unique<
                nengine::editor::SetTransformCommand>(
                    entity,
                    value));
    }

    bool open_external_path(
        const std::filesystem::path& path,
        std::string_view source_label) {

        const auto native =
            path.wstring();

        const HINSTANCE launched =
            ShellExecuteW(
                host,
                L"open",
                native.c_str(),
                nullptr,
                editor.project().is_open()
                    ? editor.project().root()
                        .wstring().c_str()
                    : nullptr,
                SW_SHOWNORMAL);

        const auto result =
            reinterpret_cast<INT_PTR>(
                launched);

        if (result <= 32) {
            editor.console().warning(
                std::string{source_label},
                "Windows could not open: " +
                    wide_to_utf8(
                        path.wstring()));
            refresh_console();
            return false;
        }

        editor.console().info(
            std::string{source_label},
            "Opened " +
                wide_to_utf8(
                    path.wstring()));

        refresh_console();
        return true;
    }

    bool build_managed_scripts() {
        if (!editor.project().is_open()) {
            editor.console().warning(
                "Scripting",
                "No project is open.");
            refresh_console();
            return false;
        }

        const auto package_manifest =
            editor.project().root() /
            "Packages" /
            "managed-packages.txt";

        std::string package_error;

        const auto packages =
            nengine::scripting::
                ManagedProjectGenerator::
                    load_package_manifest(
                        package_manifest,
                        &package_error);

        if (!package_error.empty()) {
            editor.console().warning(
                "Scripting",
                "Package manifest warning: " +
                    package_error);
        }

        nengine::scripting::
            ManagedProjectConfig project_config;

        project_config.project_name =
            "GameScripts";
        project_config.project_root =
            editor.project().root();
        project_config.packages =
            packages;

        nengine::scripting::
            ManagedProjectOutput generated;

        std::string generate_error;

        if (!nengine::scripting::
                ManagedProjectGenerator::
                    generate(
                        project_config,
                        generated,
                        &generate_error)) {

            editor.console().error(
                "Scripting",
                "Managed project generation failed: " +
                    generate_error);
            refresh_console();
            return false;
        }

        nengine::scripting::
            ManagedBuildConfig build_config;

        build_config.project_path =
            generated.project_path;
        ++managed_build_generation;

        build_config.output_directory =
            editor.project().root() /
            "Library" /
            "ManagedBuild" /
            "Debug" /
            ("Generation-" +
             std::to_string(
                 managed_build_generation));
        build_config.configuration =
            "Debug";

        editor.console().info(
            "Scripting",
            "Building C# gameplay assembly...");
        refresh_console();

        const auto build =
            nengine::scripting::
                run_managed_build(
                    build_config);

        editor.console().report_dotnet_build(
            build.output,editor.project().root());
        if (!build.success) {
            editor.console().error(
                "Scripting",
                build.message);
            refresh_console();
            return false;
        }

        editor.console().info(
            "Scripting",
            "Built " +
                wide_to_utf8(
                    build.plan
                        .assembly_path
                        .filename()
                        .wstring()));

        std::string runtime_error;

        if (editor.managed_runtime_ready()) {
            if (!editor.reload_managed_runtime(
                    build.plan.assembly_path,
                    "GameScripts",
                    &runtime_error)) {

                editor.console().error(
                    "Scripting",
                    "C# build succeeded, but collectible gameplay hot reload failed: " +
                        runtime_error);
                refresh_console();
                return false;
            }

            editor.console().info(
                "Scripting",
                "Managed gameplay DLL hot-reloaded through the stable NEngine.Bridge assembly; hostfxr stayed alive.");
        } else {
            std::string host_error;

            const auto host_info =
                nengine::scripting::
                    discover_dotnet_host(
                        {},
                        &host_error);

            if (!host_info) {
                editor.console().error(
                    "Scripting",
                    "C# build succeeded, but .NET runtime host discovery failed: " +
                        host_error);
                refresh_console();
                return false;
            }

            if (!editor.initialize_managed_runtime(
                    host_info->hostfxr_path,
                    generated.runtime_config_path,
                    build.plan.assembly_path,
                    "GameScripts",
                    &runtime_error)) {

                editor.console().error(
                    "Scripting",
                    "C# build succeeded, but managed runtime initialization failed: " +
                        runtime_error);
                refresh_console();
                return false;
            }

            editor.console().info(
                "Scripting",
                "Managed gameplay DLL loaded through hostfxr + NEngine.Bridge ABI v18; collectible hot reload is ready.");
        }

        refresh_console();
        return true;
    }

    bool generate_and_open_scripts() {
        if (!editor.project().is_open()) {
            editor.console().warning(
                "Scripting",
                "No project is open.");
            refresh_console();
            return false;
        }

        const auto package_manifest =
            editor.project().root() /
            "Packages" /
            "managed-packages.txt";

        std::string package_error;

        const auto packages =
            nengine::scripting::
                ManagedProjectGenerator::
                    load_package_manifest(
                        package_manifest,
                        &package_error);

        if (!package_error.empty()) {
            editor.console().warning(
                "Scripting",
                "Package manifest warning: " +
                    package_error);
        }

        nengine::scripting::
            ManagedProjectConfig config;

        config.project_name =
            "GameScripts";

        config.project_root =
            editor.project().root();

        config.packages =
            packages;

        nengine::scripting::
            ManagedProjectOutput output;

        std::string error;

        if (!nengine::scripting::
                ManagedProjectGenerator::
                    generate(
                        config,
                        output,
                        &error)) {

            editor.console().error(
                "Scripting",
                "Managed project generation failed: " +
                    error);

            refresh_console();
            return false;
        }

        editor.console().info(
            "Scripting",
            "Generated " +
                wide_to_utf8(
                    output.solution_path
                        .filename()
                        .wstring()) +
                " with " +
                std::to_string(
                    packages.size()) +
                " NuGet package reference(s).");

        // Ctrl+Scripts launches Visual Studio, Shift+Scripts Rider;
        // a plain click keeps the user's .sln file association.
        const auto ide=(GetKeyState(VK_CONTROL)&0x8000)!=0
            ? nengine::editor::IdeChoice::VisualStudio
            : (GetKeyState(VK_SHIFT)&0x8000)!=0
              ? nengine::editor::IdeChoice::Rider
              : nengine::editor::IdeChoice::Associated;
        const auto launch=nengine::editor::prepare_ide_launch(
            output.solution_path,ide,{},&error);
        if(!launch){
            editor.console().error("Scripting","IDE launch rejected: "+error);
            refresh_console();return false;
        }
        const auto executable=launch->executable.wstring();
        const auto working=launch->working_directory.wstring();
        const HINSTANCE launched=ShellExecuteW(
            host,L"open",executable.c_str(),
            launch->use_file_association?nullptr:launch->parameters.c_str(),
            working.c_str(),SW_SHOWNORMAL);

        const auto result =
            reinterpret_cast<INT_PTR>(
                launched);

        if (result <= 32) {
            editor.console().warning(
                "Scripting",
                "Solution generated, but Windows could not open it. Install/associate Visual Studio, Rider, or another .sln editor.");
            refresh_console();
            return false;
        }

        editor.console().info(
            "Scripting",
            "Opened managed solution.");

        refresh_console();
        return true;
    }

    bool on_command(int id, int notification) {
        bool handled = false;

        switch (id) {
        case IdCanvasEdit: {
            if (notification != BN_CLICKED) return false;
            if (canvas_mode && editor.has_ui_canvas_draft()) {
                if (editor.ui_edit_dirty())
                    editor.console().warning(
                        "UI", "Save Canvas edits before closing Canvas Edit Mode.");
                else canvas_mode = false;
            } else {
                std::string error;
                if (editor.begin_ui_canvas_edit(editor.ui_canvas_asset(), &error)) {
                    canvas_mode = true;
                    if (vulkan_preview_enabled) {
                        vulkan_preview_enabled = false;
                        SendMessageW(vulkan_preview, BM_SETCHECK, BST_UNCHECKED, 0);
                    }
                }
                else editor.console().warning("UI", error);
            }
            RECT bounds{};
            GetClientRect(host, &bounds);
            layout(bounds.right - bounds.left, bounds.bottom - bounds.top);
            handled = true;
            break;
        }
        case IdCanvasApplyParent: {
            if (notification != BN_CLICKED) return false;
            if (canvas_mode && editor.ui_edit_widget() && scene) {
                const auto selected = static_cast<int>(
                    SendMessageW(canvas_parent_combo, CB_GETCURSEL, 0, 0));
                if (selected >= 0 &&
                    static_cast<std::size_t>(selected) < canvas_parent_rows.size()) {
                    RECT bounds{};
                    GetClientRect(scene, &bounds);
                    if (!editor.ui_edit_reparent_preserve_geometry(
                        canvas_parent_rows[static_cast<std::size_t>(selected)],
                        static_cast<float>(bounds.right),
                        static_cast<float>(bounds.bottom)))
                        editor.console().warning(
                            "UI", "Invalid parent, hierarchy cycle, or unchanged parent.");
                }
            }
            handled = true;
            break;
        }
        case IdCanvasApplyPreset: {
            if (notification != BN_CLICKED) return false;
            if (canvas_mode && editor.ui_edit_widget() && scene) {
                const int index = static_cast<int>(
                    SendMessageW(canvas_preset, CB_GETCURSEL, 0, 0));
                if (index >= 0 && index < 12) {
                    RECT bounds{};
                    GetClientRect(scene, &bounds);
                    if (!editor.ui_edit_apply_anchor_preset(
                            static_cast<nengine::ui::AnchorPreset>(index),
                            static_cast<float>(bounds.right),
                            static_cast<float>(bounds.bottom)))
                        editor.console().warning(
                            "UI", "Cannot apply preset to an invisible/invalid widget.");
                }
            }
            handled = true;
            break;
        }
        case IdCanvasWidgetList:
            if (notification != CBN_SELCHANGE) return false;
            if (canvas_mode && editor.has_ui_canvas_draft()) {
                const auto index = static_cast<int>(
                    SendMessageW(canvas_widget_list, CB_GETCURSEL, 0, 0));
                if (index >= 0 &&
                    static_cast<std::size_t>(index) < canvas_widget_rows.size())
                    editor.select_ui_widget(
                        canvas_widget_rows[static_cast<std::size_t>(index)]);
            }
            handled = true;
            break;
        case IdCanvasDuplicate:
            if (notification != BN_CLICKED) return false;
            if (canvas_mode) editor.ui_edit_duplicate_selected();
            handled = true;
            break;
        case IdCanvasBack:
            if (notification != BN_CLICKED) return false;
            if (canvas_mode) editor.ui_edit_set_sibling_index(0);
            handled = true;
            break;
        case IdCanvasFront:
            if (notification != BN_CLICKED) return false;
            if (canvas_mode) editor.ui_edit_set_sibling_index(99999u);
            handled = true;
            break;
        case IdCanvasApplyGeometry: {
            if (notification != BN_CLICKED) return false;
            const auto* selected = editor.ui_edit_widget();
            if (canvas_mode && selected) {
                std::array<float, 8> values{};
                std::array<float, 4> rgba{};
                bool valid = true;
                for (std::size_t i = 0; i < values.size(); ++i)
                    valid = read_float(canvas_anchor_inputs[i], values[i]) && valid;
                for (std::size_t i = 0; i < rgba.size(); ++i)
                    valid = read_float(canvas_color_inputs[i], rgba[i]) && valid;
                if (valid) {
                    auto anchors = selected->anchors;
                    anchors.anchor_min = {values[0], values[1]};
                    anchors.anchor_max = {values[2], values[3]};
                    anchors.offset_min = {values[4], values[5]};
                    anchors.offset_max = {values[6], values[7]};
                    const bool anchors_changed =
                        editor.ui_edit_set_anchors(anchors);
                    const bool color_changed =
                        editor.ui_edit_set_color(
                            {rgba[0], rgba[1], rgba[2], rgba[3]});
                    if (!anchors_changed && !color_changed)
                        editor.console().warning(
                            "UI", "Canvas geometry/color unchanged or invalid");
                } else editor.console().warning(
                    "UI", "Canvas geometry must contain finite numbers");
            }
            handled = true;
            break;
        }
        case IdCanvasApply: {
            if (notification != BN_CLICKED) return false;
            if (canvas_mode && editor.has_ui_canvas_draft() &&
                editor.ui_edit_widget()) {
                const auto next_name = read_text(canvas_name);
                const auto next_text = read_text(canvas_text);
                bool changed = false;
                if (editor.ui_edit_widget()->name != next_name)
                    changed = editor.ui_edit_set_name(next_name) || changed;
                if (editor.ui_edit_widget()->text != next_text)
                    changed = editor.ui_edit_set_text(next_text) || changed;
                if (!changed)
                    editor.console().warning(
                        "UI", "Canvas properties unchanged or rejected; check widget name.");
            }
            handled = true;
            break;
        }
        case IdCanvasSave: {
            if (notification != BN_CLICKED) return false;
            std::string error;
            if (editor.save_ui_canvas_edit(&error))
                editor.console().info("UI", "Canvas saved and reimported.");
            else editor.console().warning("UI", error);
            handled = true;
            break;
        }
        case IdCanvasAssignImage: {
            if (notification != BN_CLICKED) return false;
            if (canvas_mode && editor.ui_edit_widget()) {
                const int index = static_cast<int>(
                    SendMessageW(assets_list, LB_GETCURSEL, 0, 0));
                if (index < 0 || static_cast<std::size_t>(index) >= asset_rows.size() ||
                    asset_rows[static_cast<std::size_t>(index)].importer_id !=
                        "NEngine.Texture" ||
                    !editor.ui_edit_set_image_texture(
                        asset_rows[static_cast<std::size_t>(index)].guid))
                    editor.console().warning("UI",
                        "Select an imported texture in Assets before assigning to Image");
            }
            handled = true;
            break;
        }
        case IdCanvasClearImage:
            if (notification != BN_CLICKED) return false;
            if (canvas_mode) editor.ui_edit_set_image_texture({});
            handled = true;
            break;
        case IdCanvasAddButton:
        case IdCanvasAddText:
        case IdCanvasAddImage: {
            if (notification != BN_CLICKED) return false;
            if (canvas_mode && editor.has_ui_canvas_draft()) {
                const auto kind = id == IdCanvasAddButton
                    ? nengine::ui::WidgetKind::Button
                    : (id == IdCanvasAddImage
                        ? nengine::ui::WidgetKind::Image
                        : nengine::ui::WidgetKind::Text);
                editor.ui_edit_add_widget(kind);
            }
            handled = true;
            break;
        }
        case IdCanvasDelete:
            if (notification != BN_CLICKED) return false;
            if (canvas_mode) editor.ui_edit_remove_selected();
            handled = true;
            break;
        case IdCanvasUndo:
            if (notification != BN_CLICKED) return false;
            if (canvas_mode) editor.ui_edit_undo();
            handled = true;
            break;
        case IdCanvasRedo:
            if (notification != BN_CLICKED) return false;
            if (canvas_mode) editor.ui_edit_redo();
            handled = true;
            break;
        case IdOpen:
            if (notification != BN_CLICKED) return false;
            open_scene_file();
            handled = true;
            break;

        case IdSave:
            if (notification != BN_CLICKED) return false;
            save_scene_file(false);
            handled = true;
            break;

        case IdNewEntity:
            if (notification != BN_CLICKED) return false;
            if (editor.can_edit()) {
                auto command =
                    std::make_unique<
                        nengine::editor::
                            CreateEntityCommand>(
                                "GameObject");

                auto* command_ptr =
                    command.get();

                if (editor.commands().execute(
                        editor.world(),
                        std::move(command))) {

                    editor.selection().set(
                        command_ptr->
                            created_entity());

                    editor.console().info(
                        "Editor",
                        "Created GameObject.");
                }
            }
            handled = true;
            break;

        case IdDeleteEntity:
            if (notification != BN_CLICKED) return false;
            if (editor.can_edit()) {
                const auto entity =
                    editor.selection().active();

                if (editor.world().is_alive(entity) &&
                    editor.commands().execute(
                        editor.world(),
                        std::make_unique<
                            nengine::editor::
                                DeleteEntityCommand>(
                                    entity))) {

                    editor.selection().clear();
                    editor.console().info(
                        "Editor",
                        "Deleted selected object subtree.");
                }
            }
            handled = true;
            break;

        case IdRefreshAssets:
            if (notification != BN_CLICKED) return false;
            {
                const auto scan =
                    editor.project().refresh_assets();

                const auto imports =
                    editor.project()
                        .import_supported_assets();

                editor.console().info(
                    "Assets",
                    "Asset refresh complete: " +
                        std::to_string(
                            editor.project()
                                .assets()
                                .size()) +
                        " asset(s); " +
                        std::to_string(
                            imports.imported) +
                        " imported, " +
                        std::to_string(
                            imports.cache_hits) +
                        " cache hit(s), " +
                        std::to_string(
                            imports.failed) +
                        " failed.");

                for (const auto& message :
                     scan.messages) {

                    const auto path =
                        wide_to_utf8(
                            message.path.wstring());

                    if (message.severity ==
                        nengine::assets::
                            AssetMessageSeverity::
                                Error) {
                        editor.console().error(
                            "Assets",
                            path + ": " +
                                message.message);
                    } else if (
                        message.severity ==
                        nengine::assets::
                            AssetMessageSeverity::
                                Warning) {
                        editor.console().warning(
                            "Assets",
                            path + ": " +
                                message.message);
                    } else {
                        editor.console().info(
                            "Assets",
                            path + ": " +
                                message.message);
                    }
                }
            }
            handled = true;
            break;

        case IdScripts:
            if (notification != BN_CLICKED) return false;
            generate_and_open_scripts();
            handled = true;
            break;

        case IdBuildScripts:
            if (notification != BN_CLICKED) return false;
            build_managed_scripts();
            handled = true;
            break;

        case IdUndo:
            if (notification != BN_CLICKED) return false;
            editor.commands().undo(editor.world());
            handled = true;
            break;

        case IdRedo:
            if (notification != BN_CLICKED) return false;
            editor.commands().redo(editor.world());
            handled = true;
            break;

        case IdPlay:
            if (notification != BN_CLICKED) return false;
            editor.begin_play_mode();
            handled = true;
            break;

        case IdStop:
            if (notification != BN_CLICKED) return false;
            editor.stop_play_mode();
            handled = true;
            break;

        case IdActionEditor:
            if(notification!=BN_CLICKED)return false;
            action_editor_visible=SendMessageW(action_toggle,BM_GETCHECK,0,0)==BST_CHECKED;
            if(action_editor_visible){
                profiler_visible=false;
                SendMessageW(profiler_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                package_editor_visible=false;
                SendMessageW(package_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                build_settings_visible=false;
                SendMessageW(build_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                std::string error;
                if(!editor.project().is_open()||
                   !action_document.open(editor.project().root(),&error)){
                    action_editor_visible=false;
                    SendMessageW(action_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                    editor.console().warning("Input Actions",error.empty()?
                       "Open a project first":error);
                }else refresh_action_editor();
            }
            {
                RECT rect{};GetClientRect(host,&rect);
                layout(rect.right,rect.bottom);
            }
            handled=true;break;
        case IdProfiler:
            if(notification!=BN_CLICKED)return false;
            profiler_visible=SendMessageW(profiler_toggle,BM_GETCHECK,0,0)==BST_CHECKED;
            if(profiler_visible){
                action_editor_visible=false;
                build_settings_visible=false;
                package_editor_visible=false;
                SendMessageW(package_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                SendMessageW(action_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                SendMessageW(build_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                refresh_profiler();
            }
            {
                RECT rect{};GetClientRect(host,&rect);
                layout(rect.right,rect.bottom);
            }
            handled=true;break;

        case IdBuildSettings:
            if(notification!=BN_CLICKED)return false;
            build_settings_visible=SendMessageW(build_toggle,BM_GETCHECK,0,0)==BST_CHECKED;
            if(build_settings_visible){
                package_editor_visible=false;
                SendMessageW(package_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                profiler_visible=false;
                SendMessageW(profiler_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                action_editor_visible=false;
                SendMessageW(action_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                if(!editor.project().is_open()){
                    build_settings_visible=false;
                    SendMessageW(build_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                    editor.console().warning("Build Settings","Open a project before editing targets");
                }else {
                    const auto draft=nengine::editor::read_build_settings(editor.project());
                    const auto title=utf8_to_wide(draft.project_name);
                    const auto tool=utf8_to_wide(draft.android_toolchain);
                    SetWindowTextW(build_name,title.c_str());
                    SetWindowTextW(build_toolchain,tool.c_str());
                    nengine::editor::AndroidToolPaths paths;
                    std::string load_error;
                    if(nengine::editor::load_android_tool_paths(editor.project(),paths,&load_error)){
                        SetWindowTextW(android_sdk_path,utf8_to_wide(paths.sdk.generic_string()).c_str());
                        SetWindowTextW(android_ndk_path,utf8_to_wide(paths.ndk.generic_string()).c_str());
                        SetWindowTextW(android_jdk_path,utf8_to_wide(paths.jdk.generic_string()).c_str());
                    }else editor.console().warning("Android tool paths",load_error);
                    SendMessageW(build_windows,BM_SETCHECK,draft.windows?BST_CHECKED:BST_UNCHECKED,0);
                    SendMessageW(build_android,BM_SETCHECK,draft.android?BST_CHECKED:BST_UNCHECKED,0);
                }
            }
            {
                RECT rect{};GetClientRect(host,&rect);
                layout(rect.right,rect.bottom);
            }
            handled=true;break;
        case IdBuildApply:
            if(notification!=BN_CLICKED)return false;
            {
                wchar_t project_name[200]{},toolchain[200]{};
                GetWindowTextW(build_name,project_name,200);
                GetWindowTextW(build_toolchain,toolchain,200);
                nengine::editor::BuildSettingsDraft draft{
                    wide_to_utf8(project_name),
                    SendMessageW(build_windows,BM_GETCHECK,0,0)==BST_CHECKED,
                    SendMessageW(build_android,BM_GETCHECK,0,0)==BST_CHECKED,
                    wide_to_utf8(toolchain)};
                std::string error;
                if(nengine::editor::apply_build_settings(editor.project(),draft,&error))
                    editor.console().info("Build Settings",
                        "Windows/Android target configuration saved; APK execution still requires Android backend");
                else editor.console().warning("Build Settings",error);
            }
            handled=true;break;
        case IdPackages:
            if(notification!=BN_CLICKED)return false;
            package_editor_visible=SendMessageW(package_toggle,BM_GETCHECK,0,0)==BST_CHECKED;
            if(package_editor_visible){
                build_settings_visible=false;
                action_editor_visible=false;
                profiler_visible=false;
                SendMessageW(build_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                SendMessageW(action_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                SendMessageW(profiler_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                std::string error;
                if(!nengine::editor::load_managed_packages(editor.project(),package_rows,&error)){
                    package_editor_visible=false;
                    SendMessageW(package_toggle,BM_SETCHECK,BST_UNCHECKED,0);
                    editor.console().warning("Packages",error);
                }else{
                    SendMessageW(package_list,LB_RESETCONTENT,0,0);
                    for(const auto& p:package_rows){
                        const auto display=utf8_to_wide(p.id+" @ "+p.version);
                        SendMessageW(package_list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(display.c_str()));
                    }
                }
            }
            {RECT rect{};GetClientRect(host,&rect);layout(rect.right,rect.bottom);}
            handled=true;break;
        case IdPackageList:
            if(notification!=LBN_SELCHANGE)return false;
            {
                const auto i=SendMessageW(package_list,LB_GETCURSEL,0,0);
                if(i>=0 && static_cast<std::size_t>(i)<package_rows.size()){
                    SetWindowTextW(package_id,utf8_to_wide(package_rows[static_cast<std::size_t>(i)].id).c_str());
                    SetWindowTextW(package_version,utf8_to_wide(package_rows[static_cast<std::size_t>(i)].version).c_str());
                }
            }
            handled=true;break;
        case IdPackageAdd:
        case IdPackageRemove:
            if(notification!=BN_CLICKED)return false;
            {
                wchar_t id[200]{},version[200]{};
                GetWindowTextW(package_id,id,200);
                GetWindowTextW(package_version,version,200);
                std::string error;
                const auto value=(id_control==IdPackageRemove)?std::string{}:wide_to_utf8(version);
                if(nengine::editor::edit_managed_package(editor.project(),wide_to_utf8(id),value,&error) &&
                   nengine::editor::load_managed_packages(editor.project(),package_rows,&error)){
                    SendMessageW(package_list,LB_RESETCONTENT,0,0);
                    for(const auto& p:package_rows){
                        const auto display=utf8_to_wide(p.id+" @ "+p.version);
                        SendMessageW(package_list,LB_ADDSTRING,0,reinterpret_cast<LPARAM>(display.c_str()));
                    }
                    editor.console().info("Packages","NuGet manifest saved; select Build C# to restore packages");
                }else editor.console().warning("Packages",error);
                refresh_console();
            }
            handled=true;break;
        case IdAndroidPathsApply:
            if(notification!=BN_CLICKED)return false;
            {
                wchar_t sdk[4096]{},ndk[4096]{},jdk[4096]{};
                GetWindowTextW(android_sdk_path,sdk,4096);
                GetWindowTextW(android_ndk_path,ndk,4096);
                GetWindowTextW(android_jdk_path,jdk,4096);
                nengine::editor::AndroidToolPaths paths{
                    std::filesystem::path(sdk),
                    std::filesystem::path(ndk),
                    std::filesystem::path(jdk)};
                std::string error;
                if(nengine::editor::save_android_tool_paths(editor.project(),paths,&error))
                    editor.console().info("Android tool paths",
                        "SDK android-35, NDK 27.x and JDK bin/java validated and saved (not APK proof)");
                else editor.console().warning("Android tool paths",error);
                refresh_console();
            }
            handled=true;break;
        case IdActionList:
            if(notification!=LBN_SELCHANGE)return false;
            {
                const auto i=SendMessageW(action_list,LB_GETCURSEL,0,0);
                if(i>=0&&static_cast<std::size_t>(i)<action_names.size()) {
                    const auto label=utf8_to_wide(action_names[static_cast<std::size_t>(i)]);
                    SetWindowTextW(action_name,label.c_str());
                }
            }
            handled=true;break;
        case IdActionAdd:
        case IdActionRemove:
        case IdActionSave:
            if(notification!=BN_CLICKED)return false;
            {
                std::string err;
                bool ok=true;
                if(id==IdActionAdd||id==IdActionRemove){
                    wchar_t name[100]{};
                    GetWindowTextW(action_name,name,100);
                    const std::string action=wide_to_utf8(name);
                    if(id==IdActionRemove)ok=action_document.erase(action,&err);
                    else {
                        const auto index=SendMessageW(action_key,CB_GETCURSEL,0,0);
                        ok=index>=0&&static_cast<std::size_t>(index)<action_key_options.size()&&
                           action_document.bind(action,action_key_options[static_cast<std::size_t>(index)],&err);
                    }
                }
                if(ok)ok=action_document.save(&err);
                if(ok) {
                    refresh_action_editor();
                    editor.console().info("Input Actions","Project action-map saved");
                }else editor.console().warning("Input Actions",err);
            }
            handled=true;break;

        case IdGameView:
            if(notification!=BN_CLICKED)return false;
            game_view_visible = SendMessageW(game_toggle,BM_GETCHECK,0,0)==BST_CHECKED;
            {
                RECT r{};GetClientRect(host,&r);
                layout(r.right-r.left,r.bottom-r.top);
                InvalidateRect(scene,nullptr,FALSE);
                InvalidateRect(game_view,nullptr,FALSE);
            }
            handled=true;
            break;

        case IdVulkanPreview:
            if (notification != BN_CLICKED) return false;
            {
                const bool requested =
                    SendMessageW(
                        vulkan_preview,
                        BM_GETCHECK,
                        0,
                        0) == BST_CHECKED;

                if (requested &&
                    (!vulkan_context ||
                     !vulkan_context->ready() ||
                     !vulkan_diagnostic_scene ||
                     !vulkan_diagnostic_scene->ready())) {

                    vulkan_preview_enabled =
                        false;

                    SendMessageW(
                        vulkan_preview,
                        BM_SETCHECK,
                        BST_UNCHECKED,
                        0);

                    editor.console().warning(
                        "Renderer",
                        "Vulkan Preview is unavailable: the diagnostic Vulkan pipeline is not ready.");
                } else {
                    vulkan_preview_enabled =
                        requested;

                    editor.console().info(
                        "Renderer",
                        requested
                            ? "Vulkan Preview enabled: presentation World Camera/MeshRenderer rendering is active."
                            : "Vulkan Preview disabled: returned to GDI Scene View.");

                    if (scene) {
                        InvalidateRect(
                            scene,
                            nullptr,
                            TRUE);
                    }
                }
            }
            handled = true;
            break;

        case IdPause:
            if (notification != BN_CLICKED) return false;
            if (editor.play_session().state() ==
                nengine::editor::PlayState::Playing) {
                editor.play_session().pause();
            } else if (
                editor.play_session().state() ==
                nengine::editor::PlayState::Paused) {
                editor.play_session().resume();
            }
            handled = true;
            break;

        case IdStep:
            if (notification != BN_CLICKED) return false;
            editor.play_session().step();
            handled = true;
            break;

        case IdHierarchy:
            if(notification!=LBN_SELCHANGE)return false;
            {
                const int selected_count=static_cast<int>(
                    SendMessageW(hierarchy,LB_GETSELCOUNT,0,0));
                if(selected_count>=0&&selected_count<=static_cast<int>(hierarchy_rows.size())){
                    std::vector<int> indexes(static_cast<std::size_t>(selected_count));
                    SendMessageW(hierarchy,LB_GETSELITEMS,indexes.size(),
                       reinterpret_cast<LPARAM>(indexes.data()));
                    editor.selection().clear();
                    for(const int i:indexes)
                        if(i>=0&&static_cast<std::size_t>(i)<hierarchy_rows.size())
                            editor.selection().add(hierarchy_rows[static_cast<std::size_t>(i)].entity);
                    const int caret=static_cast<int>(SendMessageW(hierarchy,LB_GETCARETINDEX,0,0));
                    if(caret>=0&&static_cast<std::size_t>(caret)<hierarchy_rows.size()&&
                       SendMessageW(hierarchy,LB_GETSEL,caret,0)>0)
                       editor.selection().add(hierarchy_rows[static_cast<std::size_t>(caret)].entity);
                }
            }
            handled=true;break;
        case IdDuplicateHierarchy:
            if(notification!=BN_CLICKED)return false;
            if(!editor.duplicate_selected_hierarchy())
                editor.console().warning("Hierarchy","Nothing duplicated");
            handled=true;break;
        case IdParentHierarchy:
            if(notification!=BN_CLICKED)return false;
            if(!editor.reparent_selected_to_active())
                editor.console().warning("Hierarchy","Choose multiple entities; the active row becomes their parent");
            handled=true;break;

        case IdConsole:
            if(notification!=LBN_DBLCLK)return false;
            {
                const auto index=static_cast<int>(
                    SendMessageW(console,LB_GETCURSEL,0,0));
                const auto& entries=editor.console().entries();
                if(index>=0&&static_cast<std::size_t>(index)<entries.size()&&
                   !entries[static_cast<std::size_t>(index)].source_file.empty())
                    open_external_path(entries[static_cast<std::size_t>(index)].source_file,
                        "C# Build");
            }
            handled=true;
            break;

        case IdAssets:
            if (notification == LBN_SELCHANGE) {
                refresh_generic_property_value();
                return true;
            }

            if (notification != LBN_DBLCLK) return false;
            {
                const int index =
                    static_cast<int>(
                        SendMessageW(
                            assets_list,
                            LB_GETCURSEL,
                            0,
                            0));

                if (index >= 0 &&
                    static_cast<std::size_t>(index) <
                        asset_rows.size()) {

                    const auto& asset =
                        asset_rows[
                            static_cast<std::size_t>(
                                index)];

                    if (asset.importer_id == "NEngine.Canvas") {
                        // Never switch the runtime Canvas asset out from under
                        // an unsaved authoring draft, even when clicked again.
                        if (canvas_mode && editor.has_ui_canvas_draft() &&
                            editor.ui_edit_dirty()) {
                            editor.console().warning(
                                "UI", "Save Canvas edits before switching UI assets.");
                            handled = true;
                            break;
                        }
                        // Allow explicit Canvas selection; the project
                        // UI/Main convention remains only a fallback.
                        editor.project().import_supported_assets();
                        if (editor.set_ui_canvas_asset(asset.guid)) {
                            editor.console().info(
                                "UI", "Selected Canvas preview: " +
                                    asset.relative_path.generic_string());
                            if (editor.can_edit()) {
                                std::string edit_error;
                                if (editor.begin_ui_canvas_edit(asset.guid, &edit_error)) {
                                    canvas_mode = true;
                                    if (vulkan_preview_enabled) {
                                        vulkan_preview_enabled = false;
                                        SendMessageW(vulkan_preview,
                                            BM_SETCHECK, BST_UNCHECKED, 0);
                                    }
                                    RECT client{};
                                    GetClientRect(host, &client);
                                    layout(client.right - client.left,
                                           client.bottom - client.top);
                                } else {
                                    editor.console().warning("UI", edit_error);
                                }
                            }
                            if (scene)
                                InvalidateRect(scene, nullptr, FALSE);
                        } else {
                            editor.console().warning(
                                "UI", "Canvas could not be loaded: " +
                                    asset.relative_path.generic_string());
                        }
                        refresh_console();
                        handled = true;
                        break;
                    }

                    const auto activation =
                        editor.project()
                            .activation_for(
                                asset.guid);

                    switch (activation.kind) {
                    case nengine::editor::
                        AssetActivationKind::OpenScene:
                        load_scene_path(
                            activation.source_path);
                        break;

                    case nengine::editor::
                        AssetActivationKind::OpenScript:
                        if (!open_external_path(
                                activation.source_path,
                                "Scripting")) {
                            generate_and_open_scripts();
                        }
                        break;

                    case nengine::editor::
                        AssetActivationKind::OpenExternal:
                        open_external_path(
                            activation.source_path,
                            "Assets");
                        break;

                    case nengine::editor::
                        AssetActivationKind::None:
                        editor.console().warning(
                            "Assets",
                            "Asset activation is unavailable.");
                        break;
                    }
                }
            }
            handled = true;
            break;

        case IdGenericProperties:
            if (notification != LBN_SELCHANGE) {
                return false;
            }

            // A selection change only needs to refresh the value edit.
            // Calling refresh() here would rebuild this list and reset
            // the click/keyboard selection to its first item.
            refresh_generic_property_value();
            return true;

        case IdRemoveComponent:
            if(notification!=BN_CLICKED)return false;
            if(editor.can_edit()&&!generic_property_rows.empty()){
                const auto index=static_cast<int>(
                    SendMessageW(generic_properties,LB_GETCURSEL,0,0));
                if(index>=0&&static_cast<std::size_t>(index)<generic_property_rows.size()){
                    const auto type=generic_property_rows[static_cast<std::size_t>(index)].component;
                    if(editor.remove_component_from_selection(type))
                        editor.console().info("Inspector","Removed selected component");
                    else editor.console().warning("Inspector","Cannot remove selected component");
                }
            }
            handled=true;break;

        case IdApplyProperty:
            if (notification != BN_CLICKED) {
                return false;
            }

            apply_generic_property_edit();
            handled = true;
            break;

        case IdAssignAsset:
            if (notification != BN_CLICKED) {
                return false;
            }

            assign_selected_asset_to_property();
            handled = true;
            break;

        case IdAddComponent:
            if (notification != BN_CLICKED) {
                return false;
            }

            if (editor.can_edit() &&
                add_component_list) {

                const int index =
                    static_cast<int>(
                        SendMessageW(
                            add_component_list,
                            CB_GETCURSEL,
                            0,
                            0));

                const auto entity =
                    editor.selection()
                        .active();

                if (index >= 0 &&
                    static_cast<std::size_t>(
                        index) <
                        add_component_rows
                            .size() &&
                    editor.world()
                        .is_alive(
                            entity)) {

                    const auto type =
                        add_component_rows[
                            static_cast<
                                std::size_t>(
                                    index)];

                    const auto* descriptor =
                        editor.component_registry()
                            .find(
                                type);

                    const bool added =
                        editor.add_component_to_selection(type);

                    if (added) {
                        editor.console().info(
                            "Inspector",
                            "Added component " +
                                (descriptor
                                    ? descriptor
                                        ->name
                                    : std::string{
                                        "unknown"}));
                    } else {
                        editor.console().warning(
                            "Inspector",
                            "Component could not be added.");
                    }
                }
            }

            handled = true;
            break;

        case IdName:
            if (notification != EN_KILLFOCUS) return false;
            if (editor.can_edit()) {
                const auto entity =
                    editor.selection().active();

                const auto value =
                    read_text(name);

                if (!value.empty() &&
                    editor.world().is_alive(entity) &&
                    editor.world().name(entity) != value) {

                    editor.commands().execute(
                        editor.world(),
                        std::make_unique<
                            nengine::editor::
                                RenameEntityCommand>(
                                    entity,
                                    value));
                }
            }
            handled = true;
            break;

        case IdActive:
            if (notification != BN_CLICKED) return false;
            if (editor.can_edit()) {
                const auto entity =
                    editor.selection().active();

                const bool value =
                    SendMessageW(
                        active,
                        BM_GETCHECK,
                        0,
                        0) == BST_CHECKED;

                editor.commands().execute(
                    editor.world(),
                    std::make_unique<
                        nengine::editor::
                            SetActiveCommand>(
                                entity,
                                value));
            }
            handled = true;
            break;

        case IdApplyTransform:
            if (notification != BN_CLICKED) return false;
            apply_transform_edit();
            handled = true;
            break;

        default:
            return false;
        }

        if (!handled) {
            return false;
        }

        editor.sanitize_selection();
        refresh();
        return true;
    }

    void begin_scene_pointer(
        HWND hwnd,
        int x,
        int y) {

        RECT rect{};
        if (!GetClientRect(hwnd, &rect)) return;

        const float width =
            static_cast<float>(
                rect.right - rect.left);

        const float height =
            static_cast<float>(
                rect.bottom - rect.top);

        if (canvas_mode && editor.has_ui_canvas_draft()) {
            const auto id = nengine::ui::hit_test_ui(
                editor.ui_edit_layout(width, height),
                {static_cast<float>(x), static_cast<float>(y)});
            const bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrl) editor.toggle_ui_widget(id);
            else if (!editor.is_ui_widget_selected(id))
                editor.select_ui_widget(id);
            // Ctrl-click edits selection only; dragging a selected member
            // moves the entire selection.
            if (!ctrl && id != nengine::ui::invalid_widget &&
                editor.ui_edit_begin_drag()) {
                canvas_dragging = true;
                canvas_drag_resize = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                canvas_drag_start_x = x;
                canvas_drag_start_y = y;
                SetCapture(hwnd);
            }
            refresh();
            return;
        }

        const auto selected =
            editor.selection().active();

        if (editor.can_edit() &&
            editor.world().is_alive(selected)) {

            const auto axis =
                nengine::editor::
                    hit_test_translate_gizmo(
                        editor.world(),
                        selected,
                        static_cast<float>(x),
                        static_cast<float>(y),
                        width,
                        height);

            if (axis !=
                nengine::editor::
                    SceneGizmoAxis::None) {

                const auto* transform =
                    editor.world().transform(
                        selected);

                if (transform) {
                    scene_dragging = true;
                    scene_drag_axis = axis;
                    scene_drag_entity = selected;
                    scene_drag_original =
                        *transform;
                    scene_drag_start_x = x;
                    scene_drag_start_y = y;

                    SetCapture(hwnd);
                    return;
                }
            }
        }

        const auto picked =
            nengine::editor::pick_scene_entity(
                editor.presentation_world(),
                static_cast<float>(x),
                static_cast<float>(y),
                width,
                height);

        if (picked.valid()) {
            editor.selection().set(picked);
        } else {
            editor.selection().clear();
        }

        refresh();
    }

    void update_scene_drag(
        int x,
        int y) {

        if (!scene_dragging ||
            !editor.can_edit() ||
            !editor.world().is_alive(
                scene_drag_entity)) {
            return;
        }

        auto* transform =
            editor.world().transform(
                scene_drag_entity);

        if (!transform) return;

        auto preview =
            scene_drag_original;

        preview.local_position =
            nengine::editor::
                translated_local_position_from_drag(
                    scene_drag_original
                        .local_position,
                    scene_drag_axis,
                    static_cast<float>(
                        x - scene_drag_start_x),
                    static_cast<float>(
                        y - scene_drag_start_y));

        *transform = preview;

        if (!refreshing) {
            refreshing = true;
            refresh_inspector();
            refreshing = false;
        }

        if (scene) {
            InvalidateRect(
                scene,
                nullptr,
                FALSE);
        }
    }

    void end_scene_drag() {
        if (!scene_dragging) return;

        const auto entity =
            scene_drag_entity;

        auto* transform =
            editor.world().transform(entity);

        nengine::core::Transform final_value{};
        bool has_final_value = false;

        if (transform) {
            final_value = *transform;
            has_final_value = true;
        }

        scene_dragging = false;
        scene_drag_axis =
            nengine::editor::
                SceneGizmoAxis::None;
        scene_drag_entity =
            nengine::core::Entity::invalid();

        ReleaseCapture();

        if (transform &&
            has_final_value) {

            const bool changed =
                final_value.local_position !=
                scene_drag_original
                    .local_position;

            *transform =
                scene_drag_original;

            if (changed) {
                editor.commands().execute(
                    editor.world(),
                    std::make_unique<
                        nengine::editor::
                            SetTransformCommand>(
                                entity,
                                final_value));
            }
        }

        refresh();
    }

    void cancel_scene_drag() {
        if (!scene_dragging) return;

        if (auto* transform =
                editor.world().transform(
                    scene_drag_entity)) {
            *transform =
                scene_drag_original;
        }

        scene_dragging = false;
        scene_drag_axis =
            nengine::editor::
                SceneGizmoAxis::None;
        scene_drag_entity =
            nengine::core::Entity::invalid();

        refresh();
    }

    void paint_scene(HDC dc, HWND hwnd) {
        RECT rect{};
        GetClientRect(hwnd, &rect);

        HBRUSH background =
            CreateSolidBrush(RGB(45, 47, 52));

        FillRect(dc, &rect, background);
        DeleteObject(background);

        HPEN grid_pen =
            CreatePen(
                PS_SOLID,
                1,
                RGB(65, 68, 74));

        HPEN old_pen =
            static_cast<HPEN>(
                SelectObject(dc, grid_pen));

        const int cx =
            (rect.right - rect.left) / 2;

        const int cy =
            (rect.bottom - rect.top) / 2;

        constexpr int spacing = 25;

        for (int x = cx % spacing;
             x < rect.right;
             x += spacing) {
            MoveToEx(dc, x, 0, nullptr);
            LineTo(dc, x, rect.bottom);
        }

        for (int y = cy % spacing;
             y < rect.bottom;
             y += spacing) {
            MoveToEx(dc, 0, y, nullptr);
            LineTo(dc, rect.right, y);
        }

        SelectObject(dc, old_pen);
        DeleteObject(grid_pen);

        const auto& world = hwnd==game_view ? editor.presentation_world() : editor.world();

        const auto selected = hwnd==game_view ?
            nengine::core::Entity::invalid() : editor.selection().active();
        SetBkMode(dc,TRANSPARENT);
        SetTextColor(dc,RGB(240,240,240));
        const wchar_t* title=hwnd==game_view?L"Game View (runtime)":L"Scene View (authoring)";
        TextOutW(dc,8,7,title,static_cast<int>(wcslen(title)));

        for (const auto entity :
             world.entities()) {

            const auto* transform =
                world.transform(entity);

            if (!transform) continue;

            const auto point =
                nengine::editor::
                    scene_entity_to_screen(
                        world,
                        entity,
                        static_cast<float>(
                            rect.right -
                            rect.left),
                        static_cast<float>(
                            rect.bottom -
                            rect.top));

            const int x =
                static_cast<int>(point.x);

            const int y =
                static_cast<int>(point.y);

            const bool is_selected =
                entity == selected;

            HBRUSH brush =
                CreateSolidBrush(
                    is_selected
                        ? RGB(255, 165, 45)
                        : RGB(110, 180, 255));

            RECT marker{
                x - 5,
                y - 5,
                x + 6,
                y + 6};

            FillRect(dc, &marker, brush);
            DeleteObject(brush);

            SetBkMode(dc, TRANSPARENT);
            SetTextColor(
                dc,
                RGB(230, 230, 230));

            const auto label =
                utf8_to_wide(
                    world.name(entity));

            TextOutW(
                dc,
                x + 8,
                y - 8,
                label.c_str(),
                static_cast<int>(
                    label.size()));
        }

        if (world.is_alive(selected)) {
            const auto origin =
                nengine::editor::
                    scene_entity_to_screen(
                        world,
                        selected,
                        static_cast<float>(
                            rect.right -
                            rect.left),
                        static_cast<float>(
                            rect.bottom -
                            rect.top));

            const int ox =
                static_cast<int>(origin.x);

            const int oy =
                static_cast<int>(origin.y);

            HPEN x_pen =
                CreatePen(
                    PS_SOLID,
                    3,
                    RGB(230, 90, 90));

            HPEN previous =
                static_cast<HPEN>(
                    SelectObject(dc, x_pen));

            MoveToEx(dc, ox, oy, nullptr);
            LineTo(dc, ox + 54, oy);

            SelectObject(dc, previous);
            DeleteObject(x_pen);

            HPEN z_pen =
                CreatePen(
                    PS_SOLID,
                    3,
                    RGB(90, 170, 255));

            previous =
                static_cast<HPEN>(
                    SelectObject(dc, z_pen));

            MoveToEx(dc, ox, oy, nullptr);
            LineTo(dc, ox, oy - 54);

            SelectObject(dc, previous);
            DeleteObject(z_pen);

            SetBkMode(dc, TRANSPARENT);

            SetTextColor(
                dc,
                RGB(230, 90, 90));

            TextOutW(
                dc,
                ox + 58,
                oy - 7,
                L"X",
                1);

            SetTextColor(
                dc,
                RGB(90, 170, 255));

            TextOutW(
                dc,
                ox - 5,
                oy - 69,
                L"Z",
                1);
        }

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(
            dc,
            RGB(160, 165, 175));

        // Diagnostic GDI UI overlay: the final Vulkan Game View will use
        // backend-independent UiDrawQuad geometry from NEngineUI.
        const bool painting_ui_draft =
            canvas_mode && editor.has_ui_canvas_draft();
        if ((editor.play_session().is_playing() && editor.has_ui_canvas()) ||
            painting_ui_draft) {
            const auto layout = painting_ui_draft
                ? editor.ui_edit_layout(
                    static_cast<float>(rect.right),
                    static_cast<float>(rect.bottom))
                : editor.ui_layout(
                    static_cast<float>(rect.right),
                    static_cast<float>(rect.bottom));
            const auto draws = nengine::ui::build_ui_draw_list(layout);
            // A premultiplied 1x1 BGRA DIB alpha-blends each clipped
            // rectangle against the existing GDI Scene View.
            HDC source = CreateCompatibleDC(dc);
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = 1;
            info.bmiHeader.biHeight = -1;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            info.bmiHeader.biCompression = BI_RGB;
            void* bits = nullptr;
            HBITMAP pixel = source
                ? CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits,
                                   nullptr, 0)
                : nullptr;
            HGDIOBJ old_bitmap = pixel
                ? SelectObject(source, pixel) : nullptr;
            const auto clamped = [](float value) {
                return std::isfinite(value)
                    ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
            };
            for (const auto& draw : draws) {
                const float alpha = clamped(draw.color.a);
                if (alpha <= 0.0f) continue;
                RECT box{
                    static_cast<LONG>(draw.rect.x),
                    static_cast<LONG>(draw.rect.y),
                    static_cast<LONG>(draw.rect.x + draw.rect.width),
                    static_cast<LONG>(draw.rect.y + draw.rect.height)};
                if (box.right <= box.left || box.bottom <= box.top) continue;
                if (source && bits && pixel) {
                    const auto channel = [alpha, &clamped](float c) {
                        return static_cast<std::uint32_t>(
                            std::lround(clamped(c) * alpha * 255.0f));
                    };
                    const auto a = static_cast<std::uint32_t>(
                        std::lround(alpha * 255.0f));
                    *static_cast<std::uint32_t*>(bits) =
                        (a << 24u) | (channel(draw.color.r) << 16u) |
                        (channel(draw.color.g) << 8u) |
                        channel(draw.color.b);
                    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
                    AlphaBlend(dc, box.left, box.top,
                               box.right - box.left, box.bottom - box.top,
                               source, 0, 0, 1, 1, blend);
                } else {
                    // An allocation failure still keeps the UI visible.
                    const auto to_byte = [&clamped](float c) {
                        return static_cast<int>(clamped(c) * 255.0f);
                    };
                    HBRUSH fill = CreateSolidBrush(RGB(
                        to_byte(draw.color.r), to_byte(draw.color.g),
                        to_byte(draw.color.b)));
                    FillRect(dc, &box, fill);
                    DeleteObject(fill);
                }
            }
            if (source) {
                if (old_bitmap) SelectObject(source, old_bitmap);
                if (pixel) DeleteObject(pixel);
                DeleteDC(source);
            }
            for (const auto& item : layout.items) {
                if (item.text.empty() || item.clipped_rect.width <= 0 ||
                    item.clipped_rect.height <= 0) continue;
                SetTextColor(dc, RGB(245, 245, 245));
                const auto label = utf8_to_wide(item.text);
                RECT clip{
                    static_cast<LONG>(item.clipped_rect.x),
                    static_cast<LONG>(item.clipped_rect.y),
                    static_cast<LONG>(item.clipped_rect.x +
                                      item.clipped_rect.width),
                    static_cast<LONG>(item.clipped_rect.y +
                                      item.clipped_rect.height)};
                ExtTextOutW(dc,
                    clip.left + 4, clip.top + 4,
                    ETO_CLIPPED, &clip, label.c_str(),
                    static_cast<UINT>(label.size()), nullptr);
            }

            // Keyboard-selected (or mouse-selected) button is identifiable
            // even when no pointer is hovering over it. Use the already
            // clipped rectangle to avoid drawing outside the Canvas bounds.
            const auto focused_widget = painting_ui_draft
                ? editor.selected_ui_widget() : editor.focused_ui_widget();
            for (const auto& item : layout.items) {
                if ((painting_ui_draft
                     ? !editor.is_ui_widget_selected(item.id)
                     : item.id != focused_widget) ||
                    (!painting_ui_draft &&
                     (item.kind != nengine::ui::WidgetKind::Button ||
                      !item.interactable)) ||
                    item.clipped_rect.width <= 2.0f ||
                    item.clipped_rect.height <= 2.0f)
                    continue;
                RECT focus_rect{
                    static_cast<LONG>(item.clipped_rect.x) + 1,
                    static_cast<LONG>(item.clipped_rect.y) + 1,
                    static_cast<LONG>(item.clipped_rect.x +
                                      item.clipped_rect.width) - 1,
                    static_cast<LONG>(item.clipped_rect.y +
                                      item.clipped_rect.height) - 1};
                if (focus_rect.right > focus_rect.left &&
                    focus_rect.bottom > focus_rect.top)
                    DrawFocusRect(dc, &focus_rect);
                break;
            }
        }

        constexpr wchar_t caption[] =
            L"Scene View (diagnostic top-down preview; Vulkan viewport comes later)";

        TextOutW(
            dc,
            12,
            10,
            caption,
            static_cast<int>(
                std::size(caption) - 1));
    }
};

Win32EditorShell::Win32EditorShell(
    nengine::editor::EditorModel& editor)
    : impl_(std::make_unique<Impl>(editor)) {}

Win32EditorShell::~Win32EditorShell() {
    if (impl_ &&
        impl_->host &&
        IsWindow(impl_->host)) {
        DestroyWindow(impl_->host);
        impl_->host = nullptr;
    }
}

bool Win32EditorShell::attach(
    void* native_window) {
    return impl_->attach(native_window);
}

void Win32EditorShell::tick() {
    impl_->tick();
}

void Win32EditorShell::refresh() {
    impl_->refresh();
}

} // namespace nengine::app
