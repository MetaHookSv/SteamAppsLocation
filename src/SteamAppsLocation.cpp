#include "WindowsServices.h"
#include <iostream>
#include <vector>

int main(int argc, const char** argv) {
    using namespace steam_location;
    try {
        std::vector<std::string_view> arguments;
        for (int index = 1; index < argc; ++index) arguments.emplace_back(argv[index]);
        const Services services{
            [](auto& errors) { return ReadSteamPath(errors); },
            [](const auto& path, auto& errors) { return RepairSteamClient(path, errors); },
            [](AppId appId, auto& errors) {
                return LookupSteamApi(appId, ExecutableDirectory() / L"steam_api.dll", errors);
            }};
        return Run(arguments, services, std::cout, std::cerr);
    } catch (const std::exception& error) {
        std::cerr << "[Error] " << error.what() << '\n';
        return SteamApiUnavailable;
    }
}
