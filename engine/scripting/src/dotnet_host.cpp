#include "nengine/scripting/dotnet_host.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <optional>
#include <set>
#include <sstream>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace nengine::scripting {
namespace {

void set_error(
    std::string* error,
    std::string message) {

    if (error) {
        *error = std::move(message);
    }
}

std::string json_escape(
    std::string_view value) {

    std::string result;
    result.reserve(
        value.size());

    for (const unsigned char ch :
         value) {

        switch (ch) {
        case '"':
            result += "\\\"";
            break;
        case '\\':
            result += "\\\\";
            break;
        case '\b':
            result += "\\b";
            break;
        case '\f':
            result += "\\f";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            if (ch < 0x20u) {
                std::ostringstream escaped;
                escaped
                    << "\\u"
                    << std::hex
                    << std::setw(4)
                    << std::setfill('0')
                    << static_cast<unsigned int>(
                        ch);
                result +=
                    escaped.str();
            } else {
                result.push_back(
                    static_cast<char>(
                        ch));
            }
            break;
        }
    }

    return result;
}

struct VersionKey {
    std::vector<unsigned long long>
        parts{};
    bool prerelease{false};
    std::string text{};
};

VersionKey version_key(
    std::string text) {

    VersionKey key;
    key.text =
        std::move(text);

    std::size_t i = 0u;

    while (i < key.text.size()) {
        if (!std::isdigit(
                static_cast<unsigned char>(
                    key.text[i]))) {
            key.prerelease = true;
            break;
        }

        unsigned long long value = 0u;

        while (i < key.text.size() &&
               std::isdigit(
                   static_cast<unsigned char>(
                       key.text[i]))) {

            const auto digit =
                static_cast<unsigned int>(
                    key.text[i] - '0');

            if (value >
                (std::numeric_limits<
                    unsigned long long>::max() -
                 digit) /
                    10u) {

                key.prerelease = true;
                return key;
            }

            value =
                value * 10u +
                digit;

            ++i;
        }

        key.parts.push_back(
            value);

        if (i >= key.text.size()) {
            break;
        }

        if (key.text[i] == '.') {
            ++i;
            continue;
        }

        key.prerelease = true;
        break;
    }

    return key;
}

bool newer_version(
    const VersionKey& left,
    const VersionKey& right) {

    const auto count =
        std::max(
            left.parts.size(),
            right.parts.size());

    for (std::size_t i = 0u;
         i < count;
         ++i) {

        const auto a =
            i < left.parts.size()
                ? left.parts[i]
                : 0u;

        const auto b =
            i < right.parts.size()
                ? right.parts[i]
                : 0u;

        if (a != b) {
            return a > b;
        }
    }

    if (left.prerelease !=
        right.prerelease) {

        return
            !left.prerelease;
    }

    return
        left.text >
        right.text;
}

std::filesystem::path
hostfxr_filename() {
#if defined(_WIN32)
    return "hostfxr.dll";
#elif defined(__APPLE__)
    return "libhostfxr.dylib";
#else
    return "libhostfxr.so";
#endif
}

void append_environment_root(
    std::vector<std::filesystem::path>& roots,
    const char* name) {

    if (const auto* value =
            std::getenv(name);
        value &&
        *value) {

        roots.emplace_back(
            value);
    }
}

std::vector<std::filesystem::path>
candidate_roots() {

    std::vector<std::filesystem::path>
        roots;

#if defined(_WIN32)
    append_environment_root(
        roots,
        "DOTNET_ROOT");
    append_environment_root(
        roots,
        "DOTNET_ROOT_X64");
    append_environment_root(
        roots,
        "DOTNET_ROOT(x86)");

    if (const auto* program_files =
            std::getenv(
                "ProgramW6432");
        program_files &&
        *program_files) {

        roots.emplace_back(
            std::filesystem::path{
                program_files} /
            "dotnet");
    }

    if (const auto* program_files =
            std::getenv(
                "ProgramFiles");
        program_files &&
        *program_files) {

        roots.emplace_back(
            std::filesystem::path{
                program_files} /
            "dotnet");
    }
#else
    append_environment_root(
        roots,
        "DOTNET_ROOT");
    append_environment_root(
        roots,
        "DOTNET_ROOT_X64");

    if (const auto* home =
            std::getenv("HOME");
        home &&
        *home) {

        roots.emplace_back(
            std::filesystem::path{
                home} /
            ".dotnet");
    }

    roots.emplace_back(
        "/usr/share/dotnet");
    roots.emplace_back(
        "/usr/local/share/dotnet");
#endif

    std::vector<std::filesystem::path>
        unique;

    std::set<std::string>
        seen;

    for (auto root :
         roots) {

        std::error_code error;
        root =
            std::filesystem::absolute(
                root,
                error)
                .lexically_normal();

        if (error) {
            continue;
        }

        if (seen.insert(
                root.generic_string())
                .second) {

            unique.push_back(
                std::move(
                    root));
        }
    }

    return unique;
}

std::optional<DotnetHostDiscovery>
discover_below(
    const std::filesystem::path& root) {

    const auto fxr_root =
        root /
        "host" /
        "fxr";

    std::error_code error;

    if (!std::filesystem::
            is_directory(
                fxr_root,
                error) ||
        error) {

        return std::nullopt;
    }

    std::optional<
        DotnetHostDiscovery>
        best;

    VersionKey best_version;

    for (std::filesystem::
             directory_iterator it{
                 fxr_root,
                 error},
             end;
         !error &&
         it != end;
         it.increment(error)) {

        if (!it->is_directory(
                error) ||
            error) {
            error.clear();
            continue;
        }

        const auto version =
            it->path()
                .filename()
                .string();

        auto key =
            version_key(
                version);

        if (key.parts.empty()) {
            continue;
        }

        const auto library =
            it->path() /
            hostfxr_filename();

        if (!std::filesystem::
                is_regular_file(
                    library,
                    error) ||
            error) {

            error.clear();
            continue;
        }

        if (!best ||
            newer_version(
                key,
                best_version)) {

            DotnetHostDiscovery
                found;

            found.dotnet_root =
                root;
            found.hostfxr_path =
                library;
            found.version =
                version;

            best =
                std::move(
                    found);

            best_version =
                std::move(
                    key);
        }
    }

    return best;
}

#if defined(_WIN32)

using host_char_t = wchar_t;

std::wstring utf8_to_host(
    std::string_view value) {

    if (value.empty()) {
        return {};
    }

    const auto required =
        MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            nullptr,
            0);

