# WOTR-1.1 · K5 · Localization & cultural QA — Acceptance Report

- **Card:** `WOTR-1.1 · K5 · Localization & cultural QA` (`41f6f0cf-5705-81b6-8eb3-d88b588bc07e`)
- **Mission:** `WOTR-1.1` (`docs/missions/WOTR-1.1.md`, §8 K5)
- **Role:** `Localization & Cultural QA Engineer` (`41e6f0cf-5705-81b5-8013-c27b1033fd8c`, `docs/agents/roles/localization-cultural-qa-engineer.md`)
- **Normative Decisions:** `WOTR-D-032` (`docs/DECISIONS.md`), `ADR-003` (`docs/architecture/ADR-003-PRODUCT-LOCALES.md`), `docs/L10N.md`, `WOTR-D-031`, `WOTR-D-040`
- **Branch:** `arena/5fc45f83-open-bfme` (branched from `d2e86349cede84cb995e3aeecd2e80727f88922e` on `main`)
- **Cost Gate:** `build: full`, `retail: none` (`docs/agents/cost-gates.md`)
- **Date:** `2026-11-24`

---

## 1. Summary of Changes & Scope Reconciliation

This card executes `WOTR-D-032` and `ADR-003`: re-tiering OpenBFME's required product locale baseline to six locales (`en`, `ru`, `de`, `fr`, `es`, `it`), adding Spanish (`es.json`) and Italian (`it.json`) catalogs, parking CJK catalogs (`zh-Hans.json`, `ja.json`, `ko.json`) as `"@@status": "sample"` without deleting data, retaining `pt-BR.json` as a community-tier `"partial"` catalog, registering the WOTR-1.1 HUD text IDs (`game.wotr.remaster.hud.{load,move,save}`) across all six product locales, regenerating runtime tables, and proving 6-locale canonical hash invariance plus target-screen visual, shaping, clipping, fallback, plural, and localized-input behavior.

### Touched repository files

| Path | Action | Zone / Owner | Notes |
| --- | --- | --- | --- |
| `tools/l10n/l10n.py` | Modified | `tools/l10n/**` (K5) | `REQUIRED_LOCALES = ("en", "ru", "de", "fr", "es", "it")`; added `godot/wotr_remaster` to `SCAN_GD`; `required_locale_problems` rejects missing or `"sample"` required locales; `PLURAL_CATEGORIES` for `zh`/`ja`/`ko`/`pt-BR` retained. |
| `tools/l10n/catalog/en.json` | Modified | `tools/l10n/**` (K5) | Added `game.wotr.remaster.hud.{load,move,save}` (`1301` source IDs total). |
| `tools/l10n/catalog/de.json` | Modified | `tools/l10n/**` (K5) | Added `game.wotr.remaster.hud.{load,move,save}` (`53/1301`, `"partial"`; all 50 prior translations preserved verbatim). |
| `tools/l10n/catalog/ru.json` | Modified | `tools/l10n/**` (K5) | Expanded to `53/1301` (`"partial"`), covering 100% of `common.*` and `game.wotr.*`, CLDR 3-form plural `launcher.online_count.players_online`, and 43 Launcher UI strings (all 12 prior strings preserved verbatim). |
| `tools/l10n/catalog/fr.json` | Modified | `tools/l10n/**` (K5) | Expanded to `53/1301` (`"partial"`), covering 100% of `common.*` and `game.wotr.*`, CLDR plural, and 43 Launcher UI strings (all 12 prior strings preserved verbatim). |
| `tools/l10n/catalog/es.json` | Added | `tools/l10n/**` (K5) | New Spanish product catalog (`Español`, `"@@status": "partial"`, `53/1301`). |
| `tools/l10n/catalog/it.json` | Added | `tools/l10n/**` (K5) | New Italian product catalog (`Italiano`, `"@@status": "partial"`, `53/1301`). |
| `tools/l10n/catalog/zh-Hans.json` | Modified | `tools/l10n/**` (K5) | `"@@status"` changed `"partial"` -> `"sample"` (all 12 translations retained). |
| `tools/l10n/catalog/ja.json` | Modified | `tools/l10n/**` (K5) | `"@@status"` changed `"partial"` -> `"sample"` (all 12 translations retained). |
| `tools/l10n/catalog/ko.json` | Modified | `tools/l10n/**` (K5) | `"@@status"` changed `"partial"` -> `"sample"` (all 12 translations retained). |
| `launcher/scripts/l10n/catalog.gd` | Generated | `launcher/scripts/l10n/**` (K5) | Regenerated exclusively via `python3 tools/l10n/l10n.py generate`. |
| `godot/scripts/l10n/catalog.gd` | Generated | `godot/scripts/l10n/**` (K5) | Regenerated exclusively via `python3 tools/l10n/l10n.py generate`; `launcher/scripts/l10n/l10n.gd` and `godot/scripts/l10n/l10n.gd` verified byte-identical. |
| `engine/src/GameClient/OpenBFMETextCatalog.inc` | Generated | `engine/src/GameClient/OpenBFMEText*` (Role zone; reconciled below) | Regenerated exclusively via `python3 tools/l10n/l10n.py generate` (`CONSUMERS["engine"]`). `OpenBFMEText.cpp` untouched. |
| `tools/l10n/test_l10n.py` | Modified | `tools/l10n/**` (K5) | Updated baseline assertions for the 6 product locales, `sample` rejection on `REQUIRED_LOCALES`, `pt-BR` community disposition, `zh-Hans`/`ja`/`ko` parked `sample` disposition, phase-1 WOTR IDs, and byte-identical `RUNTIME == RUNTIME_COPY`. |
| `launcher/tests/l10n_test.gd` | Modified | `launcher/tests/l10n_test.gd` (K5) | Enumerates `["en", "ru", "de", "fr", "es", "it"]`, verifies `pt-BR` (`partial`) and `zh-Hans`/`ja`/`ko` (`sample`, `not automatic_ok()`), per-message English fallback across all partial/sample locales, CLDR plurals + decimal commas (`de`, `fr`, `es`, `it`, `ru`, `pt-BR`), and retail install-alias resolution (`German`, `English`, `Russian`, `French`, `Spanish`, `Italian`, `Chinese` -> `zh-Hans`, `Korean` -> `ko`, `Brazilian` -> `pt-BR`, `Portuguese` -> `pt-BR`). |
| `docs/evidence/WOTR-1-1/K5/report.md` | Added | `docs/evidence/WOTR-1-1/K5/**` (K5) | This acceptance report. |
| `docs/evidence/WOTR-1-1/K5/wotr_hash_invariance_6locales.json` | Added | `docs/evidence/WOTR-1-1/K5/**` (K5) | Machine-verifiable 6-locale x 2-repeat canonical hash-invariance + World HUD geometry/shaping report. |
| `docs/evidence/WOTR-1-1/K5/launcher_geometry_shaping_input_6locales.json` | Added | `docs/evidence/WOTR-1-1/K5/**` (K5) | Machine-verifiable 6-locale Launcher geometry, glyph shaping, clipping, dropdown, and `LineEdit` input report. |

