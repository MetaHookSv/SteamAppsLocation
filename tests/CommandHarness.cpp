#include "WindowsServices.h"
#include <array>
#include <iostream>

// Exercise the production command and SDK loader without accessing real Steam registry keys.
int wmain(int argc, const wchar_t** argv) {
    using namespace steam_location;
    if (argc != 4) return InvalidArguments;
    const std::wstring wideAppId = argv[1];
    std::string appId;
    for (const wchar_t ch : wideAppId) {
        if (ch > 0x7f) return InvalidArguments;
        appId += static_cast<char>(ch);
    }
    const std::filesystem::path steamPath = argv[2];
    const std::filesystem::path dllPath = argv[3];
    const Services services{
        [&](auto&) { return std::optional{steamPath}; },
        [](const auto&, auto& errors) { errors << "Simulated repair failure\n"; return SteamClientRepairFailed; },
        [&](AppId id, auto& errors) { return LookupSteamApi(id, dllPath, errors); }};
    const std::array<std::string_view, 1> arguments{appId};
    return Run(arguments, services, std::cout, std::cerr);
}
