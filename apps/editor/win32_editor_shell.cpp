#include "win32_editor_shell.hpp"

#include <commctrl.h>
#include <windows.h>

#include <array>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "nengine/editor/command.hpp"
#include "nengine/editor/presentation.hpp"

namespace nengine::app {
namespace {

constexpr int kToolbarHeight = 38;
constexpr int kHierarchyWidth = 270;
constexpr int kInspectorWidth = 330;
constexpr int kBottomHeight = 150;
constexpr int kPadding = 8;

enum ControlId : int {
    IdUndo = 1001,
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
    IdConsole,
};

std::wstring widen_ascii(std::string_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (unsigned char ch : text) result.push_back(static_cast<wchar_t>(ch));
    return result;
}

std::string read_text(HWND control) {
    const int length = GetWindowTextLengthA(control);
    std::string value(static_cast<std::size_t>(length), '\0');
    if (length > 0) GetWindowTextA(control, value.data(), length + 1);
    return value;
}

void set_text(HWND control, std::string_view text) {
    SetWindowTextA(control, std::string{text}.c_str());
}

void set_float(HWND control, float value) {
    char buffer[64]{};
    std::snprintf(buffer, sizeof(buffer), "%.4f", static_cast<double>(value));
    SetWindowTextA(control, buffer);
}

bool read_float(HWND control, float& value) {
    const auto text = read_text(control);
    if (text.empty()) return false;
    char* end = nullptr;
    const float parsed = std::strtof(text.c_str(), &end);
    if (!end || end == text.c_str() || *end != '\0' || !std::isfinite(parsed)) return false;
    value = parsed;
    return true;
}

HWND create_control(
    HWND parent,
    const wchar_t* klass,
    const wchar_t* text,
    DWORD style,
    int id) {

    return CreateWindowExW(
        0, klass, text,
        WS_CHILD | WS_VISIBLE | style,
        0, 0, 10, 10,
        parent,
        reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
        GetModuleHandleW(nullptr),
        nullptr);
}

} // namespace

struct Win32EditorShell::Impl {
    explicit Impl(nengine::editor::EditorModel& value) : editor(value) {}

    nengine::editor::EditorModel& editor;
    HWND parent{nullptr};
    WNDPROC old_parent_proc{nullptr};

    HWND undo{nullptr};
    HWND redo{nullptr};
    HWND play{nullptr};
    HWND pause{nullptr};
    HWND step{nullptr};
    HWND stop{nullptr};

    HWND hierarchy{nullptr};
    HWND scene{nullptr};
    HWND inspector_panel{nullptr};
    HWND name{nullptr};
    HWND active{nullptr};

    std::array<HWND, 3> position{};
    std::array<HWND, 4> rotation{};
    std::array<HWND, 3> scale{};
    HWND apply_transform{nullptr};

    HWND console{nullptr};
    std::vector<nengine::editor::HierarchyRow> hierarchy_rows{};

    static LRESULT CALLBACK parent_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<Impl*>(GetPropW(hwnd, L"NEngineEditorShell"));
        if (!self) return DefWindowProcW(hwnd, message, wparam, lparam);

        switch (message) {
        case WM_SIZE:
            self->layout(LOWORD(lparam), HIWORD(lparam));
            return 0;
        case WM_COMMAND:
            if (self->on_command(LOWORD(wparam), HIWORD(wparam))) return 0;
            break;
        case WM_DESTROY:
            RemovePropW(hwnd, L"NEngineEditorShell");
            break;
        default:
            break;
        }
        return CallWindowProcW(self->old_parent_proc, hwnd, message, wparam, lparam);
    }

    static LRESULT CALLBACK scene_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = reinterpret_cast<Impl*>(create->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }

        if (message == WM_LBUTTONDOWN && self) {
            SetFocus(hwnd);
            return 0;
        }

        if (message == WM_PAINT && self) {
            PAINTSTRUCT paint{};
            HDC dc = BeginPaint(hwnd, &paint);
            self->paint_scene(dc, hwnd);
            EndPaint(hwnd, &paint);
            return 0;
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    bool attach(void* native) {
        parent = static_cast<HWND>(native);
        if (!parent) return false;

        SetPropW(parent, L"NEngineEditorShell", this);
        old_parent_proc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrW(parent, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&Impl::parent_proc)));
        if (!old_parent_proc) return false;