### Explicit Scope Reconciliation (`OpenBFMETextCatalog.inc`)

- `python3 tools/l10n/l10n.py generate` writes three generated consumer tables in lockstep (`launcher/scripts/l10n/catalog.gd`, `godot/scripts/l10n/catalog.gd`, and `engine/src/GameClient/OpenBFMETextCatalog.inc`), and `python3 tools/l10n/l10n.py check` (plus `tools/l10n/test_l10n.py::test_generated_files_are_current` in CI) fails if `OpenBFMETextCatalog.inc` is stale after adding `es.json`/`it.json` and updating `@@status` on `zh-Hans`/`ja`/`ko`.
- `engine/src/GameClient/OpenBFMEText*` is explicitly inside the `Localization & Cultural QA Engineer` write zone (`docs/agents/roles/localization-cultural-qa-engineer.md`), whereas the K5 card text listed `engine/**` in its out-of-scope note (`OpenBFMEText.cpp` needs no code edits).
- Per `docs/agents/protocol.md` escalation rules, this discrepancy was surfaced to the user before committing; the user explicitly authorized regenerating and committing `engine/src/GameClient/OpenBFMETextCatalog.inc` via `l10n.py generate` while leaving `OpenBFMEText.cpp` and all other `engine/**` files untouched.

---

## 2. Locale Tier Dispositions & Warden Handoff Items

### Catalog inventory (`1301` source IDs in `en.json`)

| Tag | Native Name | Tier (`ADR-003` / `WOTR-D-032`) | `@@status` | Coverage (`l10n.py coverage`) | `REQUIRED_LOCALES` | `L10n.automatic_ok()` | Selector Label Behavior |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `en` | `English` | Product (source) | `source` | `1301 / 1301` | Yes | `true` | `English` |
| `ru` | `Русский` | Product | `partial` | `53 / 1301` (1248 fallback to `en`) | Yes | `true` | Marked incomplete (`launcher.settings.language_sample`) |
| `de` | `Deutsch` | Product | `partial` | `53 / 1301` (1248 fallback to `en`) | Yes | `true` | Marked incomplete (`launcher.settings.language_sample`) |
| `fr` | `Français` | Product | `partial` | `53 / 1301` (1248 fallback to `en`) | Yes | `true` | Marked incomplete (`launcher.settings.language_sample`) |
| `es` | `Español` | Product (new in K5) | `partial` | `53 / 1301` (1248 fallback to `en`) | Yes | `true` | Marked incomplete (`launcher.settings.language_sample`) |
| `it` | `Italiano` | Product (new in K5) | `partial` | `53 / 1301` (1248 fallback to `en`) | Yes | `true` | Marked incomplete (`launcher.settings.language_sample`) |
| `pt-BR` | `Português (Brasil)` | Community (file untouched) | `partial` | `12 / 1301` (1289 fallback to `en`) | No | `true` | Marked incomplete (`launcher.settings.language_sample`) |
| `zh-Hans` | `简体中文` | Out-of-plan (parked in K5) | `sample` | `12 / 1301` (1289 fallback to `en`) | No | `false` | Marked incomplete; never picked by `Automatic` |
| `ja` | `日本語` | Out-of-plan (parked in K5) | `sample` | `12 / 1301` (1289 fallback to `en`) | No | `false` | Marked incomplete; never picked by `Automatic` |
| `ko` | `한국어` | Out-of-plan (parked in K5) | `sample` | `12 / 1301` (1289 fallback to `en`) | No | `false` | Marked incomplete; never picked by `Automatic` |

### Explicit dispositions required by K5

