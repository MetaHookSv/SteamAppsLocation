#include "WindowsServices.h"
#include <steam/steam_api_flat.h>
#include <array>
#include <bit>
#include <cstdio>
#include <io.h>
#include <system_error>

namespace steam_location {
namespace fs = std::filesystem;
namespace {

// Redirect both CRT and Win32 output before loading the SDK's own CRT.
// The command's stdout must contain only the resulting install directory.
class SdkOutputRedirect {
public:
    SdkOutputRedirect() {
        std::fflush(stdout);
        saved_ = _dup(_fileno(stdout));
        if (saved_ < 0) return;
        if (_dup2(_fileno(stderr), _fileno(stdout)) != 0) {
            _close(saved_);
            saved_ = -1;
            return;
        }
        redirected_ = SetStdHandle(STD_OUTPUT_HANDLE,
            reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)))) != FALSE;
        if (!redirected_) Restore();
    }

    ~SdkOutputRedirect() { Restore(); }
    bool Ready() const { return redirected_; }

private:
    void Restore() {
        if (saved_ < 0) return;
        std::fflush(stdout);
        _dup2(saved_, _fileno(stdout));
        SetStdHandle(STD_OUTPUT_HANDLE, reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout))));
        _close(saved_);
        saved_ = -1;
        redirected_ = false;
    }

    int saved_ = -1;
    bool redirected_ = false;
};

struct Library {
    HMODULE handle;
    ~Library() { if (handle) FreeLibrary(handle); }
};

template<typename Function>
Function Resolve(HMODULE module, const char* name) {
    return std::bit_cast<Function>(GetProcAddress(module, name));
}

struct SteamSession {
    decltype(&SteamAPI_Shutdown) shutdown;
    ~SteamSession() { shutdown(); }
};

}

SteamApiResult LookupSteamApi(AppId appId, const fs::path& dllPath, std::ostream& errors) {
    const auto appIdText = std::to_wstring(appId);
    if (!SetEnvironmentVariableW(L"SteamAppId", appIdText.c_str()) ||
        !SetEnvironmentVariableW(L"SteamGameId", appIdText.c_str())) {
        errors << "[Error] Cannot set process Steam AppId (Win32 " << GetLastError() << ").\n";
        return {};
    }

    SdkOutputRedirect redirect;
    if (!redirect.Ready()) {
        errors << "[Error] Cannot redirect Steam SDK diagnostics to stderr.\n";
        return {};
    }
    Library library{LoadLibraryW(dllPath.c_str())};
    if (!library.handle) {
        errors << "[Error] Cannot load steam_api.dll (Win32 " << GetLastError() << ").\n";
        return {};
    }
    const auto initialize = Resolve<decltype(&SteamInternal_SteamAPI_Init)>(library.handle, "SteamInternal_SteamAPI_Init");
    const auto shutdown = Resolve<decltype(&SteamAPI_Shutdown)>(library.handle, "SteamAPI_Shutdown");
    const auto apps = Resolve<decltype(&SteamAPI_SteamApps_v008)>(library.handle, "SteamAPI_SteamApps_v008");
    const auto query = Resolve<decltype(&SteamAPI_ISteamApps_GetAppInstallDir)>(library.handle, "SteamAPI_ISteamApps_GetAppInstallDir");
    if (!initialize || !shutdown || !apps || !query) {
        errors << "[Error] steam_api.dll is missing a required export.\n";
        return {};
    }
    SteamErrMsg detail{};
    // Flat SDK accessors check their own interface versions; see steam_api.h.
    if (initialize(nullptr, &detail) != k_ESteamAPIInitResult_OK) {
        errors << "[Error] Failed to initialize Steam API: " << detail << '\n';
        return {};
    }
    SteamSession session{shutdown};
    auto* interface = apps();
    if (!interface) {
        errors << "[Error] SteamApps interface is unavailable.\n";
        return {std::nullopt, InstallDirectoryUnavailable};
    }

    constexpr std::size_t installDirectoryCapacity = 32768;
    std::array<char, installDirectoryCapacity> directory{};
    const auto length = query(interface, appId, directory.data(), static_cast<uint32>(directory.size()));
    if (length == 0 || length >= directory.size() || directory[length] != '\0') {
        errors << "[Error] Failed to GetAppInstallDir.\n";
        return {std::nullopt, InstallDirectoryUnavailable};
    }
    try {
        if (auto path = ExistingInstallPath(PathFromUtf8(std::string_view(directory.data(), length))))
            return {std::move(path), Success};
    } catch (const fs::filesystem_error&) {
        // Treat invalid UTF-8 directory text as an unsuccessful API lookup.
    }
    errors << "[Error] Steam API returned an unavailable install directory.\n";
    return {std::nullopt, InstallDirectoryUnavailable};
}

fs::path ExecutableDirectory() {
    constexpr DWORD modulePathCapacity = 32768;
    std::array<wchar_t, modulePathCapacity> path{};
    const DWORD length = GetModuleFileNameW(nullptr, path.data(), modulePathCapacity);
    if (length == 0 || length >= modulePathCapacity)
        throw std::system_error(length == 0 ? GetLastError() : ERROR_INSUFFICIENT_BUFFER,
            std::system_category(), "Cannot locate executable directory");
    return fs::path(std::wstring_view(path.data(), length)).parent_path();
}

}
