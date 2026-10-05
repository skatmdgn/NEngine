#include "win32_editor_shell.hpp"

#include <windows.h>
#include <commdlg.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
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
#include "nengine/scripting/managed_project.hpp"
#include "nengine/render/vulkan_context.hpp"

namespace nengine::app {
namespace {

constexpr wchar_t kHostClassName[] = L"NEngine.EditorHost";
constexpr wchar_t kSceneClassName[] = L"NEngine.SceneView";
constexpr wchar_t kSplitterClassName[] = L"NEngine.Splitter";

constexpr int kToolbarHeight = 38;
constexpr int kSplitterSize = 6;
constexpr int kPadding = 8;

enum ControlId : int {
    IdOpen = 1001,
    IdSave,
    IdNewEntity,
    IdDeleteEntity,
    IdRefreshAssets,
    IdScripts,
    IdUndo,
    IdRedo,
    IdPlay,
    IdPause,
    IdStep,
    IdStop,
    IdHierarchy,
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
    IdApplyProperty,
    IdAssets,
    IdConsole,
    IdSplitHierarchy,
    IdSplitInspector,
    IdSplitBottom,
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
    HWND undo{nullptr};
    HWND redo{nullptr};
    HWND play{nullptr};
    HWND pause{nullptr};
    HWND step{nullptr};
    HWND stop{nullptr};

    HWND hierarchy{nullptr};
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
    HWND apply_property{nullptr};
    std::vector<GenericPropertyBinding>
        generic_property_rows{};

    HWND assets_list{nullptr};
    HWND console{nullptr};

    HWND split_hierarchy{nullptr};
    HWND split_inspector{nullptr};
    HWND split_bottom{nullptr};
    std::vector<nengine::editor::HierarchyRow> hierarchy_rows{};
    std::vector<nengine::assets::AssetRecord> asset_rows{};
    std::filesystem::path current_scene_path{};
    ULONGLONG next_asset_poll_tick{0};

    std::unique_ptr<
        nengine::render::VulkanContext>
        vulkan_context{};

    int vulkan_scene_width{-1};
    int vulkan_scene_height{-1};

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
        case WM_LBUTTONDOWN:
            SetFocus(hwnd);
            self->begin_scene_pointer(
                hwnd,
                static_cast<int>(
                    static_cast<short>(LOWORD(lparam))),
                static_cast<int>(
                    static_cast<short>(HIWORD(lparam))));
            return 0;

        case WM_MOUSEMOVE:
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
            if (self->scene_dragging) {
                self->end_scene_drag();
                return 0;
            }
            break;

