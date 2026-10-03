#include "GameInstallPath.h"
#include "Vdf.h"
#include <charconv>
#include <fstream>
#include <iterator>
#include <vector>

namespace steam_location {
namespace fs = std::filesystem;
namespace {

std::optional<VdfNode> ReadVdf(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    const std::string text{std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
    if (file.bad()) return std::nullopt;
    return ParseVdf(text);
}

std::vector<fs::path> GetLibraryFolders(const fs::path& steamPath) {
    std::vector<fs::path> libraries{steamPath};
    const auto vdf = ReadVdf(steamPath / "steamapps" / "libraryfolders.vdf");
    const auto* folders = vdf ? vdf->Find("libraryfolders") : nullptr;
    if (!folders) return libraries;
    for (const auto& entry : folders->children) {
        const auto* path = entry.Find("path");
        if (!path || !path->value || path->value->empty()) continue;
        try {
            libraries.push_back(PathFromUtf8(*path->value));
        } catch (const fs::filesystem_error&) {
            // A bad library path must not prevent scanning other libraries.
        }
    }
    return libraries;
}

}

std::optional<AppId> ParseAppId(std::string_view text) {
    if (text.empty()) return std::nullopt;
    AppId appId = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), appId);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || appId == 0)
        return std::nullopt;
    return appId;
}

fs::path PathFromUtf8(std::string_view text) {
    return fs::path(std::u8string(text.begin(), text.end()));
}

std::optional<fs::path> ExistingInstallPath(const fs::path& path) {
    std::error_code error;
    if (path.empty() || !fs::is_directory(path, error)) return std::nullopt;
    auto absolute = fs::absolute(path, error);
    if (error) return std::nullopt;
    return absolute.lexically_normal();
}

std::optional<fs::path> FindGameInstallPath(AppId appId, const fs::path& steamPath) {
    for (const auto& library : GetLibraryFolders(steamPath)) {
        const auto common = library / "steamapps" / "common";
        std::error_code error;
        if (!fs::is_directory(common, error)) continue;
        const auto manifest = ReadVdf(library / "steamapps" /
            ("appmanifest_" + std::to_string(appId) + ".acf"));
        const auto* state = manifest ? manifest->Find("AppState") : nullptr;
        const auto* installDir = state ? state->Find("installdir") : nullptr;
        if (!installDir || !installDir->value || installDir->value->empty()) continue;
        try {
            if (auto path = ExistingInstallPath(common / PathFromUtf8(*installDir->value))) return path;
        } catch (const fs::filesystem_error&) {
            // Ignore invalid directory text and continue with the next library.
        }
    }
    return std::nullopt;
}

}