1. **`pt-BR` disposition:** `tools/l10n/catalog/pt-BR.json` is preserved byte-for-byte untouched at `"@@status": "partial"` (`12/1301`) in the community tier (`ADR-003`), removed from `REQUIRED_LOCALES`, kept in `PLURAL_CATEGORIES`, and verified in `launcher/tests/l10n_test.gd` so both `Brazilian.big` and `Portuguese.big` retail install aliases resolve to the exact public tag `"pt-BR"` (never `"pt"`).
2. **`zh-Hans`, `ja`, `ko` disposition:** Parked in `tools/l10n/catalog/{zh-Hans,ja,ko}.json` by setting `"@@status": "sample"` while retaining all 12 existing translations, `PLURAL_CATEGORIES` entries, and retail install-alias mappings (`Chinese.big` -> `"zh-Hans"`, `Korean.big` -> `"ko"`). Verified in `launcher/tests/l10n_test.gd` that `L10n.automatic_ok()` returns `false` for all three, so `Automatic` never activates a parked `sample` catalog even on a matching retail install.
3. **Partial-locale honesty:** All five non-English product catalogs (`ru`, `de`, `fr`, `es`, `it`) remain `"@@status": "partial"` (`53/1301`) and are never claimed `"complete"` or production-finished; the selector visibly appends the localized `(incomplete translation)` marker for every non-`en` catalog.
4. **Warden follow-up at приёмка (`docs/**` out of K5 `Scope`):** Per the K5 card specification, `docs/**` (outside `docs/evidence/WOTR-1-1/K5/**`) was not modified in this PR. Removing the transitional phrase `"until the localization card lands"` in `docs/L10N.md:21` and `docs/missions/WOTR-1.1.md:33` is recorded here for Warden execution at card acceptance (приёмка).

---

## 3. Phase-1 String Audit & Cross-Zone QA Findings

### Catalog wiring verified in K5

- Added `tools/l10n/l10n.py` `SCAN_GD` coverage for `godot/wotr_remaster`.
- `godot/wotr_remaster/world/wotr_text.gd` looks up `game.wotr.remaster.hud.move`, `game.wotr.remaster.hud.save`, and `game.wotr.remaster.hud.load` via `L10n.has(id)` -> `L10n.t(id)`. Adding those three source IDs to `en.json` and translating them in `ru.json`, `de.json`, `fr.json`, `es.json`, and `it.json` immediately localized the WOTR Remaster World HUD buttons across all six product locales with zero edits to `godot/wotr_remaster/**`.
- `python3 tools/l10n/l10n.py check` confirms 0 unknown IDs, 0 unused IDs across all `1301` source keys, 0 placeholder/plural mismatches, 0 stale generated files, and 0 new hardcoded player strings in `launcher/`.

### Cross-zone QA observations (reported without modifying foreign write zones)

1. **`godot/wotr_remaster/world/wotr_world.gd` (`K4` Godot World Engineer zone):** `status_report()` and `_phase_name()` format developer/readout status lines (`turn 0 · planning · state ...`, `no army selected`, `regions: ...`, `no pending battle`) as inline GDScript strings rather than `L10n.t(...)` catalog keys, whereas the interactive HUD controls in `wotr_world_hud.gd` (`Move`, `Save`, `Load`) go through `wotr_text.gd` -> `L10n.t(...)`. If those readout lines remain player-visible beyond the WOTR-1.1 slice, they should be migrated to `game.wotr.remaster.hud.*` catalog IDs in the Godot World Engineer zone.
2. **`campaigns/fixtures/wotr_1_1/pack.json` (`K2` Strategic Core zone):** The synthetic 3-region deterministic fixture uses bare identifiers (`west`, `crossing`, `east`, `army_gondor`, `wotr.unit.knight`) and pins its own SHA-256 `contentHash` (`e1835095e2a93d46582ac182ec625c1f4c5b4b6902bb45ca26212ccff5d4dd74`). Display-name localization keys for campaign regions/units belong to future campaign data packs rather than fixture IDs.
3. **`launcher/fonts/Cinzel.ttf` Cyrillic fallback (`launcher/scripts/ui/style.gd`, Launcher Engineer zone):** `Cinzel.ttf` (used on `Heading` / `W.h2` and the `GoldButton` `PLAY` CTA) includes Latin and Latin-1 Supplement uppercase diacritics (`È` in `PARAMÈTRES`, `Ó` in `CONFIGURACIÓN`), so `en`, `de`, `fr`, `es`, and `it` headings render in serif `Cinzel`. `Cinzel.ttf` has no Cyrillic block (`U+0400..U+04FF`), so under `ru` Godot 4.7's font fallback chain renders `Настройки`, `Что нового`, and `ИГРАТЬ` in Godot's built-in sans-serif fallback font (`0` missing glyphs, fully legible, no `.notdef` boxes, stylistic shift from serif to sans-serif). Adding `CormorantGaramond.ttf` (already bundled in `launcher/fonts/` with full Cyrillic coverage) to `FontVariation.fallbacks` for `Cinzel` in `launcher/scripts/ui/style.gd` is an optional visual-consistency improvement for Launcher Engineer.
4. **Language selector `PopupMenu` CJK `sample` glyph boxes (`WOTR-1.1__K5__launcher-dropdown-es__2026-11-24.png`):** All six product locales (`English`, `Deutsch`, `Español`, `Français`, `Italiano`, `Русский`) and community `Português (Brasil)` render with `0` missing glyphs in the open dropdown. The three parked `"sample"` CJK native labels (`日本語`, `한국어`, `简体中文`) display hex-codepoint fallback boxes in the `PopupMenu` when no system/bundled CJK font is present — visually confirming the `ADR-003` / `WOTR-D-032` rationale for parking CJK out of the product tier until dedicated CJK font/IME work is scheduled, while keeping their `(traducción incompleta)` suffix legible.
5. **3D region label occlusion in `wotr_world.tscn` (`K4` zone):** At the initial `Camera3D` angle (`1280x720`), the 3D `Label3D` region names `west` and `east` sit partially behind the `army_gondor` (yellow) and `army_mordor` (red) banner meshes, while `crossing` in the center hex is unobstructed.

