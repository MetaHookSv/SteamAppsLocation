#include "Vdf.h"
#include <cctype>

namespace steam_location {
namespace {

bool SameKey(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (std::tolower(static_cast<unsigned char>(left[index])) !=
            std::tolower(static_cast<unsigned char>(right[index]))) return false;
    }
    return true;
}

enum class TokenKind { Text, Open, Close, End, Invalid };
struct Token {
    TokenKind kind;
    std::string text;
};

class Parser {
public:
    explicit Parser(std::string_view text) : text_(text) {
        if (text_.starts_with("\xEF\xBB\xBF")) position_ = 3;
    }

    std::optional<VdfNode> Parse() {
        VdfNode root;
        if (!ReadMembers(root, false, 0)) return std::nullopt;
        return root;
    }

private:
    Token Next() {
        while (position_ < text_.size()) {
            if (std::isspace(static_cast<unsigned char>(text_[position_]))) {
                ++position_;
            } else if (text_.substr(position_, 2) == "//") {
                const auto end = text_.find('\n', position_ + 2);
                position_ = end == std::string_view::npos ? text_.size() : end + 1;
            } else {
                break;
            }
        }
        if (position_ == text_.size()) return {TokenKind::End, {}};

        const char first = text_[position_++];
        if (first == '{') return {TokenKind::Open, {}};
        if (first == '}') return {TokenKind::Close, {}};
        std::string value;
        if (first == '"') {
            while (position_ < text_.size()) {
                const char ch = text_[position_++];
                if (ch == '"') return {TokenKind::Text, std::move(value)};
                if (ch != '\\') {
                    value += ch;
                    continue;
                }
                if (position_ == text_.size()) return {TokenKind::Invalid, {}};
                const char escaped = text_[position_++];
                switch (escaped) {
                case '\\': case '"': value += escaped; break;
                case 'n': value += '\n'; break;
                case 'r': value += '\r'; break;
                case 't': value += '\t'; break;
                default: value += '\\'; value += escaped; break;
                }
            }
            return {TokenKind::Invalid, {}};
        }

        value += first;
        while (position_ < text_.size()) {
            const char ch = text_[position_];
            if (std::isspace(static_cast<unsigned char>(ch)) || ch == '{' || ch == '}' || ch == '"') break;
            value += ch;
            ++position_;
        }
        return {TokenKind::Text, std::move(value)};
    }

    bool ReadMembers(VdfNode& object, bool insideObject, unsigned depth) {
        constexpr unsigned maxDepth = 128;
        if (depth > maxDepth) return false;
        while (true) {
            auto key = Next();
            if (key.kind == TokenKind::End) return !insideObject;
            if (key.kind == TokenKind::Close) return insideObject;
            if (key.kind != TokenKind::Text) return false;

            VdfNode member{std::move(key.text), std::nullopt, {}};
            auto value = Next();
            if (value.kind == TokenKind::Text) {
                member.value = std::move(value.text);
            } else if (value.kind == TokenKind::Open) {
                if (!ReadMembers(member, true, depth + 1)) return false;
            } else {
                return false;
            }
            object.children.push_back(std::move(member));
        }
    }

    std::string_view text_;
    std::size_t position_ = 0;
};

}

const VdfNode* VdfNode::Find(std::string_view name) const {
    for (const auto& child : children) {
        if (SameKey(child.key, name)) return &child;
    }
    return nullptr;
}

std::optional<VdfNode> ParseVdf(std::string_view text) {
    return Parser(text).Parse();
}

}
