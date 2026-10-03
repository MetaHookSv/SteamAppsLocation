#include "WindowsServices.h"
#include "TestSupport.h"
#include <array>
#include <sstream>

using namespace steam_location;
using namespace test_support;

namespace {

struct RegistryFixture {
    std::wstring name;
    HKEY root = nullptr;

    RegistryFixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        name = L"Software\\SteamAppsLocationTests\\" + std::to_wstring(GetCurrentProcessId()) +
            L"-" + std::to_wstring(stamp);
        const auto result = RegCreateKeyExW(HKEY_CURRENT_USER, name.c_str(), 0, nullptr,
            REG_OPTION_VOLATILE, KEY_ALL_ACCESS, nullptr, &root, nullptr);
        ExpectEqual(LSTATUS{ERROR_SUCCESS}, result, "Create isolated test registry key");
    }

    ~RegistryFixture() {
        RegCloseKey(root);
        RegDeleteTreeW(HKEY_CURRENT_USER, name.c_str());
    }

    void Set(const wchar_t* keyPath, const wchar_t* valueName, const std::wstring& value) {
        HKEY key = nullptr;
        ExpectEqual(LSTATUS{ERROR_SUCCESS}, RegCreateKeyExW(root, keyPath, 0, nullptr,
            REG_OPTION_VOLATILE, KEY_ALL_ACCESS, nullptr, &key, nullptr), "Create test Steam key");
        const auto result = RegSetValueExW(key, valueName, 0, REG_SZ,
            reinterpret_cast<const BYTE*>(value.c_str()), static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
        RegCloseKey(key);
        ExpectEqual(LSTATUS{ERROR_SUCCESS}, result, "Set test registry value");
    }
};

SteamApiResult Lookup(const fs::path& dll, const fs::path& installDir,
    const fs::path& marker, const wchar_t* mode, std::ostream& errors) {
    SetEnvironmentVariableW(L"STEAM_TEST_MODE", mode);
    SetEnvironmentVariableW(L"STEAM_TEST_EXPECTED_APPID", L"70");
    SetEnvironmentVariableW(L"STEAM_TEST_INSTALL_DIR", installDir.c_str());
    SetEnvironmentVariableW(L"STEAM_TEST_SHUTDOWN_FILE", marker.c_str());
    // Deliberately start with conflicting inherited values.
    SetEnvironmentVariableW(L"SteamAppId", L"10");
    SetEnvironmentVariableW(L"SteamGameId", L"10");
    return LookupSteamApi(70, dll, errors);
}

}