---

## 4. Six-Locale Canonical Hash-Invariance Proof

Executed `12` full deterministic runs (`en`, `ru`, `de`, `fr`, `es`, `it` x `2` independent repeats per locale) through `res://wotr_remaster/world/wotr_world.tscn` + native `libwotr_core.so` (`campaigns/fixtures/wotr_1_1/pack.json`, `seed = 20261121`). Full machine output is committed in `docs/evidence/WOTR-1-1/K5/wotr_hash_invariance_6locales.json`.

| Locale (`repeat 1 & 2`) | Live HUD Button Texts (`move` / `save` / `load`) | Initial State Hash | After 1st Move Hash | After Conflict Hash | `BattleRequest` Checksum (`WotrCore`) | `BattleResult` Canonical JSON SHA-256 | After Battle State Hash | Save Envelope Bytes & SHA-256 | Fresh-Core Reloaded State Hash |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `en` (`#1`, `#2`) | `Move` / `Save` / `Load` | `29689fc52edb46a4...` | `8531f522db306308...` | `c6d9f2a1a73d8b9d...` | `d2092dedace438fa...` | `fb1eada13667daba...` | `5ab6958ba0ed8d32...` | `2173 B` · `3fdf1af0c10e3d41...` | `5ab6958ba0ed8d32...` |
| `ru` (`#1`, `#2`) | `Переместить` / `Сохранить` / `Загрузить` | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) |
| `de` (`#1`, `#2`) | `Bewegen` / `Speichern` / `Laden` | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) |
| `fr` (`#1`, `#2`) | `Déplacer` / `Enregistrer` / `Charger` | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) |
| `es` (`#1`, `#2`) | `Mover` / `Guardar` / `Cargar` | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) |
| `it` (`#1`, `#2`) | `Muovi` / `Salva` / `Carica` | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) | Identical (`== en`) |

### Exact canonical 64-hex values (identical across all 12 runs)

- `pack_hash`: `e1835095e2a93d46582ac182ec625c1f4c5b4b6902bb45ca26212ccff5d4dd74`
- `initial_state_hash`: `29689fc52edb46a45a3960c641c9fe18803a4e9c59c54c8167d3b4b61ac28c73`
- `initial_snapshot_sha256`: `ebc2cbb6f238c51a579e19f25987f28653a1e9f53757684884086cafe60e1b1d`
- `after_first_move_hash`: `8531f522db30630820ef692f33f19d47d80f912e7940a9c0c9e151dcda43909a`
- `after_conflict_hash`: `c6d9f2a1a73d8b9d282011ba46611174ead8b2d17157401bb75c543158011cb2`
- `battle_request_checksum`: `d2092dedace438fadc97f158656817de6e9d4117b0cf29a54e4723d0f0a06cf2`
- `battle_request_json_sha256`: `a494c8c572bc2e71fb7297d31ea77d368da8f2e930a191803148eb4522460df1`
- `battle_result_json_sha256`: `fb1eada13667daba6f4541096fa1a1107213fb25bb9cc59e6faa4053c5bd4571`
- `after_battle_hash`: `5ab6958ba0ed8d32985e2e4b2092d6e4dbc3a790d6daf3ad134b8a974581e32b`
- `save_bytes` / `save_sha256`: `2173` bytes / `3fdf1af0c10e3d41599b4dbff83568d7cbc635c67072bb77489399752fe8ee0a`
- `reloaded_state_hash`: `5ab6958ba0ed8d32985e2e4b2092d6e4dbc3a790d6daf3ad134b8a974581e32b`
- `reloaded_snapshot_sha256`: `74544f4c28ca9077746876ed4fc8a78a12984e6965b899f78a5e244f232cb6e3`

---

## 5. Geometry, Shaping, Clipping & Localized Input QA Summary

Full machine-readable measurements are committed in `docs/evidence/WOTR-1-1/K5/wotr_hash_invariance_6locales.json` and `docs/evidence/WOTR-1-1/K5/launcher_geometry_shaping_input_6locales.json`:

1. **WOTR Remaster World HUD (`1280x720`, `en`/`ru`/`de`/`fr`/`es`/`it`):**
   - `Hud/Root/Panel` (`460 x 170 px`) encloses `Buttons` row (`460 x 32 px`) across all 6 locales (`button_row_fits_hud_panel = true`).
   - Longest button strings — Russian `Переместить` (`shaped_width = 97.0 px`, button auto-expands from `96.0 px` min-size to `105.0 px`, `clip_text = false`, `missing_glyphs = 0`) and French `Enregistrer` (`shaped_width = 81.0 px`, button `96.0 px`, `horizontal_slack_px = 7.0 px`, `missing_glyphs = 0`) — fit with zero clipping or overlap.
   - Multi-line `TextParagraph` shaping at `420 px` width for `game.wotr.map_missing.{title,text}` and `game.wotr.rts_demoted.{title,text}` (`region = "Minas Tirith"`) wraps cleanly (`1` line titles, `2–4` line bodies, `fits_width = true`, `missing_glyphs = 0` across all 6 locales).
