#pragma once

#include <Windows.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace test_support {

namespace fs = std::filesystem;

template<typename T, typename U>
void ExpectEqual(const T& expected, const U& actual, const std::string& message) {
    if (expected != actual) {
        throw std::runtime_error(message);
    }
}

inline std::string Utf8(const fs::path& path) {
    const auto bytes = path.u8string();
    return std::string(bytes.begin(), bytes.end());
}

inline std::string Quote(const fs::path& path) {
    std::string text = "\"";
    for (char ch : Utf8(path)) {
        if (ch == '\\' || ch == '"') text += '\\';
        text += ch;
    }
    return text + '"';
}

inline void Write(const fs::path& path, const std::string& text) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary);
    file << text;
    if (!file) throw std::runtime_error("Cannot write fixture");
}

struct Fixture {
    fs::path root;

    Fixture() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = fs::temp_directory_path() / (L"SteamAppsLocation-tests-" +
            std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(stamp));
        if (!fs::create_directory(root)) throw std::runtime_error("Fixture already exists");
    }

    ~Fixture() {
        std::error_code error;
        fs::remove_all(root, error);
    }

    fs::path Install(const fs::path& library, const std::string& name, unsigned appId = 70) {
        const auto path = library / "steamapps" / "common" / fs::path(std::u8string(name.begin(), name.end()));
        fs::create_directories(path);
        Write(library / "steamapps" / ("appmanifest_" + std::to_string(appId) + ".acf"),
            "\"AppState\" { \"appid\" \"" + std::to_string(appId) +
            "\" \"installdir\" \"" + name + "\" }");
        return path;
    }
};

using Test = std::pair<const char*, std::function<void()>>;

inline int RunTests(const std::vector<Test>& tests) {
    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "PASS " << name << '\n';
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "FAIL " << name << ": " << error.what() << '\n';
        }
    }
    std::cout << tests.size() - failures << '/' << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}

}
