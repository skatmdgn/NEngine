#pragma once
#include <filesystem>
#include <string>

namespace nengine::scripting {

// Native plugins are explicit trusted opt-ins, not loaded during asset scan.
// Restrict to a canonical Plugin directory and don't follow symlink shortcuts.
class NativePlugin {
public:
    NativePlugin()=default;
    NativePlugin(const NativePlugin&)=delete;
    NativePlugin& operator=(const NativePlugin&)=delete;
    ~NativePlugin(){close();}
    bool open(const std::filesystem::path& plugins_root,
              const std::filesystem::path& library,
              std::string* error=nullptr);
    void* function(const char* name) const noexcept;
    void close() noexcept;
    bool loaded() const noexcept {return handle_!=nullptr;}
private:
    void* handle_{nullptr};
};
} // namespace nengine::scripting
