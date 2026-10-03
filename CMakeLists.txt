cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
project(SteamAppsLocation LANGUAGES CXX)

if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
    message(FATAL_ERROR "SteamAppsLocation requires Windows MSVC x86. Configure with -A Win32.")
endif()
set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "Supported configurations" FORCE)

add_library(SteamAppsLocationCore STATIC
    src/Application.cpp src/GameInstallPath.cpp src/Vdf.cpp)
target_include_directories(SteamAppsLocationCore PUBLIC src)
target_compile_features(SteamAppsLocationCore PUBLIC cxx_std_20)
target_compile_definitions(SteamAppsLocationCore PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(SteamAppsLocationCore PUBLIC /W4 /utf-8 /permissive-)

add_library(SteamAppsLocationWindows STATIC src/WindowsRegistry.cpp src/WindowsSteamApi.cpp)
target_link_libraries(SteamAppsLocationWindows PUBLIC SteamAppsLocationCore advapi32)
target_include_directories(SteamAppsLocationWindows SYSTEM PRIVATE thirdparty/SteamSDK)

add_executable(SteamAppsLocation src/SteamAppsLocation.cpp)
target_link_libraries(SteamAppsLocation PRIVATE SteamAppsLocationWindows)
target_compile_options(SteamAppsLocation PRIVATE /Zi)
target_link_options(SteamAppsLocation PRIVATE /DEBUG)
set_target_properties(SteamAppsLocation PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
add_custom_command(TARGET SteamAppsLocation POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "${PROJECT_SOURCE_DIR}/thirdparty/SteamSDK/bin/steam_api.dll"
        "$<TARGET_FILE_DIR:SteamAppsLocation>/steam_api.dll"
    VERBATIM)

include(CTest)
if(BUILD_TESTING)
    add_executable(SteamAppsLocationTests tests/CoreTests.cpp)
    target_link_libraries(SteamAppsLocationTests PRIVATE SteamAppsLocationCore)
    add_test(NAME Core COMMAND SteamAppsLocationTests)
    add_library(SteamApiStub SHARED tests/SteamApiStub.cpp)
    add_library(SteamApiIncompleteStub SHARED tests/SteamApiStub.cpp)
    foreach(stub SteamApiStub SteamApiIncompleteStub)
        target_compile_features(${stub} PRIVATE cxx_std_20)
        target_compile_definitions(${stub} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
        target_compile_options(${stub} PRIVATE /W4 /utf-8 /permissive-)
    endforeach()
    target_compile_definitions(SteamApiIncompleteStub PRIVATE STEAM_TEST_MISSING_QUERY_EXPORT)
    add_executable(SteamAppsLocationWindowsTests tests/WindowsTests.cpp)
    target_link_libraries(SteamAppsLocationWindowsTests PRIVATE SteamAppsLocationWindows)
    add_dependencies(SteamAppsLocationWindowsTests SteamApiStub SteamApiIncompleteStub)
    add_test(NAME Windows COMMAND SteamAppsLocationWindowsTests
        "$<TARGET_FILE:SteamApiStub>" "$<TARGET_FILE:SteamApiIncompleteStub>")
    add_executable(SteamAppsLocationCommandHarness tests/CommandHarness.cpp)
    target_link_libraries(SteamAppsLocationCommandHarness PRIVATE SteamAppsLocationWindows)
    add_dependencies(SteamAppsLocationCommandHarness SteamApiStub SteamApiIncompleteStub SteamAppsLocation)
    add_test(NAME Command COMMAND "${CMAKE_COMMAND}"
        "-DHARNESS=$<TARGET_FILE:SteamAppsLocationCommandHarness>"
        "-DPROGRAM=$<TARGET_FILE:SteamAppsLocation>"
        "-DDLL=$<TARGET_FILE:SteamApiStub>"
        "-DINCOMPLETE_DLL=$<TARGET_FILE:SteamApiIncompleteStub>"
        "-DTEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/command-fixture"
        -P "${PROJECT_SOURCE_DIR}/tests/CommandTests.cmake")
endif()

install(TARGETS SteamAppsLocation RUNTIME DESTINATION .)
install(FILES "$<TARGET_PDB_FILE:SteamAppsLocation>"
    thirdparty/SteamSDK/bin/steam_api.dll README.md LICENSE STEAM-SDK-NOTICE.md DESTINATION .)
