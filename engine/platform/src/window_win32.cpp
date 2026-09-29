#include "nengine/platform/window.hpp"

#include <windows.h>

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
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) impl_->close_requested = true;
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    if (impl_->close_requested && impl_->hwnd) impl_->hwnd = nullptr;
    return impl_->hwnd != nullptr && !impl_->close_requested;
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

} // namespace nengine::platform
