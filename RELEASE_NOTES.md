# Coaster Manager v1.1.5 — performance and UI responsiveness

Released binary is exactly the previously tested `v1.1.5 - SHARED UI ASSET CACHE` DLL. No new native code changes were introduced during release preparation.

### Changes since v1.1.0
- Avoid repeated idle full-scene Save Template panel searches by caching unsuccessful discovery for ~2 seconds, while checking known panels more frequently.
- Preserve the original ordered Unity object discovery and functional Save Template click handling (the experimental unsorted search was reverted).
- Retain and validate process-shared UI sprite and tooltip prefab assets between menu/button refreshes rather than searching thousands of objects again.
- Maintain existing coaster Save Template, template browser, placement, cost totals, post-build Thrill/Nausea/Ferocity refresh, and Steam Workshop blueprint Create/Update.

### User testing
- Single Coaster Manager baseline: 42 FPS → v1.1.5: 48–52 FPS.
- Earlier all-mods configuration: 39–40 FPS → v1.1.5 plus other mods: 43–50 FPS (user report; latest provided loader log still filtered all other mods).
- Save Template works in v1.1.5. UI refresh hitches improved (subjective user report).

### Known limitations
- Native method guards are tied to the November 2026 game build; changed code intentionally fails closed.
- Some template placements were rejected by orientation-validation guards; those guards are preserved.
- Initial cold UI creation and some background scene searches can still cause hitches.
- Diagnostic/performance log output remains enabled in the tested DLL and is bounded/rotated.
- No Mac/Steam Deck port is included.

**No mod-loader update is required.** Steam subscription installation uses the existing TPM Mod Loader v1.3 Workshop discovery; manual users need a numbered `[Mods]` loader.ini entry.