2. **Launcher Settings & Long-Text Stress (`1280x800`, `en`/`ru`/`de`/`fr`/`es`/`it`):**
   - `32–33` visible text controls measured on `settings` and `36` visible text controls measured on `long` per locale: `0` missing glyphs across the font chain (`missing_glyphs_full_chain = 0`), `0` viewport overflows (`viewport_overflow_count = 0`), and `0` chip overlaps (`long_chip_overlaps = []`).
   - Verified `10,000,000` online-player plural chips across all 6 locales: `●  10000000 players online` (`en`), `●  10000000 игроков в сети` (`ru`), `●  10000000 Spieler online` (`de`), `●  10000000 joueurs en ligne` (`fr`), `●  10000000 jugadores en línea` (`es`), `●  10000000 giocatori online` (`it`).
   - Baseline `launcher/tests/layout_test.gd` passed across all `29 cases x 9 window sizes` (`23,157` controls measured, `rc = 0`).
3. **Localized `LineEdit` Keyboard Input & UTF-8 Editing (`1280x800`):**
   - Dispatched real `InputEventKey` Unicode keystrokes (`Viewport.push_input`) into `Multiplayer -> _name` (`LineEdit`, `870 px` width) across all 6 product locales:
     - `en`: `"Fellowship Ring 2026"` (`20` codepoints, `20` UTF-8 bytes, `shaped_width = 154.0 px`, `caret_column = 20`, `missing_glyphs = 0`)
     - `ru`: `"Средиземье Ход №5"` (`17` codepoints, `32` UTF-8 bytes, `shaped_width = 155.0 px`, `caret_column = 17`, `missing_glyphs = 0`)
     - `de`: `"Größe Übung König 7"` (`19` codepoints, `23` UTF-8 bytes, `shaped_width = 154.0 px`, `caret_column = 19`, `missing_glyphs = 0`)
     - `fr`: `"Édition Héroïque 12"` (`19` codepoints, `22` UTF-8 bytes, `shaped_width = 137.0 px`, `caret_column = 19`, `missing_glyphs = 0`)
     - `es`: `"Señor Città d'Été 42"` (`20` codepoints, `24` UTF-8 bytes, `shaped_width = 144.0 px`, `caret_column = 20`, `missing_glyphs = 0`)
     - `it`: `"Città dell'Anello 99"` (`20` codepoints, `21` UTF-8 bytes, `shaped_width = 135.0 px`, `caret_column = 20`, `missing_glyphs = 0`)
   - Verified `KEY_BACKSPACE` deletes one Unicode codepoint cleanly without corrupting multi-byte UTF-8 sequences (`backspace_multibyte_ok = true` across all 6 locales). Note: `DisplayServerOffscreen` reports `FEATURE_IME = false` (expected for Latin/Cyrillic direct Unicode key layouts in headless/offscreen Linux; desktop X11/Wayland/Windows/macOS display servers provide OS dead-key/IME composition).

---

## 6. Visual Artifacts on Google Drive & Factual Visual Inspection

Per `WOTR-D-040` and `docs/agents/drive.md`, binary PNG artifacts (`< 262,144` bytes each) are stored in `Arena/wotr-remaster/artifacts/` (folder ID `1bS29V1MzysKdktDtVEGiVfHmTaO4iRbG`) rather than Git (`tools/release/public_binaries.txt` is Warden-owned). Every PNG listed below was opened and visually inspected before submission.

