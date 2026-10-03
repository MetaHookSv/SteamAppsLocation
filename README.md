# SteamAppsLocation

A Windows x86 command-line tool that locates an installed Steam game by AppId.
Originally part of [MetaHookSv](https://github.com/hzqst/MetaHookSv/tree/main/toolsrc/SteamAppsLocation).

## Usage

```bat
SteamAppsLocation.exe 70
```

On success, stdout contains exactly one UTF-8 absolute install path followed by a newline.
Diagnostics, including Steam SDK messages, go to stderr. Failure leaves stdout empty.
The AppId must be a nonzero decimal integer in the uint32 range.

The program sets `SteamAppId` and `SteamGameId` in its own process before initializing
Steam; callers do not need to create `steam_appid.txt`.

### Lookup order

1. Read `HKCU\Software\Valve\Steam\SteamPath` and attempt the original
   `ActiveProcess\SteamClientDll` registry repair if its canonical path differs from
   `<SteamPath>\steamclient.dll`. Failure to read, resolve or repair this value does
   not stop the lookup. The repair writes the expected canonical DLL path to the
   existing registry value; it does not require administrator access.
2. Dynamically load the bundled `steam_api.dll` from the executable directory and
   query SteamApps for the requested game's install directory. Steam API access
   generally requires a running Steam client and an eligible account/AppId.
3. If the DLL, exports, initialization, interface or query are unavailable, or the
   returned directory does not exist, scan Steam library manifests. Check Steam's
   primary library first, then the additional libraries in
   `steamapps/libraryfolders.vdf`. Read each `appmanifest_<appid>.acf` and use its
   `installdir` under `steamapps/common`, returning only an existing directory.

Manifest lookup works without a running Steam client or `steam_api.dll`, provided
SteamPath and the installed game's manifest are available. Missing or malformed
library configuration still permits checking the primary library. Missing,
unreadable or malformed manifests are skipped. This fallback is ported from
the MetaHook installer `MainViewModel.GetGameInstallPath` implementation.

### Exit codes

| Code | Meaning |
| --- | --- |
| 0 | An existing install directory was found through either lookup method. |
| 1 | Missing, extra or invalid AppId argument. |
| 4 | SteamPath could not be read. |
| 5 | ActiveProcess SteamClientDll could not be read. |
| 6 | SteamClientDll registry repair failed. |
| 7 | SteamApps interface, install-directory query or returned directory was unavailable. |
| 8 | Steam API DLL/export/initialization was unavailable, or an unexpected error occurred. |
| 9 | The expected steamclient.dll path could not be resolved. |
| 10 | The active SteamClientDll path could not be resolved or compared. |

After both lookup methods fail, the first failing stage determines the exit code.
A successful lookup returns 0 even when an earlier registry or API stage failed.

## Build and test

Requirements: Windows, Visual Studio 2022 with the C++ desktop workload and Windows
SDK, and CMake 3.21 or newer. The x86 Steam SDK headers, import library and runtime
are provided by the [SteamSDK](https://github.com/MetaHookSv/SteamSDK) submodule at
`thirdparty/SteamSDK`; no MetaHook checkout is required.

Clone with submodules:

```bat
git clone --recurse-submodules https://github.com/MetaHookSv/SteamAppsLocation.git
cd SteamAppsLocation
```

For an existing checkout, initialize or update the SDK before building:

```bat
git submodule update --init --recursive
```

```bat
scripts\build-SteamAppsLocation-x86-Release.bat
scripts\build-SteamAppsLocation-x86-Debug.bat
```

These scripts configure, build, run CTest, and install to
`install\x86\<Configuration>`. The install directory contains the EXE, PDB, Steam API
DLL, README, license and SDK notice. The build uses the static MSVC runtime.

Equivalent commands for Release:

```bat
cmake -G "Visual Studio 17 2022" -A Win32 -S . -B build/x86/Release -DCMAKE_INSTALL_PREFIX=install/x86/Release
cmake --build build/x86/Release --config Release --parallel
ctest --test-dir build/x86/Release -C Release --output-on-failure
cmake --install build/x86/Release --config Release
```

Tests use temporary libraries, simulated SDK DLLs and isolated temporary registry
keys. They do not modify the real Steam registry keys or require Steam to run.
The copied import library is retained with the SDK but the executable does not
link it, allowing manifest fallback when the DLL is absent.

## Builds and releases

GitHub Actions builds and tests x86 Release for main pushes and pull requests.
Tags matching `v*` create a release. Both workflows package the installed files as
`SteamAppsLocation-windows-x86.7z` and verify the archive with `7z t`.

## License

Tool code is distributed under the original [MIT license](LICENSE).
Valve's SDK files retain their original copyright notices; see
[STEAM-SDK-NOTICE.md](STEAM-SDK-NOTICE.md).