    if (required <= 0) {
        return {};
    }

    std::wstring result(
        static_cast<std::size_t>(
            required),
        L'\0');

    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            value.data(),
            static_cast<int>(
                value.size()),
            result.data(),
            required) !=
        required) {

        return {};
    }

    return result;
}

std::wstring path_to_host(
    const std::filesystem::path& path) {

    return path.wstring();
}

void* open_library(
    const std::filesystem::path& path) {

    return reinterpret_cast<void*>(
        LoadLibraryW(
            path.c_str()));
}

void* load_symbol(
    void* library,
    const char* name) {

    return library
        ? reinterpret_cast<void*>(
            GetProcAddress(
                reinterpret_cast<
                    HMODULE>(
                        library),
                name))
        : nullptr;
}

void close_library(
    void* library) noexcept {

    if (library) {
        FreeLibrary(
            reinterpret_cast<HMODULE>(
                library));
    }
}

#define NENGINE_HOSTFXR_CALLTYPE __cdecl
#define NENGINE_CORECLR_CALLTYPE __stdcall

#else

using host_char_t = char;

std::string utf8_to_host(
    std::string_view value) {

    return std::string{
        value};
}

std::string path_to_host(
    const std::filesystem::path& path) {

    return path.string();
}

void* open_library(
    const std::filesystem::path& path) {

    return dlopen(
        path.c_str(),
        RTLD_LAZY |
            RTLD_LOCAL);
}

void* load_symbol(
    void* library,
    const char* name) {

    return library
        ? dlsym(
            library,
            name)
        : nullptr;
}

void close_library(
    void* library) noexcept {

    if (library) {
        dlclose(
            library);
    }
}

#define NENGINE_HOSTFXR_CALLTYPE
#define NENGINE_CORECLR_CALLTYPE

#endif

using hostfxr_handle =
    void*;

using hostfxr_initialize_for_runtime_config_fn =
    std::int32_t(
        NENGINE_HOSTFXR_CALLTYPE*)(
            const host_char_t*,
            const void*,
            hostfxr_handle*);

using hostfxr_get_runtime_delegate_fn =
    std::int32_t(
        NENGINE_HOSTFXR_CALLTYPE*)(
            hostfxr_handle,
            std::int32_t,
            void**);

using hostfxr_close_fn =
    std::int32_t(
        NENGINE_HOSTFXR_CALLTYPE*)(
            hostfxr_handle);

