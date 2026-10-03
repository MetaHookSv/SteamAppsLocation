#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string_view>

namespace steam_location {

using AppId = std::uint32_t;

std::optional<AppId> ParseAppId(std::string_view text);
std::filesystem::path PathFromUtf8(std::string_view text);
std::optional<std::filesystem::path> ExistingInstallPath(const std::filesystem::path& path);
std::optional<std::filesystem::path> FindGameInstallPath(
    AppId appId, const std::filesystem::path& steamPath);

}
