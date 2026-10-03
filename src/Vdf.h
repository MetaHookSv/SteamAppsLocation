#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace steam_location {

struct VdfNode {
    std::string key;
    std::optional<std::string> value;
    std::vector<VdfNode> children;

    const VdfNode* Find(std::string_view name) const;
};

std::optional<VdfNode> ParseVdf(std::string_view text);

}
