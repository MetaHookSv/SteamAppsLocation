#pragma once

#include "GameInstallPath.h"
#include <functional>
#include <ostream>
#include <span>
#include <string_view>

namespace steam_location {

enum ExitCode {
    Success = 0,
    InvalidArguments = 1,
    SteamPathUnavailable = 4,
    SteamClientUnavailable = 5,
    SteamClientRepairFailed = 6,
    InstallDirectoryUnavailable = 7,
    SteamApiUnavailable = 8,
    ExpectedSteamClientInvalid = 9,
    ActiveSteamClientInvalid = 10,
};

struct SteamApiResult {
    std::optional<std::filesystem::path> installPath;
    int failureCode = SteamApiUnavailable;
};

struct Services {
    std::function<std::optional<std::filesystem::path>(std::ostream&)> readSteamPath;
    std::function<int(const std::filesystem::path&, std::ostream&)> repairSteamClient;
    std::function<SteamApiResult(AppId, std::ostream&)> lookupSteamApi;
};

int Run(std::span<const std::string_view> arguments, const Services& services,
    std::ostream& output, std::ostream& errors);

}
