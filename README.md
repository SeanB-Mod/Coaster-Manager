# Coaster Manager

**Save, rebuild and share rollercoasters in Two Point Museum.**

Current version: **1.1.5** · Platform: **Windows x64** · Requires the rollercoaster/Funfair DLC and the native [TPM Mod Loader](https://github.com/SeanB-Mod/TPM-Mod-Loader).

This repository is intended for the v1.1.5 source, build scripts, tests, documentation and Windows DLL. The compiled DLL in the release package was not rebuilt or modified.

This update improves idle performance and reduces repeated coaster-browser refresh work. It retains Save Template, local library, template building, price accounting, Steam Workshop publishing, and placed-coaster statistics.

## Performance observed in the user's game

| Condition | Observed FPS |
|---|---:|
| Earlier Coaster Manager on its own | 42 |
| v1.1.5 on its own | 48–52 |
| Earlier full mod setup | 39–40 |
| v1.1.5 with other mods (user report) | 43–50 |

FPS depends on the scene, hardware and other loaded mods; measurements are not a reproducible frame-time benchmark. The latest supplied loader log was still in selective-loading mode (Coaster Manager only), so the stated full-mod result has not yet been independently validated from a loader log.

## Features and behaviour

- Save rollercoaster layouts with name and screenshot; browse, preview and reuse `.coaster` templates.
- Preview/place a station and reconstruct supports and track through the guarded native construction process.
- Placed-coaster Thrill, Nausea and Ferocity are refreshed using the native spline calculation.
- Template browser displays station + track + £500 per support + £500 per decoration. Steam blueprint Create/Update descriptions use the same full total without changing existing `.coaster` metadata.
- Ordered Save Template UI discovery uses a two-second retry after unsuccessful searches; this avoids the v1.1.3 unsorted-search regression that caused nonfunctional buttons.
- Shared Unity icon/tooltip prefab assets are cached across browser refreshes where valid, reducing redundant global searches.

## Compatibility and caveats

- Two Point Museum for Windows x64, with rollercoaster/Funfair DLC and a working native DLL mod loader.
- Game build guarded against the supplied November 2026 Windows game binaries. Changed game code may cause features to fail closed until revalidated; don't bypass checks.
- This is a **native DLL mod**, not a standalone BepInEx managed plugin.
- Some template placements were safely rejected by orientation-validation guards in the development logs. No guard was weakened for this release.
- One-off opening/rendering work can still cause short hitches; the refresh cache is not a complete removal of first-time UI costs.
- Debug/performance logs remain enabled, with an 8 MiB logging budget and rotation. They were **not stripped** so the DLL remains byte-for-byte the tested binary. They may be reduced in a separately tested future version.
- No Mac/Steam Deck native builds included in this release.

## Installation (Windows)

1. Exit Two Point Museum and back up any older `TPM-Coaster-Tools.dll` **outside** the game's mod folders.
2. Copy `Mods/TPM-Coaster-Tools.dll` from the package into the game directory's `Mods` folder.
3. For manual installs with TPM Mod Loader v1.3, add an unused numbered entry for `TPM-Coaster-Tools.dll` under `[Mods]` in `Mods/loader.ini`; local DLLs aren't auto-discovered.
4. Restart the game. Confirm the DLL loads in `Mods/Logs/loader.log`.

Steam Workshop subscribers should update via the existing Workshop item instead of installing an additional manual copy. Read [INSTALL-WINDOWS.txt](INSTALL-WINDOWS.txt) for diagnostic loader settings and full steps. **Do not delete saved coaster templates.**

## Building from source

`source/` contains the Windows native C implementation, included components and automated tests. Use [source/build.cmd](source/build.cmd) with `clang-cl` and `lld-link` available in your PATH (or [source/build.sh](source/build.sh) on a compatible environment). Output: `Mods/TPM-Coaster-Tools.dll`.

The included DLL's SHA-256 is `2a9431c531bfa94ada28ea99cd7ece1f183322458c267f085aa0de1848e93a7a`. See [RELEASE_VALIDATION.json](RELEASE_VALIDATION.json) for the checks performed when the package was prepared.

This repository intentionally contains no `winhttp.dll` or `loader.ini`. Do not upload the source archive to a Steam Workshop DLL entry; use the Workshop-specific package as described in [UPLOAD.md](UPLOAD.md).

## Development/validation

In-game: the user confirmed the Save Template button works, smoother refreshes, and the above FPS results; latest game log includes completed saves and validated template reconstruction. Offline: source reproduced the compiled DLL SHA-256, all 11 reviewed November method guards match, the BuildSpline guard matches, and component JSON / template total pricing / Workshop price tests pass. Windows-only UI automated tests have not been run in this Linux build environment.

Developed with assistance from ChatGPT by OpenAI.

## Project status and licensing

Windows x64 v1.1.5 is the user-tested baseline. Native game updates may require compatibility review. No open-source licence was supplied with this package; a public repository alone does not grant reuse or redistribution permissions.
