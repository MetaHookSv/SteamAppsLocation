#include <Windows.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::wstring Environment(const wchar_t* name) {
    const DWORD required = GetEnvironmentVariableW(name, nullptr, 0);
    if (required == 0) return {};
    std::wstring value(required, L'\0');
    const DWORD length = GetEnvironmentVariableW(name, value.data(), required);
    value.resize(length < required ? length : 0);
    return value;
}

}

extern "C" __declspec(dllexport) int __cdecl SteamInternal_SteamAPI_Init(const char*, char* error) {
    std::printf("SDK diagnostic on stdout\n");
    if (Environment(L"STEAM_TEST_MODE") == L"init-fails") {
        std::strcpy(error, "Simulated initialization failure");
        return 1;
    }
    const auto expected = Environment(L"STEAM_TEST_EXPECTED_APPID");
    if (Environment(L"SteamAppId") != expected || Environment(L"SteamGameId") != expected) {
        std::strcpy(error, "Incorrect process AppId");
        return 1;
    }
    return 0;
}

extern "C" __declspec(dllexport) void* __cdecl SteamAPI_SteamApps_v008() {
    static int interfaceToken;
    if (Environment(L"STEAM_TEST_MODE") == L"null-interface") return nullptr;
    return &interfaceToken;
}

#ifndef STEAM_TEST_MISSING_QUERY_EXPORT
extern "C" __declspec(dllexport) std::uint32_t __cdecl SteamAPI_ISteamApps_GetAppInstallDir(
    void*, std::uint32_t appId, char* buffer, std::uint32_t capacity) {
    if (Environment(L"STEAM_TEST_MODE") == L"query-fails" ||
        std::to_wstring(appId) != Environment(L"STEAM_TEST_EXPECTED_APPID")) return 0;
    const auto directory = Environment(L"STEAM_TEST_INSTALL_DIR");
    const int required = WideCharToMultiByte(CP_UTF8, 0, directory.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (required <= 0 || static_cast<std::uint32_t>(required) > capacity) return 0;
    WideCharToMultiByte(CP_UTF8, 0, directory.c_str(), -1, buffer, static_cast<int>(capacity), nullptr, nullptr);
    return static_cast<std::uint32_t>(required - 1);
}
#endif

extern "C" __declspec(dllexport) void __cdecl SteamAPI_Shutdown() {
    std::printf("SDK shutdown diagnostic on stdout\n");
    const auto marker = Environment(L"STEAM_TEST_SHUTDOWN_FILE");
    if (!marker.empty()) {
        std::ofstream file{std::filesystem::path(marker)};
        file << "shutdown\n";
    }
}
