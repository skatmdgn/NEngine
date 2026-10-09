#include "nengine/platform/window.hpp"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <string_view>
#include <utility>

namespace nengine::platform {
namespace {

constexpr wchar_t kWindowClassName[] = L"NEngine.EditorWindow";

std::wstring utf8_to_wide(std::string_view text) {
    if (text.empty()) return {};
    const int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (needed <= 0) return L"NEngine";
    std::wstring result(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), needed);
    return result;
}

LRESULT CALLBACK window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam) {
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    auto* close_requested = reinterpret_cast<bool*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (message) {
    case WM_CLOSE:
        if (close_requested) *close_requested = true;
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        if (close_requested) *close_requested = true;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }
}

input::Key translate_key(
    WPARAM wparam,
    LPARAM lparam) noexcept {

    const auto vk =
        static_cast<UINT>(
            wparam);

    if (vk >= 'A' && vk <= 'Z') {
        return static_cast<input::Key>(
            static_cast<std::uint16_t>(
                input::Key::A) +
            (vk - 'A'));
    }

    if (vk >= '0' && vk <= '9') {
        return static_cast<input::Key>(
            static_cast<std::uint16_t>(
                input::Key::Digit0) +
            (vk - '0'));
    }

    switch (vk) {
    case VK_SPACE:
        return input::Key::Space;
    case VK_RETURN:
        return input::Key::Enter;
    case VK_ESCAPE:
        return input::Key::Escape;
    case VK_TAB:
        return input::Key::Tab;
    case VK_BACK:
        return input::Key::Backspace;
    case VK_UP:
        return input::Key::Up;
    case VK_DOWN:
        return input::Key::Down;
    case VK_LEFT:
        return input::Key::Left;
    case VK_RIGHT:
        return input::Key::Right;
    case VK_LSHIFT:
        return input::Key::LeftShift;
    case VK_RSHIFT:
        return input::Key::RightShift;
    case VK_LCONTROL:
        return input::Key::LeftControl;
    case VK_RCONTROL:
        return input::Key::RightControl;
    case VK_LMENU:
        return input::Key::LeftAlt;
    case VK_RMENU:
        return input::Key::RightAlt;
    case VK_SHIFT: {
        const auto scan =
            static_cast<UINT>(
                (lparam >> 16) &
                0xff);

        const auto side =
            MapVirtualKeyW(
                scan,
                MAPVK_VSC_TO_VK_EX);

        return side == VK_RSHIFT
            ? input::Key::RightShift
            : input::Key::LeftShift;
    }
    case VK_CONTROL:
        return (lparam &
                (1ll << 24)) != 0
            ? input::Key::RightControl
            : input::Key::LeftControl;
    case VK_MENU:
        return (lparam &
                (1ll << 24)) != 0
            ? input::Key::RightAlt
            : input::Key::LeftAlt;
    default:
        return input::Key::Unknown;
    }
}

void consume_input_message(
    input::InputState& input_state,
    const MSG& message) noexcept {

    switch (message.message) {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        input_state.set_key(
            translate_key(
                message.wParam,
                message.lParam),
            true);
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        input_state.set_key(
            translate_key(
                message.wParam,
                message.lParam),
            false);
        break;

    case WM_LBUTTONDOWN:
        input_state.set_key(
            input::Key::MouseLeft,
            true);
        break;

    case WM_LBUTTONUP:
        input_state.set_key(
            input::Key::MouseLeft,
            false);
        break;

    case WM_RBUTTONDOWN:
        input_state.set_key(
            input::Key::MouseRight,
            true);
        break;

    case WM_RBUTTONUP:
        input_state.set_key(
            input::Key::MouseRight,
            false);
        break;

    case WM_MBUTTONDOWN:
        input_state.set_key(
            input::Key::MouseMiddle,
            true);
        break;

    case WM_MBUTTONUP:
        input_state.set_key(
            input::Key::MouseMiddle,
            false);
        break;

    case WM_MOUSEWHEEL:
        input_state.add_wheel(
            static_cast<float>(
                GET_WHEEL_DELTA_WPARAM(
                    message.wParam)) /
            static_cast<float>(
                WHEEL_DELTA));
        break;

    case WM_KILLFOCUS:
        input_state.release_all();
        break;

    default:
        break;
    }
}