using load_assembly_and_get_function_pointer_fn =
    std::int32_t(
        NENGINE_CORECLR_CALLTYPE*)(
            const host_char_t*,
            const host_char_t*,
            const host_char_t*,
            const host_char_t*,
            void*,
            void**);

constexpr std::int32_t
    kLoadAssemblyAndGetFunctionPointer =
        5;

std::string result_code(
    std::int32_t value) {

    std::ostringstream text;
    text
        << "0x"
        << std::hex
        << std::uppercase
        << static_cast<std::uint32_t>(
            value);

    return text.str();
}

} // namespace

std::optional<DotnetHostDiscovery>
discover_dotnet_host(
    const std::filesystem::path& explicit_root,
    std::string* error) {

    if (!explicit_root.empty()) {
        if (auto found =
                discover_below(
                    explicit_root)) {

            return found;
        }

        set_error(
            error,
            "hostfxr was not found below explicit .NET root: " +
                explicit_root
                    .generic_string());

        return std::nullopt;
    }

    for (const auto& root :
         candidate_roots()) {

        if (auto found =
                discover_below(
                    root)) {

            return found;
        }
    }

    set_error(
        error,
        "hostfxr was not found in DOTNET_ROOT or conventional .NET installation roots");

    return std::nullopt;
}

bool write_dotnet_runtime_config(
    const std::filesystem::path& path,
    const DotnetRuntimeConfig& config,
    std::string* error) {

    if (path.empty() ||
        config.target_framework.empty() ||
        config.framework_name.empty() ||
        config.framework_version.empty() ||
        config.roll_forward.empty()) {

        set_error(
            error,
            "runtimeconfig requires path, target framework, framework version and roll-forward policy");
        return false;
    }

    std::error_code filesystem_error;

    if (!path.parent_path().empty()) {
        std::filesystem::
            create_directories(
                path.parent_path(),
                filesystem_error);

        if (filesystem_error) {
            set_error(
                error,
                "could not create runtimeconfig directory: " +
                    filesystem_error
                        .message());
            return false;
        }
    }

    std::ofstream output(
        path,
        std::ios::binary |
            std::ios::trunc);

    if (!output) {
        set_error(
            error,
            "could not open runtimeconfig for writing");
        return false;
    }

    output
        << "{\n"
        << "  \"runtimeOptions\": {\n"
        << "    \"tfm\": \""
        << json_escape(
            config.target_framework)
        << "\",\n"
        << "    \"framework\": {\n"
        << "      \"name\": \""
        << json_escape(
            config.framework_name)
        << "\",\n"
        << "      \"version\": \""
        << json_escape(
            config.framework_version)
        << "\"\n"
        << "    },\n"
        << "    \"rollForward\": \""
        << json_escape(
            config.roll_forward)
        << "\"\n"
        << "  }\n"
        << "}\n";

    if (!output.good()) {
        set_error(
            error,
            "failed writing runtimeconfig");
        return false;
    }

    return true;
}

DotnetHost::~DotnetHost() {
    shutdown();
}

DotnetHost::DotnetHost(
    DotnetHost&& other) noexcept
    : library_(
          std::exchange(
              other.library_,
              nullptr)),
      runtime_context_(
          std::exchange(
              other.runtime_context_,
              nullptr)),
      load_assembly_(
          std::exchange(
              other.load_assembly_,
              nullptr)),
      close_hostfxr_(
          std::exchange(
              other.close_hostfxr_,
              nullptr)),
      diagnostic_(
          std::move(
              other.diagnostic_)) {}

DotnetHost&
DotnetHost::operator=(
    DotnetHost&& other) noexcept {

    if (this == &other) {
        return *this;
    }

    shutdown();

    library_ =
        std::exchange(
            other.library_,
            nullptr);

    runtime_context_ =
        std::exchange(
            other.runtime_context_,
            nullptr);

    load_assembly_ =
        std::exchange(
            other.load_assembly_,
            nullptr);

    close_hostfxr_ =
        std::exchange(
            other.close_hostfxr_,
            nullptr);

    diagnostic_ =
        std::move(
            other.diagnostic_);

    return *this;
}

