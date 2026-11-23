# ADR-003: Product locale baseline and localization ownership

- **Status:** Accepted; locale set amended 2026-11-23
- **Date:** 2026-11-20 (amended by `WOTR-D-032`)
- **Decision:** `WOTR-D-013` as amended by `WOTR-D-032`
- **Reference:** `Creative36711/synapse@83955634d1c6d87d505a676ace3b5cfffa8b3e75`

## Context

OpenBFME already has a strict source catalog (`tools/l10n/catalog/en.json`), generated catalogs for Godot/C++, placeholder/plural validation, per-message English fallback and coverage reports. Before this decision it exposed only English and a small German sample. The related Synapse product establishes eight production locales: English, Russian, Simplified Chinese, Japanese, Brazilian Portuguese, German, French and Korean.

Simulation, save/network hashes and content IDs must not depend on display language. A locale appearing in a selector without meaningful coverage is not production support.

## Decision

The required product locale set is:

```text
product (Phases 1-3, mandatory):   en, ru, de, fr, es, it
community (from Phase 6, optional): pl, sv, tr, da, no, pt-BR
excluded from the plan:             zh-Hans, ja, ko
```

These are BCP-47 product locale tags. Do not collapse `pt-BR` to generic `pt`, `zh-Hans` to generic `zh` or a Norwegian choice to the macrolanguage `no` silently: `no` is the tag the existing retail alias table already emits, and it stays exact in catalogs, settings, saves, telemetry and pack metadata. Runtime plural/number rules may share language-family implementation internally, but the public tag remains exact.

Removing a locale from the required set is a scope decision, not a gate change: every locale that remains in `product` still needs 100% coverage, native/cultural review and the rendering/font/input gate, and an out-of-plan locale may not ship as a silently partial product claim. Bringing `zh-Hans`, `ja` or `ko` back requires a new `WOTR-D-*` record.

Until the `Localization & Cultural QA Engineer` card lands, `tools/l10n/l10n.py` keeps validating the previous eight-locale set; the oracle stays stricter than the plan, and no check is disabled in the meantime.

`en` is the source. Each of the other five product catalogs must exist from the card that lands this amendment (`es` and `it` are new; `de`, `fr`, `ru` already exist), may initially be `partial`, and falls back per message to English. The three CJK catalogs stay in the tree without a product claim until the same card records their disposition (retain as `sample` or delete), so the decision is reversible by data and not by rewriting history. A release may call a locale **production-supported** only when the catalog is complete for that release and the localization acceptance gate passes. Until then the UI labels it clearly as incomplete; no locale is silently omitted from planning or coverage.

All player-facing OpenBFME and WOTR Remaster text uses stable catalog IDs. Locale-dependent values are presentation-only and may not enter canonical state, RNG, command ordering, `BattleRequest`, `BattleResult`, pack identity or save/network hashes. Campaign Packs localize names/descriptions by ID with the same English fallback contract.

## Ownership and independent audit

Add **Localization & Cultural QA Engineer**, an Executor specialization. It owns catalog/runtime/font/input work and locale evidence; it does not self-accept. The UX & Modding Auditor independently verifies completeness, fallback visibility, clipping/layout, font coverage, shaping and line breaking for the scripts in scope, pseudolocalization and Campaign Pack fallback.

## Acceptance gate per production locale

1. catalog validation and generated-file freshness;
2. 100% source-ID coverage with placeholder/plural parity and no stale IDs;
3. native-language editorial/cultural review recorded in evidence (machine translation alone is insufficient);
4. headless locale smoke plus opened screenshots at target scale for launcher, strategic map/HUD, battle notices, save/load errors and Campaign Pack validation;
5. font/glyph coverage, shaping and input/IME smoke where applicable;
6. long-text/pseudolocale layout and English fallback checks;
7. deterministic test showing locale switches do not change canonical state/save hash;
8. independent UX & Modding Auditor pass.

## Consequences

- Locale support becomes a release-quality obligation rather than a sample UI path.
- `es` and `it` need new catalogs plus generated-file regeneration; their plural categories and decimal-comma entries already exist in all three runtimes. `tr` and `da` exist in neither `PLURAL_CATEGORIES` nor `DECIMAL_COMMA`, and no retail `lang/<Name>.big` alias maps to them, so their automatic-language path can only ever be a manual pick; that work is the Phase 6 card and touches C++ (`OpenBFMEText.cpp`), so it carries a build gate.
- The retail aliases `chinese`/`korean` keep resolving to exact tags while their catalogs exist, but they are no longer product obligations; `brazilian`/`portuguese` -> `pt-BR` moves to the community tier.
- Translation can proceed incrementally, but production claims cannot.
- Locale-specific artwork or copy still needs provenance/license and cultural review.
