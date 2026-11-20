# War Of The Ring: Remaster · фазы 1–8

Редакция 1.0 · 2026-11-20 · владелец: Warden.

Источник статуса исполнения — база Notion «Миссии»; этот файл фиксирует продуктовую последовательность и опубликованную редакцию. Фаза — рамка результата, миссия — принимаемая работа внутри неё. В каждой фазе есть минимум одна миссия.

## Общие гейты

Любая фаза закрывается только когда:

1. все обязательные миссии приняты Warden;
2. независимые аудиторы воспроизвели оракул на принятом SHA;
3. save/schema migration и deterministic hash не имеют незакрытой регрессии;
4. retail-dependent критерии проверены в Arena на реальных BFME2 1.06 + RotWK 2.01 данных;
5. документация, Campaign Pack schema и upstream pin соответствуют коду.

## Фаза 1 · Technical walking skeleton

**Цель:** доказать вертикальный путь отдельного ядра через реальный tactical runtime.

### WOTR-1.1 · Контракты и детерминированный walking skeleton

`Campaign Pack → карта → WEGO move → conflict → BattleRequest → OpenBFME tactical map → BattleResult → campaign state → save/load`.

Выход: маленький synthetic Campaign Pack, отдельный `WotrRemaster` core, canonical state/hash/save, Godot map interaction, native adapter и один полный retail round-trip в Arena. Tactical profile: Vanilla RotWK 2.01. Подробное ТЗ: `docs/missions/WOTR-1.1.md`.

## Фаза 2 · Campaign Pack и редактор

**Цель:** сделать стратегический мир полностью data-driven и редактируемым.

### WOTR-2.1 · Campaign Pack v1 и editor loop

Schema/versioning, topology and hex presentation data, factions/regions/armies, asset references, validation diagnostics, migrations, undo/redo, import/export and reopen-identical hash. Vanilla sample pack и custom-asset sample проходят один validator.

## Фаза 3 · Strategic MVP

**Цель:** законченный sandbox-first цикл кампании с простым AI.

### WOTR-3.1 · WEGO strategic MVP

Turn planning/resolution, movement, conflicts, economy/recruitment baseline, fog/visibility baseline, simple command-producing AI, autoresolve baseline, campaign victory/defeat and repeatable 20-turn simulation. Narrative content не является условием MVP.

## Фаза 4 · Isengard–Rohan–Gondor visual slice

**Цель:** одна качественная вертикальная область Средиземья.

### WOTR-4.1 · Трёхфракционный visual slice

2.5D terrain and landmarks, readable hex/region overlays, strategic armies, weather/time presentation, polished HUD, editor workflow and tactical transitions for Isengard, Rohan and Gondor. Visual/performance budgets проверяются на целевом профиле.

## Фаза 5 · Full Middle-earth

**Цель:** масштабировать проверенный slice до полной карты и основного состава сторон.

### WOTR-5.1 · Middle-earth campaign pack

Полная topology, основные factions, стартовые сценарии, map art/LOD/streaming, regional encounters and balanced strategic data. Content validation и deterministic simulation остаются зелёными на полном объёме.

## Фаза 6 · Mods и custom content

**Цель:** публичный, безопасный и воспроизводимый content pipeline.

### WOTR-6.1 · Mod-aware Campaign Packs

Profile inheritance/overrides, dependency and conflict diagnostics, custom asset provenance/licensing, pack discovery, deterministic load order and compatibility matrix. Vanilla RotWK 2.01 не меняется при установке mod pack.

## Фаза 7 · Advanced systems и narrative/hybrid

**Цель:** глубина кампании поверх стабильного ядра.

### WOTR-7.1 · Events, diplomacy and hybrid campaigns

Versioned event system, quests/objectives, diplomacy, heroes and persistent consequences, narrative/hybrid layers, authoring support and replay-safe event decisions. Контент остаётся данными; core не ветвится под один сценарий.

## Фаза 8 · Advanced AI и multiplayer

**Цель:** сложный AI и сетевой strategic режим после зрелого determinism/save.

### WOTR-8.1 · Lockstep strategy, reconnect and advanced AI

Advanced planning AI, deterministic lockstep command stream, desync detection, replay, save/resume/reconnect, host migration policy and mixed strategic/tactical multiplayer compatibility. Ни один сетевой путь не обходит обычную command validation.

## Активность

Активная фаза: **1**. Первый принимаемый результат: **WOTR-1.1**. Остальные миссии остаются черновиками до прохождения зависимостей своей фазы.