| Artifact Name (`Arena/wotr-remaster/artifacts/`) | Drive File ID | Resolution & Size | SHA-256 | Factual Visual Inspection |
| --- | --- | --- | --- | --- |
| `WOTR-1.1__K5__wotr-world-en__2026-11-24.png` | `12Gn0IJsVk01O_oZU4d3bi3JHGm-I2Kde` | `1280x720` · `49,715 B` | `da49304df08206e065e0d175e090df0b0397d1e9f2951ae84664a59cf44646a4` | Dark slate 3D board with three grey-green hex cells (`west`, `crossing`, `east`); yellow flag (`army_gondor x12 / wotr.unit.knight`) on `west`, red flag (`army_mordor x12 / wotr.unit.orc`) on `east` (3D labels `west`/`east` partially behind flags; `crossing` unobstructed). Top-left dark translucent HUD panel displays 5 crisp white readout lines (`state 29689fc52edb`) and buttons `Move` (disabled grey), `Save`, `Load` with generous horizontal padding. |
| `WOTR-1.1__K5__wotr-world-ru__2026-11-24.png` | `1r6a1b3vPWis3RdekYnG0Ao_tTp9X3fXl` | `1280x720` · `51,366 B` | `e90a589c9861380d51bdc7b790604a599a0efe7e54990bd62fd1f5352b277240` | Identical 3D board and state `29689fc52edb`. Top-left HUD buttons render crisp Cyrillic `Переместить`, `Сохранить`, `Загрузить` with zero clipping, zero `.notdef` glyphs, and ~150 px of remaining right-hand space inside the 460 px HUD panel. |
| `WOTR-1.1__K5__wotr-world-de__2026-11-24.png` | `11KBGzSL88TyOXoBsFU8V5eXjuUUDQUZC` | `1280x720` · `51,075 B` | `43b4a5667240ae40665e54900cac9ce906a59b45c18eac379e09cf3183f11701` | Identical 3D board and state `29689fc52edb`. HUD buttons render `Bewegen`, `Speichern`, `Laden` centered in 96x32 px buttons with no truncation or overlap. |
| `WOTR-1.1__K5__wotr-world-fr__2026-11-24.png` | `1bNmElNTnM9Xg9jIBuD0CRAdcD1gyIOB4` | `1280x720` · `50,935 B` | `53186809dfe00f52b028efe4efb1cb39bb9e41e57a4f57b3dba6c6ad42c6c2e2` | Identical 3D board and state `29689fc52edb`. HUD buttons render `Déplacer` (with clear acute accent on `é`), `Enregistrer`, `Charger` cleanly inside their button boxes. |
| `WOTR-1.1__K5__wotr-world-es__2026-11-24.png` | `1L8rBINza7alU6SNaa_Pm7qLpvOvlLv80` | `1280x720` · `50,196 B` | `ced71c3ae5513ba54507cb38a4179776b49a55082e7703c69e95462b227681c3` | Identical 3D board and state `29689fc52edb`. HUD buttons render Spanish `Mover`, `Guardar`, `Cargar` centered with ample margins. |
| `WOTR-1.1__K5__wotr-world-it__2026-11-24.png` | `1IshRw-6c4pzvfm1UHlnGZn10V2lU6_Xe` | `1280x720` · `49,631 B` | `456ac3276733359b4a53ce1dd867f7b3eeb67b48b4af010ddcb9849892408161` | Identical 3D board and state `29689fc52edb`. HUD buttons render Italian `Muovi`, `Salva`, `Carica` centered with ample margins. |
| `WOTR-1.1__K5__launcher-settings-en__2026-11-24.png` | `1q1xmLRSVrV5zO3934hEcSXKYE89XUtPa` | `1280x800` · `237,689 B` | `1f9213066d2cb612801f8bb5a9562aa712e636ff56bfe10902bc74f810a31d62` | Dark gold/amber smoky background, left sidebar (`Play`, `News`, `Multiplayer`, `Mods`, `Game files`, active `Settings`), gold serif `SETTINGS` heading, tabs `General` / `Display` / `About & logs`, bordered card with 6 toggle/selector rows (`Channel`, `Free camera`, `Update automatically...`, `Show how many players...`, `Offer to download...`, `Open .replay...`) plus `Language` (`Automatic (English)`), bottom bar `Up to date`, `Check for updates`, and gold `PLAY` button. Zero clipping or overlap. |
| `WOTR-1.1__K5__launcher-settings-ru__2026-11-24.png` | `1IUyF8pyjYOWptFX876vvxAPpCg3sMmdT` | `1280x800` · `241,264 B` | `100e0f6a4cdc7cb04864169f780e666e9fbefc2916b97c17dcf1cc26a50813a9` | Sidebar (`Играть`, `Новости`, `Сетевая игра`, `Моды`, `Файлы игры`, `Настройки`), tabs (`Общие`, `Экран`, `О программе и логи`), translated rows (`Канал`, `Свободная камера`, `Автоматически обновлять в фоне`, `Показывать число игроков в сети`, `Предлагать загрузку файлов игр`, `Язык` -> `Автоматически (English)` with `ё` and guillemets `«Автоматически»`), bottom bar `Актуальная версия`, `Проверить обновления`, `ИГРАТЬ`. Untranslated `online_count_note` and `Open .replay...` row fall back cleanly per-message to English. `Настройки` heading and `ИГРАТЬ` render in bold sans-serif fallback font (legible, 0 `.notdef` boxes). |
| `WOTR-1.1__K5__launcher-settings-de__2026-11-24.png` | `1hZlLqYhpuF53ADOBMqCnCNN6_TYZlZc1` | `1280x800` · `240,557 B` | `89c1bc846cebcea617a2467e42e5438bbfd596c6f28fef2cfbc10b1a259c1903` | Gold serif `EINSTELLUNGEN`, sidebar (`Spielen`, `Neuigkeiten`, `Mehrspieler`, `Mods`, `Spieldateien`, `Einstellungen`), tabs (`Allgemein`, `Anzeige`, `Info & Protokolle`), German rows (`Kanal`, `Freie Kamera`, `Im Hintergrund automatisch aktualisieren`, `Anzeigen, wie viele Spieler online sind`, `Anbieten, die Spiele herunterzuladen`, `Sprache` -> `Automatisch (English)`), English fallback on `online_count_note` and `replay_association*`, bottom `Aktuell`, `Nach Updates suchen`, `SPIELEN`. Zero clipping. |
| `WOTR-1.1__K5__launcher-settings-fr__2026-11-24.png` | `1TEya_FTB980ER3aUmIomoctGAIQZlRan` | `1280x800` · `240,642 B` | `1f32a00cb8619e31e1daae3ba5518b134cf9c8f6a856ada2e3baf6df3c3acab9` | Gold serif `PARAMÈTRES` (crisp `È`), sidebar (`Jouer`, `Actualités`, `Multijoueur`, `Mods`, `Fichiers du jeu`, `Paramètres`), tabs (`Général`, `Affichage`, `À propos et journaux`), French rows (`Canal`, `Caméra libre`, `Mettre à jour automatiquement en arrière-plan`, `Afficher le nombre de joueurs en ligne`, `Proposer le téléchargement des jeux`, `Langue` -> `Automatique (English)`), English fallback on `online_count_note` and `replay_association*`, bottom `À jour`, `Rechercher des mises à jour`, `JOUER`. Zero clipping. |
| `WOTR-1.1__K5__launcher-settings-es__2026-11-24.png` | `1jPw8l8oOkXM28bBDM3x3optnoGEOOCs5` | `1280x800` · `241,267 B` | `337685544ae367bad26651e1094b241b321e6df0acebfcb92af875566a51df5e` | Gold serif `CONFIGURACIÓN` (crisp `Ó`), sidebar (`Jugar`, `Noticias`, `Multijugador`, `Mods`, `Archivos del juego`, `Configuración`), tabs (`General`, `Pantalla`, `Acerca de y registros`), Spanish rows (`Canal`, `Cámara libre`, `Actualizar automáticamente en segundo plano`, `Mostrar cuántos jugadores hay en línea`, `Ofrecer la descarga de los juegos`, `Idioma` -> `Automático (English)` with crisp `á`, `é`, `í`, `ó`, `ú`), English fallback on `online_count_note` and `replay_association*`, bottom `Actualizado`, `Buscar actualizaciones`, `JUGAR`. Zero clipping. |
| `WOTR-1.1__K5__launcher-settings-it__2026-11-24.png` | `1l6sJln62AHPS8SFl-ZaScEu1CHwEogpf` | `1280x800` · `240,377 B` | `2010b8ae7f2203f688616a478ba6d6da51f76241210fe5ea96cb1d6dc695dee7` | Gold serif `IMPOSTAZIONI`, sidebar (`Gioca`, `Novità`, `Multigiocatore`, `Mod`, `File di gioco`, `Impostazioni`), tabs (`Generali`, `Schermo`, `Informazioni e log`), Italian rows (`Canale`, `Visuale libera`, `Aggiorna automaticamente in background`, `Mostra quanti giocatori sono online`, `Offri il download dei giochi`, `Lingua` -> `Automatico (English)`), English fallback on `online_count_note` and `replay_association*`, bottom `Aggiornato`, `Controlla aggiornamenti`, `GIOCA`. Zero clipping. |
| `WOTR-1.1__K5__launcher-dropdown-es__2026-11-24.png` | `1utnpFWOg7Fj-yRRAzBtPQDOcWTJe9gxi` | `1280x800` · `243,706 B` | `e1b9fbcd8bac06f9a17a5e9474319e459143302f33ce3b8ccfd06b48723a2914` | Spanish Settings screen with the Language `PopupMenu` open over the bottom edge: `Automático (English)` (selected radio dot), `English`, `Deutsch (traducción incompleta)`, `Español (traducción incompleta)`, `Français (traducción incompleta)`, `Italiano (traducción incompleta)`, `Português (Brasil) (traducción incompleta)`, and `Русский (traducción incompleta)` all render crisply; the 3 parked `sample` CJK rows show hex-codepoint fallback boxes on their CJK characters followed by legible `(traducción incompleta)`. |
| `WOTR-1.1__K5__launcher-long-de__2026-11-24.png` | `1JI_cNKdybgPlO1uhxgjXup_G-Y159nRu` | `1280x800` · `226,789 B` | `8d1e4c9466a8e805f3d0dfc4cca25fbbda5a6a111e859db9122137c58d263780` | Play screen in `de` under `long` stress state: 2-line wrapped top warning banner, italic subtitle `The Rise of the Witch-king, neu gebaut`, green online pill `10000000 Spieler online` with zero overlap, `DEINE SPIELE` card (`Wiederholung öffnen...`, `Spielstand öffnen...`), parchment `NEUIGKEITEN` card, bottom progress bar with ellipsis-clipped `Ka...`, `Pause`, `Abbrechen`, `SPIELEN`. |
| `WOTR-1.1__K5__launcher-long-ru__2026-11-24.png` | `1fQdOmDbqf_W6GFO9nKGb5QyI-BHMQvc9` | `1280x800` · `223,723 B` | `51c5cefa00809b04f7f16d99b2f7888be8479f1ce47643e2cf1212c96eb376ef` | Play screen in `ru` under `long` stress state: crisp italic Cyrillic `пересобранная` in `CormorantGaramond-Italic`, green pill `10000000 игроков в сети` (Russian `many` CLDR plural), `ВАШИ ИГРЫ` (`Открыть повтор...`, `Открыть сохранение...`), parchment `Что нового`, bottom `Пауза`, `Отмена`, `ИГРАТЬ`. Zero overlap or overflow. |
| `WOTR-1.1__K5__launcher-long-es__2026-11-24.png` | `1ySbay0mgcqpSpXiw-qZCom5mgGlmSeNb` | `1280x800` · `225,986 B` | `d94b1e04ef01e56afe10c14eb5c5de81126f55c9d2280b3a7f23fab1055f746a` | Play screen in `es` under `long` stress state: italic `The Rise of the Witch-king, reconstruido`, green pill `10000000 jugadores en línea` (Spanish `other` plural with `í`), `TUS JUEGOS` (`Abrir repetición...`, `Abrir partida guardada...`), parchment `NOVEDADES`, bottom `Pausar`, `Cancelar`, `JUGAR`. Zero overlap or overflow. |
| `WOTR-1.1__K5__launcher-input-es-it__2026-11-24.png` | `1Vir8AT7xgMZHV6Uv_L9hBeTMyc99C_h1` | `1280x800` · `245,109 B` | `8d127d82df06bd800f1ed5ba96b54c15d0a23fb13f460aaed640c2639bce17fd` | Multiplayer screen (`Multijugador` selected in sidebar, English fallback on untranslated `MULTIPLAYER` body strings) with focused gold-bordered `LineEdit` displaying typed string `Señor Città d'Été 42` and vertical caret after `2`; all diacritics (`ñ`, `à`, `É`, `é`) crisp in `Inter.ttf`. |
| `WOTR-1.1__K5__launcher-input-ru__2026-11-24.png` | `1u2TRBgVxr9EoaD8w5JMkuhcWWVwpSW8F` | `1280x800` · `245,246 B` | `a4d25ef53d505758d064395ef397091c367468ff7c2d4381cd0c701c3af4a165` | Multiplayer screen (`Сетевая игра` selected in sidebar) with focused gold-bordered `LineEdit` displaying typed Cyrillic + numero sign `Средиземье Ход №5` and vertical caret after `5`; crisp rendering in `Inter.ttf` with zero clipping. |