bool ensure_window_class(HINSTANCE instance) {
    WNDCLASSEXW existing{};
    existing.cbSize = sizeof(existing);
    if (GetClassInfoExW(instance, kWindowClassName, &existing)) return true;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = window_proc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wc.lpszClassName = kWindowClassName;
    return RegisterClassExW(&wc) != 0;
}

} // namespace

struct Window::Impl {
    explicit Impl(WindowDesc value) : desc(std::move(value)) {}
    WindowDesc desc;
    HWND hwnd{nullptr};
    bool close_requested{false};
    input::InputState input_state{};
};

Window::Window(WindowDesc desc) : impl_(std::make_unique<Impl>(std::move(desc))) {}

Window::~Window() {
    if (impl_ && impl_->hwnd) {
        DestroyWindow(impl_->hwnd);
        impl_->hwnd = nullptr;
    }
}

Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

bool Window::open() {
    if (impl_->hwnd) return true;

    const HINSTANCE instance = GetModuleHandleW(nullptr);
    if (!ensure_window_class(instance)) return false;

    DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (impl_->desc.resizable) style |= WS_THICKFRAME | WS_MAXIMIZEBOX;

    RECT rect{0, 0, static_cast<LONG>(impl_->desc.width), static_cast<LONG>(impl_->desc.height)};
    AdjustWindowRectEx(&rect, style, FALSE, 0);

    const auto title = utf8_to_wide(impl_->desc.title);
    impl_->close_requested = false;
    impl_->hwnd = CreateWindowExW(
        0, kWindowClassName, title.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rect.right - rect.left, rect.bottom - rect.top,
        nullptr, nullptr, instance, &impl_->close_requested);

    if (!impl_->hwnd) return false;
    ShowWindow(impl_->hwnd, SW_SHOWDEFAULT);
    UpdateWindow(impl_->hwnd);
    return true;
}

void Window::close() {
    impl_->close_requested = true;
    if (impl_->hwnd) {
        DestroyWindow(impl_->hwnd);
        impl_->hwnd = nullptr;
    }
}

bool Window::poll_events() {
    impl_->input_state.begin_frame();

    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            impl_->close_requested = true;
        }

        consume_input_message(
            impl_->input_state,
            message);

        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    if (impl_->hwnd) {
        POINT cursor{};

        if (GetCursorPos(&cursor) &&
            ScreenToClient(
                impl_->hwnd,
                &cursor)) {

            impl_->input_state
                .set_pointer_position(
                    static_cast<float>(
                        cursor.x),
                    static_cast<float>(
                        cursor.y));
        }
    }

    if (impl_->close_requested &&
        impl_->hwnd) {
        impl_->hwnd = nullptr;
    }

    return impl_->hwnd != nullptr &&
        !impl_->close_requested;
}

bool Window::is_open() const noexcept { return impl_->hwnd != nullptr && !impl_->close_requested; }
bool Window::close_requested() const noexcept { return impl_->close_requested; }

std::uint32_t Window::client_width() const noexcept {
    if (!impl_->hwnd) return 0;
    RECT rect{};
    if (!GetClientRect(impl_->hwnd, &rect)) return 0;
    return static_cast<std::uint32_t>(std::max<LONG>(0, rect.right - rect.left));
}

std::uint32_t Window::client_height() const noexcept {
    if (!impl_->hwnd) return 0;
    RECT rect{};
    if (!GetClientRect(impl_->hwnd, &rect)) return 0;
    return static_cast<std::uint32_t>(std::max<LONG>(0, rect.bottom - rect.top));
}

void* Window::native_handle() const noexcept { return impl_->hwnd; }

input::InputState& Window::input_state() noexcept {
    return impl_->input_state;
}

const input::InputState& Window::input_state() const noexcept {
    return impl_->input_state;
}

} // namespace nengine::platform
