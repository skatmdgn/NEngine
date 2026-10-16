#include "nengine/scripting/managed_build.hpp"

#include <algorithm>
#include <climits>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/wait.h>
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

std::filesystem::path dotnet_name() {
#if defined(_WIN32)
    return "dotnet.exe";
#else
    return "dotnet";
#endif
}

bool executable_candidate(
    const std::filesystem::path& path) {

    std::error_code error;

    return
        !path.empty() &&
        std::filesystem::is_regular_file(
            path,
            error) &&
        !error;
}

std::optional<std::filesystem::path>
from_dotnet_root() {

    const char* root =
        std::getenv(
            "DOTNET_ROOT");

#if defined(_WIN32)
    if (!root || !*root) {
        root =
            std::getenv(
                "DOTNET_ROOT(x86)");
    }
#endif

    if (!root || !*root) {
        return std::nullopt;
    }

    const auto candidate =
        std::filesystem::path{
            root} /
        dotnet_name();

    return executable_candidate(
               candidate)
        ? std::optional<
            std::filesystem::path>{
                std::filesystem::absolute(
                    candidate)}
        : std::nullopt;
}

std::optional<std::filesystem::path>
from_path_environment() {

    const char* raw =
        std::getenv(
            "PATH");

    if (!raw || !*raw) {
        return std::nullopt;
    }

#if defined(_WIN32)
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif

    std::string_view path{
        raw};

    std::size_t begin = 0u;

    while (begin <= path.size()) {
        const auto end =
            path.find(
                separator,
                begin);

        const auto part =
            end ==
                std::string_view::npos
                ? path.substr(
                    begin)
                : path.substr(
                    begin,
                    end - begin);

        if (!part.empty()) {
            const auto candidate =
                std::filesystem::path{
                    std::string{part}} /
                dotnet_name();

            if (executable_candidate(
                    candidate)) {
                return
                    std::filesystem::absolute(
                        candidate);
            }
        }

        if (end ==
            std::string_view::npos) {
            break;
        }

        begin =
            end + 1u;
    }

    return std::nullopt;
}

#if defined(_WIN32)
// Encode argv according to CommandLineToArgvW/CRT rules, NOT cmd.exe rules.
// The actual executable is launched directly through CreateProcessW.
std::wstring windows_argv_quote(std::wstring_view argument) {
    std::wstring quoted = L"\"";
    std::size_t backslashes = 0;
    for (const wchar_t ch : argument) {
        if (ch == L'\\') {
            ++backslashes;
            continue;
        }
        if (ch == L'"') {
            quoted.append(backslashes * 2u + 1u, L'\\');
            quoted.push_back(ch);
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(ch);
    }
    quoted.append(backslashes * 2u, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

std::optional<std::wstring> utf8_to_wide(std::string_view argument) {
    if (argument.empty()) return std::wstring{};
    if (argument.size() > static_cast<std::size_t>(INT_MAX))
        return std::nullopt;
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
        argument.data(), static_cast<int>(argument.size()), nullptr, 0);
    if (count <= 0) return std::nullopt;
    std::wstring output(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
            argument.data(), static_cast<int>(argument.size()),
            output.data(), count) != count)
        return std::nullopt;
    return output;
}
#else
std::string shell_quote(
    std::string_view argument) {

    std::string quoted;
    quoted.push_back('\'');

    for (const char ch :
         argument) {

        if (ch == '\'') {
            quoted += "'\\''";
        } else {
            quoted.push_back(ch);
        }
    }

    quoted.push_back('\'');
    return quoted;
}
#endif

int normalize_exit_code(
    int raw) noexcept {

    if (raw < 0) {
        return raw;
    }

#if defined(_WIN32)
    return raw;
#else
    if (WIFEXITED(raw)) {
        return WEXITSTATUS(raw);
    }

    if (WIFSIGNALED(raw)) {
        return 128 +
            WTERMSIG(raw);
    }

    return raw;
#endif
}

} // namespace

std::optional<std::filesystem::path>
discover_dotnet_executable(
    const std::filesystem::path&
        explicit_path) {

    if (!explicit_path.empty()) {
        if (executable_candidate(
                explicit_path)) {
            return
                std::filesystem::absolute(
                    explicit_path);
        }

        if (std::filesystem::
                is_directory(
                    explicit_path)) {

            const auto candidate =
                explicit_path /
                dotnet_name();

            if (executable_candidate(
                    candidate)) {
                return
                    std::filesystem::absolute(
                        candidate);
            }
        }

        return std::nullopt;
    }

    if (auto root =
            from_dotnet_root()) {
        return root;
    }

    return
        from_path_environment();
}