        WNDCLASSEXW scene_class{};
        scene_class.cbSize = sizeof(scene_class);
        if (!GetClassInfoExW(GetModuleHandleW(nullptr), L"NEngine.SceneView", &scene_class)) {
            scene_class.style = CS_HREDRAW | CS_VREDRAW;
            scene_class.lpfnWndProc = &Impl::scene_proc;
            scene_class.hInstance = GetModuleHandleW(nullptr);
            scene_class.hCursor = LoadCursorW(nullptr, IDC_CROSS);
            scene_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
            scene_class.lpszClassName = L"NEngine.SceneView";
            if (!RegisterClassExW(&scene_class)) return false;
        }

        undo = create_control(parent, L"BUTTON", L"Undo", BS_PUSHBUTTON, IdUndo);
        redo = create_control(parent, L"BUTTON", L"Redo", BS_PUSHBUTTON, IdRedo);
        play = create_control(parent, L"BUTTON", L"Play", BS_PUSHBUTTON, IdPlay);
        pause = create_control(parent, L"BUTTON", L"Pause", BS_PUSHBUTTON, IdPause);
        step = create_control(parent, L"BUTTON", L"Step", BS_PUSHBUTTON, IdStep);
        stop = create_control(parent, L"BUTTON", L"Stop", BS_PUSHBUTTON, IdStop);

        hierarchy = create_control(parent, L"LISTBOX", L"", LBS_NOTIFY | WS_BORDER | WS_VSCROLL, IdHierarchy);

        scene = CreateWindowExW(
            0, L"NEngine.SceneView", L"",
            WS_CHILD | WS_VISIBLE | WS_BORDER,
            0, 0, 10, 10,
            parent, nullptr, GetModuleHandleW(nullptr), this);

        inspector_panel = create_control(parent, L"STATIC", L"Inspector", SS_LEFT, 0);

        create_control(parent, L"STATIC", L"Name", SS_LEFT, 0);
        name = create_control(parent, L"EDIT", L"", WS_BORDER | ES_AUTOHSCROLL, IdName);
        active = create_control(parent, L"BUTTON", L"Active", BS_AUTOCHECKBOX, IdActive);

        create_control(parent, L"STATIC", L"Position X/Y/Z", SS_LEFT, 0);
        position[0] = create_control(parent, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdPosX);
        position[1] = create_control(parent, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdPosY);
        position[2] = create_control(parent, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdPosZ);

        create_control(parent, L"STATIC", L"Rotation X/Y/Z/W", SS_LEFT, 0);
        rotation[0] = create_control(parent, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdRotX);
        rotation[1] = create_control(parent, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdRotY);
        rotation[2] = create_control(parent, L"EDIT", L"0", WS_BORDER | ES_AUTOHSCROLL, IdRotZ);
        rotation[3] = create_control(parent, L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL, IdRotW);

        create_control(parent, L"STATIC", L"Scale X/Y/Z", SS_LEFT, 0);
        scale[0] = create_control(parent, L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL, IdScaleX);
        scale[1] = create_control(parent, L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL, IdScaleY);
        scale[2] = create_control(parent, L"EDIT", L"1", WS_BORDER | ES_AUTOHSCROLL, IdScaleZ);

