#include "Application.h"

namespace steam_location {

int Run(std::span<const std::string_view> arguments, const Services& services,
    std::ostream& output, std::ostream& errors) {
    const auto appId = arguments.size() == 1 ? ParseAppId(arguments.front()) : std::nullopt;
    if (!appId) {
        errors << "[Error] Usage: SteamAppsLocation <appid> (nonzero uint32 decimal).\n";
        return InvalidArguments;
    }

    int firstFailure = Success;
    const auto steamPath = services.readSteamPath(errors);
    if (steamPath) {
        firstFailure = services.repairSteamClient(*steamPath, errors);
    } else {
        firstFailure = SteamPathUnavailable;
        errors << "[Error] Failed to get SteamPath.\n";
    }

    auto api = services.lookupSteamApi(*appId, errors);
    std::optional<std::filesystem::path> installPath;
    if (api.installPath) {
        installPath = ExistingInstallPath(*api.installPath);
        if (!installPath) {
            api.failureCode = InstallDirectoryUnavailable;
            errors << "[Error] Steam API returned an unavailable install directory.\n";
        }
    }
    if (!installPath) {
        if (firstFailure == Success) firstFailure = api.failureCode;
        if (steamPath) installPath = FindGameInstallPath(*appId, *steamPath);
    }
    if (!installPath) {
        errors << "[Error] No existing install directory found for AppId " << *appId << ".\n";
        return firstFailure;
    }

    const auto bytes = installPath->u8string();
    output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    output.put('\n');
    return Success;
}

}
