#include "WindowsServices.h"

namespace steam_location {
namespace fs = std::filesystem;
namespace {

constexpr wchar_t steamKey[] = L"Software\\Valve\\Steam";
constexpr wchar_t activeProcessKey[] = L"Software\\Valve\\Steam\\ActiveProcess";

struct RegistryKey {
    HKEY handle = nullptr;
    ~RegistryKey() { if (handle) RegCloseKey(handle); }
};

std::optional<std::wstring> ReadRegistryValue(HKEY root, const wchar_t* keyPath,
    const wchar_t* valueName, std::ostream& errors) {
    RegistryKey key;
    auto status = RegOpenKeyExW(root, keyPath, 0, KEY_READ, &key.handle);
    if (status != ERROR_SUCCESS) {
        errors << "[Error] Cannot open Steam registry key (Win32 " << status << ").\n";
        return std::nullopt;
    }
    DWORD byteCount = 0;
    DWORD type = 0;
    status = RegQueryValueExW(key.handle, valueName, nullptr, &type, nullptr, &byteCount);
    if (status != ERROR_SUCCESS || type != REG_SZ || byteCount % sizeof(wchar_t) != 0) {
        errors << "[Error] Cannot read Steam registry string (Win32 " << status << ").\n";
        return std::nullopt;
    }
    if (byteCount == 0) return std::wstring{};
    std::wstring value(byteCount / sizeof(wchar_t), L'\0');
    status = RegQueryValueExW(key.handle, valueName, nullptr, &type,
        reinterpret_cast<BYTE*>(value.data()), &byteCount);
    if (status != ERROR_SUCCESS || type != REG_SZ || byteCount % sizeof(wchar_t) != 0) {
        errors << "[Error] Cannot read Steam registry string (Win32 " << status << ").\n";
        return std::nullopt;
    }
    value.resize(byteCount / sizeof(wchar_t));
    while (!value.empty() && value.back() == L'\0') value.pop_back();
    return value;
}

}

std::optional<fs::path> ReadSteamPath(std::ostream& errors, HKEY registryRoot) {
    const auto value = ReadRegistryValue(registryRoot, steamKey, L"SteamPath", errors);
    if (!value || value->empty()) return std::nullopt;
    return fs::path(*value);
}

int RepairSteamClient(const fs::path& steamPath, std::ostream& errors, HKEY registryRoot) {
    const auto activePath = ReadRegistryValue(registryRoot, activeProcessKey, L"SteamClientDll", errors);
    if (!activePath || activePath->empty()) return SteamClientUnavailable;

    std::error_code error;
    const auto expected = fs::canonical(steamPath / L"steamclient.dll", error);
    if (error) {
        errors << "[Error] Cannot resolve expected steamclient.dll: " << error.message() << ".\n";
        return ExpectedSteamClientInvalid;
    }
    const auto active = fs::canonical(fs::path(*activePath), error);
    if (error) {
        errors << "[Error] Cannot resolve ActiveProcess SteamClientDll: " << error.message() << ".\n";
        return ActiveSteamClientInvalid;
    }
    const bool same = fs::equivalent(expected, active, error);
    if (error) {
        errors << "[Error] Cannot compare steamclient.dll paths: " << error.message() << ".\n";
        return ActiveSteamClientInvalid;
    }
    if (same) return Success;

    RegistryKey key;
    auto status = RegOpenKeyExW(registryRoot, activeProcessKey, 0, KEY_SET_VALUE, &key.handle);
    if (status == ERROR_SUCCESS) {
        const auto value = expected.wstring();
        status = RegSetValueExW(key.handle, L"SteamClientDll", 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    }
    if (status != ERROR_SUCCESS) {
        errors << "[Error] Cannot repair ActiveProcess SteamClientDll (Win32 " << status
            << "). Restarting Steam may resolve this.\n";
        return SteamClientRepairFailed;
    }
    return Success;
}

}