---

## 7. Verification & Cost-Gate Execution (`build: full`, `retail: none`)

### Executed commands and results

| Check | Command | Exit Code | Result |
| --- | --- | --- | --- |
| Catalog generation & staleness gate | `python3 tools/l10n/l10n.py generate && python3 tools/l10n/l10n.py check` | `0` | `ok` (0 validation, usage, unused, staleness, required-locale, or lint problems) |
| Catalog coverage report | `python3 tools/l10n/l10n.py coverage` | `0` | `de/es/fr/it/ru`: `53/1301` (`partial`); `pt-BR`: `12/1301` (`partial`); `ja/ko/zh-Hans`: `12/1301` (`sample`) |
| Python + headless Godot L10N test suite | `GODOT=/tmp/godotbin/godot python3 -m pytest -v tools/l10n/test_l10n.py` | `0` | `34 passed` (including `launcher/tests/l10n_test.gd` normal + `--errors` misuse checks) |
| Launcher layout & GDScript suites | `launcher/tests/layout_test.gd` + `pytest tools/test_gdscript.py tools/release/test_launcher_tools.py` | `0` | `LAYOUT OK 29 cases x 9 sizes, 23157 controls measured`; `21 passed` (`test_gdscript.py`); `62 passed` (`test_launcher_tools.py`) |
| Full C++ engine + `openbfme_tests` + `wotr_core` build | `JOBS=2 bash tools/arena/build_engine.sh --tests && JOBS=2 bash godot/wotr_remaster/core/native/build_core_extension.sh` | `0` | Built `openbfme.linux.template_debug.x86_64.so`, `openbfme_tests`, and `libwotr_core.so` |
| C++ `OpenBFMEText` & `WotrRemaster` tests | `./engine/build/openbfme_tests --source-file="*test_openbfme_text.cpp"` and `--source-file="*wotr*"` | `0` | `4 passed` (`63` assertions) and `102 passed` (`2059` assertions) |
| Full `openbfme_tests` suite (68 `--source-file` batches across all 674 test `.cpp` files) | `./engine/build/openbfme_tests --source-file=<10-file chunks>` (excluding 5 host-tool MP3 cases below) | `0` | `5646 passed, 0 failed` (`1,409,703` assertions passed, `0` failed) |
| WOTR-1.1 scenario + 6-locale x 2-repeat hash invariance | `bash godot/wotr_remaster/scenario/run_scenario.sh` + 12-run 6-locale harness | `0` | `WOTR-K4 PASS` + `12/12` locale runs bit-for-bit hash-identical to English |
| Repository hygiene & audit gates | `python3 tools/precommit.py`, `python3 tools/release/pr_audit.py --tree HEAD^{tree} --base origin/main`, `git diff --check` | `0` | Clean (verified on final commit tree) |

### Explicit `not run` / skipped items (`cost-gates.md` honesty rule)

| Suite / Case | Status | Reason |
| --- | --- | --- |
| Retail-gated engine tests (`ROTWK_INSTALL` / `BFME2_INSTALL`) | `not run` (skipped cleanly by runner) | Card cost gate is `retail: none`; no retail `.big` archives are mounted in this sandbox. |
| Five MP3 golden-fixture cases in `engine/tests/test_audio_decode.cpp` | `not run` (`--test-case-exclude=*MP3*`) | Requires host encoders `lame` (3.100) and `mpg123` to synthesize `/engine/build/audio_fixtures/*.mp3.bin`, which are absent in the rootless Arena sandbox (documented in `WOTR-D-031` and `K4/report.md`; CI runs these in the dedicated `fixtures` job). All non-MP3 `test_audio_decode.cpp` cases ran and passed. |
| OS-native CJK/dead-key IME composition window | `not run` (direct Unicode `InputEventKey` verified) | `DisplayServerOffscreen` reports `FEATURE_IME = false`; CJK locales (`zh-Hans`, `ja`, `ko`) are parked as `"sample"` out of the product tier per `ADR-003` / `WOTR-D-032`. |
