# Steamworks SDK provenance

The SDK headers, `steam_api.lib` and x86 `steam_api.dll` were copied without source changes from Steamworks SDK:

- Headers and import library: `include/SteamSDK`.
- Runtime: `tools/steam_api.dll`.

The header files retain Valve Corporation's original copyright notices.

These third-party files are not covered by this tool's MIT license.

Steamworks SDK usage is governed by Valve's applicable Steamworks terms.

The headers use the standard `steam/` include layout. The import library is kept for SDK completeness; the tool resolves the required runtime exports dynamically.
