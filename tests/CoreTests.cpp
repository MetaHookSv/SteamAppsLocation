#include "Application.h"
#include "Vdf.h"
#include "TestSupport.h"
#include <array>
#include <sstream>

using namespace steam_location;
using namespace test_support;

int main() {
    return RunTests({
        {"AppId validation", [] {
            ExpectEqual(AppId{70}, ParseAppId("70").value_or(0), "Valid AppId");
            ExpectEqual(AppId{4294967295u}, ParseAppId("4294967295").value_or(0), "Maximum AppId");
            for (auto invalid : {"", "0", "-1", "+70", "70x", " 70", "70 ", "4294967296"})
                ExpectEqual(false, ParseAppId(invalid).has_value(), "Reject invalid AppId");
        }},
        {"VDF nesting, comments, BOM and escapes", [] {
            auto vdf = ParseVdf("\xEF\xBB\xBF// header\n\"LibraryFolders\" { 0 { \"path\" \"D:\\\\Steam Library\" nested { value \"a\\\"b\" } } }");
            ExpectEqual(true, vdf.has_value(), "Parse VDF");
            const auto* libraries = vdf->Find("libraryfolders");
            ExpectEqual(true, libraries != nullptr, "Case insensitive key lookup");
            const auto* entry = libraries->Find("0");
            ExpectEqual(std::string("D:\\Steam Library"), *entry->Find("path")->value, "Decode escaped backslashes");
            ExpectEqual(std::string("a\"b"), *entry->Find("nested")->Find("value")->value, "Decode escaped quotes");
        }},
        {"Malformed VDF is rejected", [] {
            for (auto invalid : {"\"AppState\" {", "\"key\"", "\"root\" { \"key\" }", "}", "\"unterminated", "\"root\" { } }"})
                ExpectEqual(false, ParseVdf(invalid).has_value(), "Reject malformed VDF");
        }},
        {"Primary library without libraryfolders", [] {
            Fixture fixture;
            const auto expected = fixture.Install(fixture.root, "Half-Life");
            ExpectEqual(expected, FindGameInstallPath(70, fixture.root).value_or(fs::path{}), "Find primary game");
        }},
        {"Primary library takes precedence", [] {
            Fixture fixture;
            const auto secondary = fixture.root / "other";
            const auto expected = fixture.Install(fixture.root, "Primary");
            fixture.Install(secondary, "Secondary");
            Write(fixture.root / "steamapps/libraryfolders.vdf",
                "\"libraryfolders\" { 0 { path " + Quote(secondary) + " apps { 70 1 } } }");
            ExpectEqual(expected, FindGameInstallPath(70, fixture.root).value_or(fs::path{}), "Prefer primary library");
        }},
        {"Additional Unicode library with escaped path", [] {
            Fixture fixture;
            const auto secondary = fixture.root / L"Steam 游戏库";
            const auto expected = fixture.Install(secondary, "Half-Life 中文");
            Write(fixture.root / "steamapps/libraryfolders.vdf",
                "\"libraryfolders\" { 0 { path " + Quote(fixture.root) + " } 1 { path " + Quote(secondary) + " } }");
            ExpectEqual(expected, FindGameInstallPath(70, fixture.root).value_or(fs::path{}), "Find Unicode path");
        }},
        {"Broken libraryfolders still allows primary library", [] {
            Fixture fixture;
            const auto expected = fixture.Install(fixture.root, "Half-Life");
            Write(fixture.root / "steamapps/libraryfolders.vdf", "\"libraryfolders\" { bad");
            ExpectEqual(expected, FindGameInstallPath(70, fixture.root).value_or(fs::path{}), "Primary survives parse failure");
        }},
        {"Bad manifests do not prevent later libraries", [] {
            Fixture fixture;
            const auto broken = fixture.root / "broken";
            const auto missing = fixture.root / "missing";
            const auto good = fixture.root / "good";
            Write(fixture.root / "steamapps/appmanifest_70.acf", "AppState {");
            fs::create_directories(fixture.root / "steamapps/common");
            Write(broken / "steamapps/appmanifest_70.acf", "AppState { name Game }");
            fs::create_directories(broken / "steamapps/common");
            Write(missing / "steamapps/appmanifest_70.acf", "AppState { installdir Gone }");
            fs::create_directories(missing / "steamapps/common");
            const auto expected = fixture.Install(good, "Half-Life");
            Write(fixture.root / "steamapps/libraryfolders.vdf",
                "libraryfolders { 1 { path " + Quote(broken) + " } 2 { path " + Quote(missing) +
                " } 3 { path " + Quote(good) + " } }");
            ExpectEqual(expected, FindGameInstallPath(70, fixture.root).value_or(fs::path{}), "Continue past bad manifests");
        }},
        {"Not installed and wrong AppId return no path", [] {
            Fixture fixture;
            fixture.Install(fixture.root, "Half-Life");
            ExpectEqual(false, FindGameInstallPath(10, fixture.root).has_value(), "Do not return another AppId");
            ExpectEqual(false, FindGameInstallPath(70, fixture.root / "absent").has_value(), "Missing library");
            Write(fixture.root / "steamapps/appmanifest_70.acf", "AppState { installdir \"\" }");
            ExpectEqual(false, FindGameInstallPath(70, fixture.root).has_value(), "Empty installdir");
        }},
        {"Unreadable manifest does not block another library", [] {
            Fixture fixture;
            fixture.Install(fixture.root, "Locked game");
            const auto secondary = fixture.root / "other";
            const auto expected = fixture.Install(secondary, "Available game");
            Write(fixture.root / "steamapps/libraryfolders.vdf",
                "libraryfolders { 1 { path " + Quote(secondary) + " } }");
            const auto manifest = fixture.root / "steamapps/appmanifest_70.acf";
            const auto file = CreateFileW(manifest.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            ExpectEqual(false, file == INVALID_HANDLE_VALUE, "Lock fixture manifest");
            const auto actual = FindGameInstallPath(70, fixture.root);
            CloseHandle(file);
            ExpectEqual(expected, actual.value_or(fs::path{}), "Continue past unreadable manifest");
        }},
        {"API result takes precedence and stdout is one UTF-8 path", [] {
            Fixture fixture;
            fixture.Install(fixture.root, "Manifest game");
            const auto expected = fixture.root / L"API 游戏";
            fs::create_directories(expected);
            Services services{
                [&](auto&) { return std::optional{fixture.root}; },
                [](auto&, auto&) { return Success; },
                [&](AppId appId, auto&) {
                    ExpectEqual(AppId{70}, appId, "Forward AppId");
                    return SteamApiResult{expected};
                }};
            std::ostringstream output, errors;
            const std::array<std::string_view, 1> arguments{"70"};
            ExpectEqual(Success, Run(arguments, services, output, errors), "API success");
            ExpectEqual(Utf8(expected) + '\n', output.str(), "Only UTF-8 path on stdout");
        }},
        {"Repair and API failures still allow manifest success", [] {
            Fixture fixture;
            const auto expected = fixture.Install(fixture.root, "Half-Life");
            for (int repairCode : {SteamClientUnavailable, SteamClientRepairFailed, ExpectedSteamClientInvalid, ActiveSteamClientInvalid}) {
                Services services{
                    [&](auto&) { return std::optional{fixture.root}; },
                    [=](auto&, auto& errors) { errors << "repair failed\n"; return repairCode; },
                    [](AppId, auto& errors) { errors << "API failed\n"; return SteamApiResult{}; }};
                std::ostringstream output, errors;
                const std::array<std::string_view, 1> arguments{"70"};
                ExpectEqual(Success, Run(arguments, services, output, errors), "Fallback overrides prior failures");
                ExpectEqual(Utf8(expected) + '\n', output.str(), "Fallback stdout");
                ExpectEqual(true, !errors.str().empty(), "Diagnostics use stderr");
            }
        }},
        {"Invalid API directory falls back", [] {
            Fixture fixture;
            const auto expected = fixture.Install(fixture.root, "Half-Life");
            Services services{
                [&](auto&) { return std::optional{fixture.root}; },
                [](auto&, auto&) { return Success; },
                [&](AppId, auto&) { return SteamApiResult{fixture.root / "absent"}; }};
            std::ostringstream output, errors;
            const std::array<std::string_view, 1> arguments{"70"};
            ExpectEqual(Success, Run(arguments, services, output, errors), "Invalid API result falls back");
            ExpectEqual(Utf8(expected) + '\n', output.str(), "Fallback result");
        }},
        {"Total failure preserves earliest stage code", [] {
            Fixture fixture;
            for (int repairCode : {Success, SteamClientUnavailable, SteamClientRepairFailed, ExpectedSteamClientInvalid, ActiveSteamClientInvalid}) {
                Services services{
                    [&](auto&) { return std::optional{fixture.root}; },
                    [=](auto&, auto&) { return repairCode; },
                    [](AppId, auto&) { return SteamApiResult{std::nullopt, InstallDirectoryUnavailable}; }};
                std::ostringstream output, errors;
                const std::array<std::string_view, 1> arguments{"70"};
                const int expected = repairCode == Success ? InstallDirectoryUnavailable : repairCode;
                ExpectEqual(expected, Run(arguments, services, output, errors), "Preserve first failure");
                ExpectEqual(std::string{}, output.str(), "No stdout on failure");
            }
        }},
        {"Missing Steam registry path permits API success", [] {
            Fixture fixture;
            Services services{
                [](auto&) -> std::optional<fs::path> { return std::nullopt; },
                [](auto&, auto&) -> int { throw std::runtime_error("Repair must not run without SteamPath"); },
                [&](AppId, auto&) { return SteamApiResult{fixture.root}; }};
            std::ostringstream output, errors;
            const std::array<std::string_view, 1> arguments{"70"};
            ExpectEqual(Success, Run(arguments, services, output, errors), "API can work without registry path");
            services.lookupSteamApi = [](AppId, auto&) { return SteamApiResult{}; };
            output.str("");
            ExpectEqual(SteamPathUnavailable, Run(arguments, services, output, errors), "Root error retained on total failure");
            ExpectEqual(std::string{}, output.str(), "No failed result on stdout");
        }},
        {"Invalid arguments do not access Steam or registry", [] {
            Services services;
            std::ostringstream output, errors;
            const std::array<std::string_view, 0> missing{};
            const std::array<std::string_view, 1> invalid{"70x"};
            const std::array<std::string_view, 2> extra{"70", "10"};
            ExpectEqual(InvalidArguments, Run(missing, services, output, errors), "Missing argument");
            ExpectEqual(InvalidArguments, Run(invalid, services, output, errors), "Invalid argument");
            ExpectEqual(InvalidArguments, Run(extra, services, output, errors), "Extra argument");
            ExpectEqual(std::string{}, output.str(), "No stdout on invalid input");
        }},
    });
}