bool DotnetHost::initialize(
    const std::filesystem::path& hostfxr_path,
    const std::filesystem::path& runtime_config,
    std::string* error) {

    shutdown();

    if (hostfxr_path.empty() ||
        runtime_config.empty()) {

        diagnostic_ =
            "hostfxr path and runtimeconfig path are required";
        set_error(
            error,
            diagnostic_);
        return false;
    }

    library_ =
        open_library(
            hostfxr_path);

    if (!library_) {
        diagnostic_ =
            "could not load hostfxr library: " +
            hostfxr_path
                .generic_string();

        set_error(
            error,
            diagnostic_);
        return false;
    }

    const auto initialize =
        reinterpret_cast<
            hostfxr_initialize_for_runtime_config_fn>(
                load_symbol(
                    library_,
                    "hostfxr_initialize_for_runtime_config"));

    const auto get_delegate =
        reinterpret_cast<
            hostfxr_get_runtime_delegate_fn>(
                load_symbol(
                    library_,
                    "hostfxr_get_runtime_delegate"));

    const auto close =
        reinterpret_cast<
            hostfxr_close_fn>(
                load_symbol(
                    library_,
                    "hostfxr_close"));

    if (!initialize ||
        !get_delegate ||
        !close) {

        diagnostic_ =
            "hostfxr library is missing required hosting entry points";
        set_error(
            error,
            diagnostic_);
        shutdown();
        return false;
    }

    const auto runtime_config_text =
        path_to_host(
            runtime_config);

    if (runtime_config_text.empty()) {
        diagnostic_ =
            "runtimeconfig path could not be converted for hostfxr";
        set_error(
            error,
            diagnostic_);
        shutdown();
        return false;
    }

    hostfxr_handle context =
        nullptr;

    auto status =
        initialize(
            runtime_config_text.c_str(),
            nullptr,
            &context);

    if (status != 0 ||
        !context) {

        diagnostic_ =
            "hostfxr runtime initialization failed with " +
            result_code(
                status);

        set_error(
            error,
            diagnostic_);
        shutdown();
        return false;
    }

    void* load_assembly =
        nullptr;

    status =
        get_delegate(
            context,
            kLoadAssemblyAndGetFunctionPointer,
            &load_assembly);

    if (status != 0 ||
        !load_assembly) {

        close(
            context);

        diagnostic_ =
            "hostfxr load_assembly delegate lookup failed with " +
            result_code(
                status);

        set_error(
            error,
            diagnostic_);
        shutdown();
        return false;
    }

    runtime_context_ =
        context;
    load_assembly_ =
        load_assembly;
    close_hostfxr_ =
        reinterpret_cast<void*>(
            close);

    diagnostic_ =
        "hostfxr runtime initialized";

    return true;
}

void* DotnetHost::load_unmanaged_entry(
    const std::filesystem::path& assembly_path,
    std::string_view assembly_qualified_type,
    std::string_view method_name,
    std::string* error) const {

    if (!ready()) {
        set_error(
            error,
            "hostfxr runtime is not initialized");
        return nullptr;
    }

    if (assembly_path.empty() ||
        assembly_qualified_type.empty() ||
        method_name.empty()) {

        set_error(
            error,
            "managed entry lookup requires assembly, type and method");
        return nullptr;
    }

    const auto assembly =
        path_to_host(
            assembly_path);

    const auto type =
        utf8_to_host(
            assembly_qualified_type);

    const auto method =
        utf8_to_host(
            method_name);

    if (assembly.empty() ||
        type.empty() ||
        method.empty()) {

        set_error(
            error,
            "managed entry lookup string conversion failed");
        return nullptr;
    }

    const auto load_assembly =
        reinterpret_cast<
            load_assembly_and_get_function_pointer_fn>(
                load_assembly_);

    // hostfxr's sentinel -1 requests an [UnmanagedCallersOnly] method.
    const auto* unmanaged_callers_only =
        reinterpret_cast<
            const host_char_t*>(
                static_cast<
                    std::uintptr_t>(
                        ~std::uintptr_t{0}));

    void* function =
        nullptr;

    const auto status =
        load_assembly(
            assembly.c_str(),
            type.c_str(),
            method.c_str(),
            unmanaged_callers_only,
            nullptr,
            &function);

    if (status != 0 ||
        !function) {

        set_error(
            error,
            "managed unmanaged-entry lookup failed with " +
                result_code(
                    status));

        return nullptr;
    }

    return function;
}

void DotnetHost::shutdown() noexcept {

    if (runtime_context_ &&
        close_hostfxr_) {

        const auto close =
            reinterpret_cast<
                hostfxr_close_fn>(
                    close_hostfxr_);

        close(
            runtime_context_);
    }

    runtime_context_ = nullptr;
    load_assembly_ = nullptr;
    close_hostfxr_ = nullptr;

    close_library(
        library_);

    library_ = nullptr;
}

} // namespace nengine::scripting
