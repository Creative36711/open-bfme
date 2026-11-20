# WOTR role specializations

All engineering specializations inherit `executor.md`; all audits inherit `auditor.md`.

| Role | Primary write zone | Card gate |
|---|---|---|
| Strategic Core Engineer | `engine/src/GameLogic/WotrRemaster/**`, focused tests | canonical serialization, dual-run equal hash, save/load and independent determinism audit |
| Tactical Bridge Engineer | WOTR adapter and named OpenBFME integration points | synthetic mapping plus real retail tactical round-trip; every persistent field accounted for |
| Godot World Engineer | `godot/wotr_remaster/world/**` | headless interaction, opened screenshot/video, frame/memory facts |
| Campaign Editor Engineer | `godot/wotr_remaster/editor/**`, campaign schemas/migrations | validation, undo/redo, export/reopen identical hash and malformed input failures |
| Technical Artist | `art/**`, assigned presentation assets | provenance/license/hash, opened visual, retail/custom references and budget |
| AI & Simulation Engineer | WOTR AI/autoresolve modules | ordinary commands only, named RNG, fixed-seed repeatability, no hidden mutation |
| Localization & Cultural QA Engineer | `tools/l10n/**`, assigned localization runtime/font/input paths | required-locale coverage, native review, rendering/font/input smoke and locale-invariant hash |
| Upstream Integrator | `OPENBFME_UPSTREAM`, sync report/tooling and named conflicts | range/conflict report, full build/regressions and native retail round-trip |
| Integration Auditor | isolated tests/evidence | clean-checkout Arena strategic→tactical→result→save proof |
| Determinism & Save Auditor | test fixtures/evidence | independent dual run, save/load/replay, corruption/migration and RNG audit |
| UX & Modding Auditor | UX/mod compatibility tests/evidence | readability/accessibility, pack validation, Vanilla isolation and no retail copies |

A card may narrow these paths. It may widen them only explicitly and must name the owner/auditor affected.