        apply_transform = create_control(parent, L"BUTTON", L"Apply Transform", BS_PUSHBUTTON, IdApplyTransform);
        console = create_control(parent, L"LISTBOX", L"", WS_BORDER | WS_VSCROLL, IdConsole);
        SendMessageW(console, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"NEngine Console"));
        SendMessageW(console, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Editor shell attached."));

        RECT client{};
        GetClientRect(parent, &client);
        layout(client.right - client.left, client.bottom - client.top);
        return true;
    }

    void layout(int width, int height) {
        if (!parent) return;
        width = (width < 900) ? 900 : width;
        height = (height < 600) ? 600 : height;

        int x = kPadding;
        const int button_width = 78;
        const int button_height = 26;
        MoveWindow(undo, x, 6, button_width, button_height, TRUE); x += button_width + 4;
        MoveWindow(redo, x, 6, button_width, button_height, TRUE); x += button_width + 18;
        MoveWindow(play, x, 6, button_width, button_height, TRUE); x += button_width + 4;
        MoveWindow(pause, x, 6, button_width, button_height, TRUE); x += button_width + 4;
        MoveWindow(step, x, 6, button_width, button_height, TRUE); x += button_width + 4;
        MoveWindow(stop, x, 6, button_width, button_height, TRUE);

        const int content_top = kToolbarHeight;
        const int bottom_top = height - kBottomHeight;
        const int content_height = bottom_top - content_top - kPadding;

        MoveWindow(hierarchy, kPadding, content_top, kHierarchyWidth - kPadding, content_height, TRUE);

        const int inspector_x = width - kInspectorWidth;
        MoveWindow(scene, kHierarchyWidth, content_top, inspector_x - kHierarchyWidth - kPadding, content_height, TRUE);

        int iy = content_top + 8;
        const int ix = inspector_x + 10;
        const int iw = kInspectorWidth - 20;

        MoveWindow(inspector_panel, ix, iy, iw, 20, TRUE); iy += 28;

        const auto children = std::vector<HWND>{};
        HWND current = GetWindow(parent, GW_CHILD);
        std::vector<HWND> statics;
        while (current) {
            wchar_t cls[32]{};
            GetClassNameW(current, cls, 32);
            if (std::wstring_view{cls} == L"Static" && current != inspector_panel) statics.push_back(current);
            current = GetWindow(current, GW_HWNDNEXT);
        }

        // Static controls were created in Name/Position/Rotation/Scale order.
        if (statics.size() >= 4) {
            MoveWindow(statics[0], ix, iy + 4, 70, 20, TRUE);
        }
        MoveWindow(name, ix + 72, iy, iw - 72, 24, TRUE); iy += 32;
        MoveWindow(active, ix, iy, 100, 24, TRUE); iy += 34;

        if (statics.size() >= 4) MoveWindow(statics[1], ix, iy + 4, iw, 20, TRUE); iy += 24;
        const int third = (iw - 8) / 3;
        for (int i = 0; i < 3; ++i) MoveWindow(position[i], ix + i * (third + 4), iy, third, 24, TRUE);
        iy += 34;

        if (statics.size() >= 4) MoveWindow(statics[2], ix, iy + 4, iw, 20, TRUE); iy += 24;
        const int quarter = (iw - 12) / 4;
        for (int i = 0; i < 4; ++i) MoveWindow(rotation[i], ix + i * (quarter + 4), iy, quarter, 24, TRUE);
        iy += 34;

        if (statics.size() >= 4) MoveWindow(statics[3], ix, iy + 4, iw, 20, TRUE); iy += 24;
        for (int i = 0; i < 3; ++i) MoveWindow(scale[i], ix + i * (third + 4), iy, third, 24, TRUE);
        iy += 34;

        MoveWindow(apply_transform, ix, iy, iw, 27, TRUE);
        MoveWindow(console, kPadding, bottom_top + kPadding, width - 2 * kPadding, kBottomHeight - 2 * kPadding, TRUE);
    }

    void refresh() {
        refresh_toolbar();
        refresh_hierarchy();
        refresh_inspector();
        if (scene) InvalidateRect(scene, nullptr, TRUE);
    }

    void refresh_toolbar() {
        const auto state = nengine::editor::build_toolbar(editor);
        EnableWindow(undo, state.can_undo);
        EnableWindow(redo, state.can_redo);
        EnableWindow(play, state.can_play);
        EnableWindow(stop, state.can_stop);
        EnableWindow(pause, state.can_pause || state.can_resume);
        EnableWindow(step, state.can_step);

        SetWindowTextW(undo, widen_ascii(state.undo_label).c_str());
        SetWindowTextW(redo, widen_ascii(state.redo_label).c_str());
        SetWindowTextW(pause, state.can_resume ? L"Resume" : L"Pause");
    }

    void refresh_hierarchy() {
        hierarchy_rows = nengine::editor::build_hierarchy(editor);
        SendMessageW(hierarchy, LB_RESETCONTENT, 0, 0);

        int selected_index = -1;
        for (std::size_t i = 0; i < hierarchy_rows.size(); ++i) {
            const auto& row = hierarchy_rows[i];
            std::string text(row.depth * 3u, ' ');
            if (!row.active) text += "[off] ";
            text += row.name;
            SendMessageW(hierarchy, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(widen_ascii(text).c_str()));
            if (row.selected) selected_index = static_cast<int>(i);
        }
        if (selected_index >= 0) SendMessageW(hierarchy, LB_SETCURSEL, selected_index, 0);
    }

    void refresh_inspector() {
        const auto snapshot = nengine::editor::build_inspector(editor);
        const bool enabled = snapshot.valid && editor.can_edit();

        EnableWindow(name, enabled);
        EnableWindow(active, enabled);
        EnableWindow(apply_transform, enabled);
        for (auto handle : position) EnableWindow(handle, enabled);
        for (auto handle : rotation) EnableWindow(handle, enabled);
        for (auto handle : scale) EnableWindow(handle, enabled);

        if (!snapshot.valid) {
            set_text(name, "");
            SendMessageW(active, BM_SETCHECK, BST_UNCHECKED, 0);
            return;
        }

        set_text(name, snapshot.name);
        SendMessageW(active, BM_SETCHECK, snapshot.active ? BST_CHECKED : BST_UNCHECKED, 0);

        const auto& world = editor.presentation_world();
        const auto* transform = world.transform(snapshot.entity);
        if (!transform) return;

        set_float(position[0], transform->local_position.x);
        set_float(position[1], transform->local_position.y);
        set_float(position[2], transform->local_position.z);

        set_float(rotation[0], transform->local_rotation.x);
        set_float(rotation[1], transform->local_rotation.y);
        set_float(rotation[2], transform->local_rotation.z);
        set_float(rotation[3], transform->local_rotation.w);

        set_float(scale[0], transform->local_scale.x);
        set_float(scale[1], transform->local_scale.y);
        set_float(scale[2], transform->local_scale.z);
    }

    bool apply_transform_edit() {
        if (!editor.can_edit()) return false;
        const auto entity = editor.selection().active();
        const auto* current = editor.world().transform(entity);
        if (!current) return false;

        auto value = *current;
        if (!read_float(position[0], value.local_position.x) ||
            !read_float(position[1], value.local_position.y) ||
            !read_float(position[2], value.local_position.z) ||
            !read_float(rotation[0], value.local_rotation.x) ||
            !read_float(rotation[1], value.local_rotation.y) ||
            !read_float(rotation[2], value.local_rotation.z) ||
            !read_float(rotation[3], value.local_rotation.w) ||
            !read_float(scale[0], value.local_scale.x) ||
            !read_float(scale[1], value.local_scale.y) ||
            !read_float(scale[2], value.local_scale.z)) {
            SendMessageW(console, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Transform edit rejected: invalid number."));
            return false;
        }

        return editor.commands().execute(
            editor.world(),
            std::make_unique<nengine::editor::SetTransformCommand>(entity, value));
    }

    bool on_command(int id, int notification) {
        switch (id) {
        case IdUndo:
            if (notification == BN_CLICKED) editor.commands().undo(editor.world());
            break;
        case IdRedo:
            if (notification == BN_CLICKED) editor.commands().redo(editor.world());
            break;
        case IdPlay:
            if (notification == BN_CLICKED) editor.play_session().play(editor.world());
            break;
        case IdStop:
            if (notification == BN_CLICKED) editor.play_session().stop();
            break;
        case IdPause:
            if (notification == BN_CLICKED) {
                if (editor.play_session().state() == nengine::editor::PlayState::Playing) editor.play_session().pause();
                else if (editor.play_session().state() == nengine::editor::PlayState::Paused) editor.play_session().resume();
            }
            break;
        case IdStep:
            if (notification == BN_CLICKED) editor.play_session().step();
            break;
        case IdHierarchy:
            if (notification == LBN_SELCHANGE) {
                const int index = static_cast<int>(SendMessageW(hierarchy, LB_GETCURSEL, 0, 0));
                if (index >= 0 && static_cast<std::size_t>(index) < hierarchy_rows.size()) {
                    editor.selection().set(hierarchy_rows[static_cast<std::size_t>(index)].entity);
                }
            }
            break;
        case IdName:
            if (notification == EN_KILLFOCUS && editor.can_edit()) {
                const auto entity = editor.selection().active();
                const auto value = read_text(name);
                if (!value.empty() && editor.world().is_alive(entity) && editor.world().name(entity) != value) {
                    editor.commands().execute(
                        editor.world(),
                        std::make_unique<nengine::editor::RenameEntityCommand>(entity, value));
                }
            }
            break;
        case IdActive:
            if (notification == BN_CLICKED && editor.can_edit()) {
                const auto entity = editor.selection().active();
                const bool value = SendMessageW(active, BM_GETCHECK, 0, 0) == BST_CHECKED;
                editor.commands().execute(
                    editor.world(),
                    std::make_unique<nengine::editor::SetActiveCommand>(entity, value));
            }
            break;
        case IdApplyTransform:
            if (notification == BN_CLICKED) apply_transform_edit();
            break;
        default:
            return false;
        }

        editor.sanitize_selection();
        refresh();
        return true;
    }

    void paint_scene(HDC dc, HWND hwnd) {
        RECT rect{};
        GetClientRect(hwnd, &rect);

        HBRUSH background = CreateSolidBrush(RGB(45, 47, 52));
        FillRect(dc, &rect, background);
        DeleteObject(background);

        HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(65, 68, 74));
        HPEN old_pen = static_cast<HPEN>(SelectObject(dc, grid_pen));

        const int cx = (rect.right - rect.left) / 2;
        const int cy = (rect.bottom - rect.top) / 2;
        constexpr int spacing = 25;

        for (int x = cx % spacing; x < rect.right; x += spacing) {
            MoveToEx(dc, x, 0, nullptr);
            LineTo(dc, x, rect.bottom);
        }
        for (int y = cy % spacing; y < rect.bottom; y += spacing) {
            MoveToEx(dc, 0, y, nullptr);
            LineTo(dc, rect.right, y);
        }

        SelectObject(dc, old_pen);
        DeleteObject(grid_pen);

        const auto& world = editor.presentation_world();
        const auto selected = editor.selection().active();

        for (const auto entity : world.entities()) {
            const auto* transform = world.transform(entity);
            if (!transform) continue;

            const int x = cx + static_cast<int>(transform->local_position.x * 25.0f);
            const int y = cy - static_cast<int>(transform->local_position.z * 25.0f);
            const bool is_selected = entity == selected;

            HBRUSH brush = CreateSolidBrush(is_selected ? RGB(255, 165, 45) : RGB(110, 180, 255));
            RECT marker{x - 5, y - 5, x + 6, y + 6};
            FillRect(dc, &marker, brush);
            DeleteObject(brush);

            SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, RGB(230, 230, 230));
            const auto label = widen_ascii(world.name(entity));
            TextOutW(dc, x + 8, y - 8, label.c_str(), static_cast<int>(label.size()));
        }

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(160, 165, 175));
        const wchar_t* caption = L"Scene View (top-down editor preview; Vulkan viewport comes later)";
        TextOutW(dc, 12, 10, caption, static_cast<int>(wcslen(caption)));
    }
};

Win32EditorShell::Win32EditorShell(nengine::editor::EditorModel& editor)
    : impl_(std::make_unique<Impl>(editor)) {}

Win32EditorShell::~Win32EditorShell() {
    if (impl_ && impl_->parent && impl_->old_parent_proc) {
        SetWindowLongPtrW(impl_->parent, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(impl_->old_parent_proc));
        RemovePropW(impl_->parent, L"NEngineEditorShell");
    }
}

bool Win32EditorShell::attach(void* native_window) {
    return impl_->attach(native_window);
}

void Win32EditorShell::refresh() {
    impl_->refresh();
}

} // namespace nengine::app
