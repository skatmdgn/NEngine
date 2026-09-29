#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace nengine::platform {

struct WindowDesc {
    std::string title{"NEngine"};
    std::uint32_t width{1440};
    std::uint32_t height{900};
    bool resizable{true};
};

class Window {
public:
    explicit Window(WindowDesc desc = {});
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) noexcept;
    Window& operator=(Window&&) noexcept;

    bool open();
    void close();
    bool poll_events();

    bool is_open() const noexcept;
    bool close_requested() const noexcept;
    std::uint32_t client_width() const noexcept;
    std::uint32_t client_height() const noexcept;
    void* native_handle() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace nengine::platform
