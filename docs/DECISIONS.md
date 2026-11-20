# WOTR Remaster · журнал решений

Этот файл содержит только решения WOTR Remaster. Исторические решения импортированного ScriptCat, нужные его regression fixtures, изолированы в `tools/script-cat/upstream-fixtures/docs/DECISIONS.md` и не определяют продукт.

## WOTR-D-001 · Один репозиторий и ScriptCat как транспорт

**Дата:** 2026-11-20 · **Статус:** принято.

Код продукта, процессные документы, транспорт ScriptCat и upstream pin живут в `Creative36711/open-bfme`. Отдельный orchestration-репозиторий не создаётся. Практики Vault/Synapse применяются через зоны, карточки, независимый аудит, исполнимые оракулы и append-only evidence.

**Откат:** ScriptCat можно выключить, не меняя продукт; перенос управления в другой репозиторий требует нового решения.

## WOTR-D-002 · Отдельное deterministic core

**Дата:** 2026-11-20 · **Статус:** принято.

Стратегическое ядро называется `WotrRemaster` и не зависит от Godot presentation. Retail-faithful OpenBFME `LivingWorldLogic` не является скрытым source of truth нового продукта. Допустимо переиспользовать проверенные tactical DTO/capture pieces через adapter.

**Откат:** удалить route/modules до появления принятых WOTR saves; архитектурная замена после этого требует migration plan.

## WOTR-D-003 · Godot 2.5D + OpenBFME tactical runtime

**Дата:** 2026-11-20 · **Статус:** принято.

Godot реализует 2.5D hex map, HUD и Campaign Pack editor. OpenBFME Godot Nightly запускает тактические бои и предоставляет retail asset runtime. Переход и возврат выполняются нативным versioned `BattleRequest`/`BattleResult`, а не отдельным процессом с потерей состояния.

## WOTR-D-004 · WEGO, sandbox-first и последовательность сложности

**Дата:** 2026-11-20 · **Статус:** принято.

Базовая модель команд — WEGO. Первый продуктовый профиль — sandbox-first; narrative/hybrid добавляется данными после устойчивого ядра. MVP использует простой AI, создающий обычные команды. Advanced AI и multiplayer отложены до фазы 8.

## WOTR-D-005 · Retail и Campaign Packs

**Дата:** 2026-11-20 · **Статус:** принято.

Первый tactical profile — Vanilla RotWK 2.01. Retail BFME2/RotWK читается напрямую и не копируется в продукт. Campaign Packs обязаны поддерживать retail references, licensed custom assets и deterministic mod-aware overrides. BIG transfer — только байт-в-байт с hash; bulk начинается после canary.

## WOTR-D-006 · Фазы 1–8

**Дата:** 2026-11-20 · **Статус:** принято.

Публичный план состоит из фаз 1–8 в `docs/PHASES.md`; каждая содержит минимум одну миссию. Первая миссия `WOTR-1.1` доказывает map → movement → tactical battle → full result → map/save.

## WOTR-D-007 · OpenBFME baseline

**Дата:** 2026-11-20 · **Статус:** принято.

Первый pin — `Open-BFME/openbfme-godot` tag `v0.4.12-nightly.261118100`, commit `4a8d82727c2664c381b7b1c07344881760c4765b`, GPL-3.0. Upstream обновляется только отдельной карточкой с range/conflict report и полным WOTR gate.

## WOTR-D-008 · ScriptCat provenance and local release

**Дата:** 2026-11-20 · **Статус:** принято.

Последний полный historical tree перед удалением Arena-транспорта в Vault — commit `fe8ec9368fc2f5cb9a78a5cdbc47b66e30c07cf6`, parent удаления `eecb68739dbb66a83a6cd9a3d0dbec9650a78ade`. Upstream ScriptCat version — 1.16.1. WOTR-specific prompt/profile/portability adaptation выпускается как 1.16.2, сохраняя MIT license и frozen upstream fixtures для regression evidence.

## WOTR-D-009 · Legacy WOTR boundary

**Дата:** 2026-11-20 · **Статус:** заменено WOTR-D-012.

Механики из `Creative36711/wotr` не импортируются автоматически. Любой перенос начинается с отдельного обсуждения состава, лицензии, compatibility и migration; до решения репозиторий используется только как возможный future reference. Аудит и решение WOTR-D-012 завершили это обсуждение, сохранив запрет runtime dependency и license gate.

## WOTR-D-010 · Upstream Integrator — Executor specialization

**Дата:** 2026-11-20 · **Статус:** принято.

`Upstream Integrator` классифицируется как узкий `Executor`: он реализует конечный pinned sync и сдаёт проверяемый результат, но не содержит постоянно работающий transport. Функция `Maintainer` зарезервирована исключительно за `Transport Maintainer`, который чинит ScriptCat/relay. Upstream Integrator не принимает собственный sync; независимый audit сохраняется.

## WOTR-D-011 · Tiered build и retail cost gates

**Дата:** 2026-11-20 · **Статус:** принято.

Каждая карточка до выдачи объявляет `build: none|focused|full` и `retail: none|reuse|hydrate`. Итерация начинается static/focused проверками. Full engine build обязателен для engine/ABI/upstream/final handoff; retail — только для archive/map/start/tactical round-trip, upstream, release и final mission boundaries. Drive hydration требует явного разрешения, metadata/canary/space checks и reuse verified `.retail`. Нормативная матрица: `docs/agents/cost-gates.md`.

## WOTR-D-012 · Legacy world importer, tactical sides и custom maps

**Дата:** 2026-11-20 · **Статус:** принято · **ADR:** `docs/architecture/ADR-002-LEGACY-WORLD-IMPORT-AND-TACTICAL-CAPACITY.md`.

World/locations/regions/ownership/content из `Creative36711/wotr@fd12e3a3a52ad485f95af3ff559610fd2778570b` переносятся versioned one-way importer со stable IDs, canonical output и provenance/loss report; React/Tauri/Win32/OCR runtime не переносится. Публикация полного dataset и `__wotr_maps.big` ждёт подтверждённой лицензии. Tactical capacity считается по противоборствующим sides, союзники делят start; overflow выбирает первый совместимый declared fallback map либо canonical autoresolve. Новые карты поставляются как лицензированные content-addressed Campaign Pack overlays с starts/profile/hash/provenance.

## WOTR-D-013 · Synapse locale baseline

**Дата:** 2026-11-20 · **Статус:** принято · **ADR:** `docs/architecture/ADR-003-PRODUCT-LOCALES.md`.

Обязательные product locales: `en`, `ru`, `zh-Hans`, `ja`, `pt-BR`, `de`, `fr`, `ko`. Английский — source/per-message fallback; production support требует 100% coverage, native-language/cultural review, rendering/font/input evidence и locale-invariant canonical hash. Добавляется Executor-specialization `Localization & Cultural QA Engineer`; независимую проверку выполняет UX & Modding Auditor.

## WOTR-D-014 · ScriptCat 1.16.3 process/profile refresh

**Дата:** 2026-11-20 · **Статус:** принято.

Без изменения transport protocol ScriptCat обновляется с 1.16.2 до 1.16.3: embedded regulation 1.1, точная роль Upstream Integrator, declared build/retail Cost gate, eight-locale i18n block и bootstrap profile с обязательным чтением cost policy. Source/version/bundle меняются вместе и проходят полный transport gate.
