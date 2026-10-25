#include "nengine/scripting/native_plugin.hpp"
#include <system_error>
#include <utility>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace nengine::scripting {
namespace {
bool fail(std::string* error,const char* message) {
    if(error)*error=message;
    return false;
}
bool symlink_in_path(const std::filesystem::path& root,
                     const std::filesystem::path& target) {
    auto current=target;
    while(!current.empty()) {
        std::error_code ec;
        if(std::filesystem::is_symlink(current,ec)||ec)return true;
        if(current==root)break;
        auto next=current.parent_path();
        if(next==current)break;
        current=std::move(next);
    }
    return current!=root;
}
}
bool NativePlugin::open(const std::filesystem::path& plugins_root,
                        const std::filesystem::path& library,
                        std::string* error) {
    if(handle_)return fail(error,"native plugin already loaded");
    if(plugins_root.empty()||library.empty()||!library.is_absolute())
        return fail(error,"plugin must be an absolute file within its Plugins root");
#if defined(_WIN32)
    if(library.extension()!=L".dll")
        return fail(error,"Windows plugin must have .dll extension");
#else
    if(library.extension()!=".so")
        return fail(error,"POSIX plugin must have .so extension");
#endif
    std::error_code ec;
    auto root=std::filesystem::weakly_canonical(plugins_root,ec);
    if(ec||!std::filesystem::is_directory(root,ec)||ec||
       std::filesystem::is_symlink(plugins_root,ec)||ec)
        return fail(error,"native Plugins root is invalid");
    if(std::filesystem::is_symlink(library,ec)||ec)
        return fail(error,"native plugin symlinked library rejected");
    auto file=std::filesystem::weakly_canonical(library,ec);
    if(ec||!std::filesystem::is_regular_file(file,ec)||ec||
       symlink_in_path(root,file))
        return fail(error,"plugin library path is unsafe");
    auto relative=file.lexically_relative(root);
    if(relative.empty()||relative.is_absolute()||
       *relative.begin()==".."||file==root)
        return fail(error,"plugin escapes Plugins root");
#if defined(_WIN32)
    auto h=LoadLibraryExW(file.c_str(),nullptr,
       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!h)return fail(error,"Windows LoadLibraryExW failed; missing native dependency or invalid DLL");
    handle_=h;
#else
    auto h=dlopen(file.c_str(),RTLD_NOW|RTLD_LOCAL);
    if(!h)return fail(error,"POSIX dlopen failed; missing dependency or invalid .so");
    handle_=h;
#endif
    if(error)error->clear();
    return true;
}
void* NativePlugin::function(const char* name) const noexcept {
    if(!handle_||!name||!*name)return nullptr;
#if defined(_WIN32)
    return reinterpret_cast<void*>(
        GetProcAddress(static_cast<HMODULE>(handle_),name));
#else
    return dlsym(handle_,name);
#endif
}
void NativePlugin::close() noexcept {
    if(!handle_)return;
#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif
    handle_=nullptr;
}
} // namespace nengine::scripting