int main(int argc, const char** argv) {
    if (argc != 3) return 1;
    const auto dll = PathFromUtf8(argv[1]);
    const auto incomplete = PathFromUtf8(argv[2]);
    return RunTests({
        {"Registry SteamPath is Unicode and optional", [] {
            RegistryFixture registry;
            std::ostringstream errors;
            ExpectEqual(false, ReadSteamPath(errors, registry.root).has_value(), "Missing registry value");
            registry.Set(L"Software\\Valve\\Steam", L"SteamPath", L"D:\\Steam 游戏");
            ExpectEqual(fs::path{L"D:\\Steam 游戏"}, ReadSteamPath(errors, registry.root).value_or(fs::path{}), "Read Unicode registry string");
            registry.Set(L"Software\\Valve\\Steam", L"SteamPath", L"");
            ExpectEqual(false, ReadSteamPath(errors, registry.root).has_value(), "Empty registry value");
        }},
        {"Steam client repair uses isolated registry", [] {
            Fixture fixture;
            RegistryFixture registry;
            const auto expected = fixture.root / "steamclient.dll";
            const auto other = fixture.root / "other.dll";
            Write(expected, "fixture");
            Write(other, "fixture");
            registry.Set(L"Software\\Valve\\Steam\\ActiveProcess", L"SteamClientDll", other.wstring());
            std::ostringstream errors;
            ExpectEqual(Success, RepairSteamClient(fixture.root, errors, registry.root), "Repair mismatched path");
            std::array<wchar_t, 32768> repaired{};
            DWORD repairedBytes = static_cast<DWORD>(repaired.size() * sizeof(wchar_t));
            ExpectEqual(LSTATUS{ERROR_SUCCESS}, RegGetValueW(registry.root,
                L"Software\\Valve\\Steam\\ActiveProcess", L"SteamClientDll", RRF_RT_REG_SZ,
                nullptr, repaired.data(), &repairedBytes), "Read repaired value");
            ExpectEqual(fs::canonical(expected).wstring(), std::wstring(repaired.data()), "Registry contains expected DLL path");
            // A second repair must see the repaired value and need no write.
            ExpectEqual(Success, RepairSteamClient(fixture.root, errors, registry.root), "Already matching path");
            registry.Set(L"Software\\Valve\\Steam\\ActiveProcess", L"SteamClientDll", (fixture.root / "absent.dll").wstring());
            ExpectEqual(ActiveSteamClientInvalid, RepairSteamClient(fixture.root, errors, registry.root), "Unresolvable active path");
            ExpectEqual(ExpectedSteamClientInvalid, RepairSteamClient(fixture.root / "missing", errors, registry.root), "Unresolvable expected path");
        }},
        {"Missing ActiveProcess returns original stage code", [] {
            RegistryFixture registry;
            std::ostringstream errors;
            ExpectEqual(SteamClientUnavailable, RepairSteamClient(L"D:\\Steam", errors, registry.root), "Missing ActiveProcess");
        }},
        {"Missing DLL and export return code 8", [&] {
            Fixture fixture;
            std::ostringstream errors;
            ExpectEqual(SteamApiUnavailable, LookupSteamApi(70, fixture.root / "absent.dll", errors).failureCode, "Missing DLL");
            const auto marker = fixture.root / "shutdown.txt";
            ExpectEqual(SteamApiUnavailable, Lookup(incomplete, fixture.root, marker, L"success", errors).failureCode, "Missing export");
            ExpectEqual(false, fs::exists(marker), "Do not initialize incomplete SDK");
        }},
        {"Failed initialization does not call shutdown", [&] {
            Fixture fixture;
            std::ostringstream errors;
            const auto marker = fixture.root / "shutdown.txt";
            const auto result = Lookup(dll, fixture.root, marker, L"init-fails", errors);
            ExpectEqual(false, result.installPath.has_value(), "No init failure path");
            ExpectEqual(SteamApiUnavailable, result.failureCode, "Init failure code");
            ExpectEqual(false, fs::exists(marker), "No shutdown after failed init");
            ExpectEqual(true, errors.str().find("Simulated initialization failure") != std::string::npos, "SDK error detail");
        }},
        {"Null interface and failed query call shutdown", [&] {
            Fixture fixture;
            for (auto mode : {L"null-interface", L"query-fails"}) {
                std::ostringstream errors;
                const auto marker = fixture.root / (std::wstring(mode) + L".txt");
                const auto result = Lookup(dll, fixture.root, marker, mode, errors);
                ExpectEqual(false, result.installPath.has_value(), "No query failure path");
                ExpectEqual(InstallDirectoryUnavailable, result.failureCode, "Query failure code");
                ExpectEqual(true, fs::exists(marker), "Shutdown after initialized failure");
            }
        }},
        {"Nonexistent API directory calls shutdown", [&] {
            Fixture fixture;
            std::ostringstream errors;
            const auto marker = fixture.root / "shutdown.txt";
            const auto result = Lookup(dll, fixture.root / "absent", marker, L"success", errors);
            ExpectEqual(false, result.installPath.has_value(), "Reject nonexistent API directory");
            ExpectEqual(InstallDirectoryUnavailable, result.failureCode, "Unavailable directory code");
            ExpectEqual(true, fs::exists(marker), "Shutdown after invalid directory");
        }},
        {"API returns UTF-8 directory and releases SDK", [&] {
            Fixture fixture;
            const auto expected = fixture.root / L"Steam 游戏";
            fs::create_directory(expected);
            std::ostringstream errors;
            const auto marker = fixture.root / "shutdown.txt";
            const auto result = Lookup(dll, expected, marker, L"success", errors);
            ExpectEqual(expected, result.installPath.value_or(fs::path{}), "UTF-8 SDK path");
            ExpectEqual(true, fs::exists(marker), "Shutdown after success");
            const auto moved = fixture.root / "released.dll";
            fs::copy_file(dll, moved);
            Lookup(moved, expected, marker, L"success", errors);
            ExpectEqual(true, fs::remove(moved), "Release DLL handle after query");
        }},
    });
}