        case WM_CAPTURECHANGED:
            if (self->scene_dragging &&
                reinterpret_cast<HWND>(lparam) != hwnd) {
                self->cancel_scene_drag();
                return 0;
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            self->paint_scene(dc, hwnd);
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
        undo = create_control(host, L"BUTTON", L"Undo", BS_PUSHBUTTON, IdUndo);
        redo = create_control(host, L"BUTTON", L"Redo", BS_PUSHBUTTON, IdRedo);
        play = create_control(host, L"BUTTON", L"Play", BS_PUSHBUTTON, IdPlay);
        pause = create_control(host, L"BUTTON", L"Pause", BS_PUSHBUTTON, IdPause);
        step = create_control(host, L"BUTTON", L"Step", BS_PUSHBUTTON, IdStep);
        stop = create_control(host, L"BUTTON", L"Stop", BS_PUSHBUTTON, IdStop);

        if (!open_scene || !save_scene || !new_entity || !delete_entity ||
            !refresh_assets || !scripts || !undo || !redo ||
            !play || !pause || !step || !stop) {
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
            LBS_NOTIFY | WS_BORDER | WS_VSCROLL,
            IdHierarchy);

        if (!hierarchy) {
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

        apply_property = create_control(
            host,
            L"BUTTON",
            L"Apply Property",
            BS_PUSHBUTTON,
            IdApplyProperty);

        if (!inspector_title || !label_name || !name || !active ||
            !label_position || !position[0] || !position[1] || !position[2] ||
            !label_rotation || !rotation[0] || !rotation[1] ||
            !rotation[2] || !rotation[3] ||
            !label_scale || !scale[0] || !scale[1] || !scale[2] ||
            !apply_transform ||
            !generic_title ||
            !generic_properties ||
            !generic_value ||
            !apply_property) {
            shell_log("attach failed: inspector control creation");
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

        if (!assets_list || !console) {
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
                    " swapchain image(s). GDI Scene View presentation remains active for now.");
        } else {
            editor.console().info(
                "Renderer",
                "Vulkan bootstrap unavailable; using GDI Scene View: " +
                    context->diagnostic());
        }

        vulkan_context =
            std::move(context);
    }

    void poll_vulkan_scene_resize() {
        if (!vulkan_context ||
            !vulkan_context->ready() ||
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
        }
    }

    void tick() {
        if (!parent || !host || !IsWindow(parent) || !IsWindow(host)) {
            return;
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

        const ULONGLONG now = GetTickCount64();
        if (now >= next_asset_poll_tick) {
            next_asset_poll_tick = now + 1000ull;

            const auto result =
                editor.project().poll_assets();

            if (!result.changes.empty()) {
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
        x += button_width + 16;

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
            content_height,
            TRUE);

        const int scene_x =
            left_split_x +
            kSplitterSize;

        MoveWindow(
            scene,
            scene_x,
            content_top,
            std::max(
                10,
                right_split_x -
                    scene_x),
            content_height,
            TRUE);

        int iy =
            content_top + 8;

        const int ix =
            inspector_x + 10;

        const int iw =
            inspector_width - 20;

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
                    62);

        MoveWindow(
            generic_properties,
            ix,
            iy,
            iw,
            generic_list_height,
            TRUE);
        iy += generic_list_height + 4;

        MoveWindow(
            generic_value,
            ix,
            iy,
            iw,
            24,
            TRUE);
        iy += 28;

        MoveWindow(
            apply_property,
            ix,
            iy,
            iw,
            27,
            TRUE);

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

        const auto data =
            nengine::core::SceneSerializer::capture(
                editor.world(),
                scene_name,
                &editor.component_serialization());

        std::string error;
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

        refresh_assets_panel();
        refresh_console();
        refresh_window_title();

        shell_log("refresh scene invalidate");
        if (scene) {
            InvalidateRect(scene, nullptr, TRUE);
        }

        shell_log("refresh complete");
        refreshing = false;
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
            }
        }

        if (selected_index >= 0) {
            SendMessageW(
                hierarchy,
                LB_SETCURSEL,
                selected_index,
                0);
        }
    }

    void refresh_generic_property_editor(
        const nengine::editor::InspectorSnapshot& snapshot) {

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

            return;
        }

        SendMessageW(
            generic_properties,
            LB_SETCURSEL,
            0,
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

            set_text(
                generic_value,
                "");

            EnableWindow(
                generic_value,
                FALSE);

            EnableWindow(
                apply_property,
                FALSE);

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

        const auto solution =
            output.solution_path.wstring();

        const HINSTANCE launched =
            ShellExecuteW(
                host,
                L"open",
                solution.c_str(),
                nullptr,
                editor.project()
                    .root()
                    .wstring()
                    .c_str(),
                SW_SHOWNORMAL);

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
            editor.play_session().play(editor.world());
            handled = true;
            break;

        case IdStop:
            if (notification != BN_CLICKED) return false;
            editor.play_session().stop();
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
            if (notification != LBN_SELCHANGE) return false;
            {
                const int index =
                    static_cast<int>(
                        SendMessageW(
                            hierarchy,
                            LB_GETCURSEL,
                            0,
                            0));

                if (index >= 0 &&
                    static_cast<std::size_t>(index) <
                        hierarchy_rows.size()) {
                    editor.selection().set(
                        hierarchy_rows[
                            static_cast<std::size_t>(index)]
                            .entity);
                }
            }
            handled = true;
            break;

        case IdAssets:
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

            refresh_generic_property_value();
            handled = true;
            break;

        case IdApplyProperty:
            if (notification != BN_CLICKED) {
                return false;
            }

            apply_generic_property_edit();
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

        const auto& world =
            editor.presentation_world();

        const auto selected =
            editor.selection().active();

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
