#include "nengine/platform/window.hpp"

#include <utility>

namespace nengine::platform {

struct Window::Impl {
    explicit Impl(WindowDesc value) : desc(std::move(value)) {}
    WindowDesc desc;
    bool open{false};
    bool close_requested{false};
};

Window::Window(WindowDesc desc) : impl_(std::make_unique<Impl>(std::move(desc))) {}
Window::~Window() = default;
Window::Window(Window&&) noexcept = default;
Window& Window::operator=(Window&&) noexcept = default;

bool Window::open() {
    impl_->open = true;
    return true;
}

void Window::close() {
    impl_->close_requested = true;
    impl_->open = false;
}

bool Window::poll_events() {
    return impl_->open && !impl_->close_requested;
}

bool Window::is_open() const noexcept { return impl_->open; }
bool Window::close_requested() const noexcept { return impl_->close_requested; }
std::uint32_t Window::client_width() const noexcept { return impl_->desc.width; }
std::uint32_t Window::client_height() const noexcept { return impl_->desc.height; }
void* Window::native_handle() const noexcept { return nullptr; }

} // namespace nengine::platform