std::optional<ManagedBuildPlan>
make_managed_build_plan(
    const ManagedBuildConfig& config,
    std::string* error) {

    if (config.project_path.empty() ||
        config.output_directory.empty() ||
        config.configuration.empty()) {

        set_error(
            error,
            "managed build requires project path, output directory and configuration");

        return std::nullopt;
    }

    std::error_code filesystem_error;

    if (!std::filesystem::
            is_regular_file(
                config.project_path,
                filesystem_error) ||
        filesystem_error) {

        set_error(
            error,
            "managed project file does not exist: " +
                config.project_path
                    .generic_string());

        return std::nullopt;
    }

    const auto executable =
        discover_dotnet_executable(
            config.dotnet_executable);

    if (!executable) {
        set_error(
            error,
            "dotnet executable was not found; install a .NET SDK or configure DOTNET_ROOT");

        return std::nullopt;
    }

    ManagedBuildPlan plan;
    plan.executable =
        *executable;

    const auto utf8_path = [](const std::filesystem::path& path) {
        const auto utf8 = path.u8string();
        return std::string{reinterpret_cast<const char*>(utf8.data()),
                           utf8.size()};
    };

    plan.arguments = {
        "build",
        utf8_path(config.project_path),
        "--configuration",
        config.configuration,
        "--output",
        utf8_path(config.output_directory),
        "--nologo"
    };

    if (!config.restore) {
        plan.arguments.push_back(
            "--no-restore");
    }

    const auto assembly_name =
        config.project_path
            .stem()
            .string();

    plan.assembly_path =
        config.output_directory /
        (assembly_name + ".dll");

    plan.pdb_path =
        config.output_directory /
        (assembly_name + ".pdb");

    return plan;
}

ManagedBuildResult run_managed_build(
    const ManagedBuildConfig& config) {

    ManagedBuildResult result;
    std::string error;

    const auto plan =
        make_managed_build_plan(
            config,
            &error);

    if (!plan) {
        result.message =
            std::move(error);
        return result;
    }

    result.plan =
        *plan;

    std::error_code filesystem_error;

    std::filesystem::create_directories(
        config.output_directory,
        filesystem_error);

    if (filesystem_error) {
        result.message =
            "could not create managed build output directory: " +
            config.output_directory
                .generic_string();

        return result;
    }

#if defined(_WIN32)
    std::wstring command = windows_argv_quote(result.plan.executable.wstring());
    for (const auto& argument : result.plan.arguments) {
        const auto wide = utf8_to_wide(argument);
        if (!wide) {
            result.message = "managed build argument is not valid UTF-8";
            return result;
        }
        command.push_back(L' ');
        command += windows_argv_quote(*wide);
    }
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(result.plan.executable.c_str(), command.data(),
            nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
            nullptr, nullptr, &startup, &process)) {
        result.message = "could not launch dotnet build with CreateProcessW";
        return result;
    }
    const DWORD wait_status = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exit_code = 0;
    const bool finished = wait_status == WAIT_OBJECT_0 &&
        GetExitCodeProcess(process.hProcess, &exit_code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    result.exit_code = finished ? static_cast<int>(exit_code) : -1;
#else
    std::ostringstream command;
    command << shell_quote(result.plan.executable.string());
    for (const auto& argument : result.plan.arguments)
        command << ' ' << shell_quote(argument);
    result.exit_code = normalize_exit_code(std::system(command.str().c_str()));
#endif

    if (result.exit_code != 0) {
        result.message =
            "dotnet build failed with exit code " +
            std::to_string(
                result.exit_code);

        return result;
    }

    if (!std::filesystem::
            is_regular_file(
                result.plan.assembly_path,
                filesystem_error) ||
        filesystem_error) {

        result.message =
            "dotnet build succeeded but managed assembly was not produced: " +
            result.plan.assembly_path
                .generic_string();

        return result;
    }

    result.success = true;
    result.message =
        "managed scripts built: " +
        result.plan.assembly_path
            .generic_string();

    return result;
}

} // namespace nengine::scripting
