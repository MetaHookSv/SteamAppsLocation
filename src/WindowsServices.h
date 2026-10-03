#pragma once

#include "Application.h"
#include <Windows.h>

namespace steam_location {

std::optional<std::filesystem::path> ReadSteamPath(
    std::ostream& errors, HKEY registryRoot = HKEY_CURRENT_USER);
int RepairSteamClient(const std::filesystem::path& steamPath,
    std::ostream& errors, HKEY registryRoot = HKEY_CURRENT_USER);
SteamApiResult LookupSteamApi(AppId appId, const std::filesystem::path& dllPath,
    std::ostream& errors);
std::filesystem::path ExecutableDirectory();

}
