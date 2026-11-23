# War Of The Ring: Remaster · фазы 1–8

Редакция 1.3 · 2026-11-23 · владелец: Warden.

Изменения редакция 1.3 (аддитивные, `WOTR-D-034`…`WOTR-D-037`): концептуальные решения по редактору, режимам,
героям, крепостям, таксономии сущностей и стратегическим механикам global map вписаны в фазы 2–8. Текст фазы 1,
оракулы и инварианты активной миссии не изменены. Полный разбор: `docs/handoff/2026-11-23-warden-concept-review.md`.
Редакция 1.2 сохранена в истории Git.

Источник статуса исполнения — база Notion «Миссии»; этот файл фиксирует продуктовую последовательность и опубликованную редакцию. Фаза — рамка результата, миссия — принимаемая работа внутри неё. В каждой фазе есть минимум одна миссия.

## Общие гейты

Любая фаза закрывается только когда:

1. все обязательные миссии приняты Warden;
2. независимые аудиторы воспроизвели оракул на принятом SHA;
3. save/schema migration и deterministic hash не имеют незакрытой регрессии;
4. retail-dependent критерии проверены в Arena на реальных BFME2 1.06 + RotWK 2.01 данных;
5. заявленный Cost gate соответствует `docs/agents/cost-gates.md`; full build/Drive hydration не запускаются для несвязанных карточек;
6. обязательные локали product-набора `en`, `ru`, `de`, `fr`, `es`, `it` имеют coverage report, а локаль не меняет canonical hash; community-набор (`pl`, `sv`, `tr`, `da`, `no`, `pt-BR`) проверяется с фазы 6 и не заявляется как production-supported (`WOTR-D-032`);
7. документация, Campaign Pack schema и upstream pin соответствуют коду.

## Фаза 0 · Baseline decisions (решения приняты; history sanitation открыта)

Приняты ADR-001…004: Godot-native boundary; перенос legacy world через one-way importer без legacy runtime; tactical capacity по opposed sides с fallback/autoresolve; custom maps как лицензированные content-addressed overlays; eight-locale baseline; адаптация существующего launcher/one-archive pipeline с отдельной WOTR identity; tiered build/retail cost policy; Upstream Integrator как Executor. Полный legacy dataset и `__wotr_maps.big` остаются заблокированы license/provenance gate. Retail history всё ещё содержит запрещённые объекты: cleanup выполняется отдельно как destructive owner-approved repository operation, не блокирует проектирование, но public release workflow остаётся заблокирован до fresh-clone all-ref audit rc 0.

## Фаза 1 · Technical walking skeleton

**Цель:** доказать вертикальный путь отдельного ядра через реальный tactical runtime.

### WOTR-1.1 · Контракты и детерминированный walking skeleton

`Campaign Pack → карта → WEGO move → conflict → BattleRequest → OpenBFME tactical map → BattleResult → campaign state → save/load`.

Выход: маленький synthetic Campaign Pack, отдельный `WotrRemaster` core, canonical state/hash/save, Godot map interaction, native adapter и один полный retail round-trip в Arena. Tactical profile: Vanilla RotWK 2.01. Подробное ТЗ: `docs/missions/WOTR-1.1.md`.

### WOTR-1.2 · Player entry and portable playtest package

Зависит от принятой WOTR-1.1, но снятие этой зависимости само по себе не запускает подготовку: для старта WOTR-1.2 требуется отдельное будущее решение. Существующий OpenBFME launcher и one-archive pipeline адаптируются как WOTR product flavor: один распаковываемый архив на заявленную платформу, WOTR identity/route, отдельные update data/settings/saves, retail detection, offline start, signed updates и fail-closed package audit. Стратегия и tactical runtime остаются одним native game process; новый updater не создаётся. После отдельного решения о подготовке первой карточкой должна стать B0-репетиция оракула. Подробное ТЗ: `docs/missions/WOTR-1.2.md`.

Выход: первый внутренний playtest, который обычный тестер запускает через **Play WOTR Remaster** без checkout, build tools, environment variables или developer CLI. Public release остаётся заблокирован history/license/signing gates; непроверенная платформа не называется поддерживаемой.

## Фаза 2 · Campaign Pack и редактор

**Цель:** сделать стратегический мир полностью data-driven и редактируемым.

### WOTR-2.1 · Campaign Pack v1 и editor loop

Schema/versioning, topology and hex presentation data, factions/regions/armies, ordered primary/fallback map references, content-addressed map components, validation diagnostics, migrations, undo/redo, import/export and reopen-identical hash. Versioned one-way legacy importer проходит clean-room fixture, stable-ID/canonical-output/provenance-loss tests; full legacy content не публикуется до license gate. Vanilla sample pack и licensed custom-map sample проходят один validator.

Редактор — универсальный редактор данных для всех режимов (`WOTR-D-034`); уровни авторинга (шаблоны → данные
системы → расширение схемы по ADR) и запрет форка ядра фиксируются там же. Перед заморозкой схемы принимается ADR
таксономии стратегических/тактических сущностей (`WOTR-D-036`), включая явный `tactical_template_ref` и запрет
эвристической подстановки тактических шаблонов. Представление героёв (маркер + портрет) и style brief героев и
крепостей — входные решения для редактора и визуала (`WOTR-D-035`).

## Фаза 3 · Strategic MVP

**Цель:** законченный sandbox-first цикл кампании с простым AI.

### WOTR-3.1 · WEGO strategic MVP

Turn planning/resolution, movement, conflicts, economy/recruitment baseline, fog/visibility baseline, simple command-producing AI, autoresolve baseline, campaign victory/defeat and repeatable 20-turn simulation. Conflict setup partitions armies into opposed tactical sides; allies share a start, capacity overflow uses a compatible declared fallback map or canonical autoresolve with explicit reason. Narrative content не является условием MVP.

Режим «Свободная война за кольцо» — цель Strategic MVP (`WOTR-D-035`). Baseline фазы включает механики `WOTR-D-037`:
экономику трёх ресурсов и рекрутирование в поселениях с лимитом в ход, supply-пул армий, туман войны с «последними
сведениями», UI-выбор автобоя vs RTS, перехват движения (две упорядоченных движения-волны в ходу), базовые осадные
бонусы и автобой гарнизона, правила отступления/рассеивания aftermath и уровни героёв. Структура хода — семантика
ядра и фиксируется ADR Strategic MVP, не данными пака.

## Фаза 4 · Isengard–Rohan–Gondor visual slice

**Цель:** одна качественная вертикальная область Средиземья.

### WOTR-4.1 · Трёхфракционный visual slice

2.5D terrain and landmarks, readable hex/region overlays, strategic armies, weather/time presentation, polished HUD, editor workflow and tactical transitions for Isengard, Rohan and Gondor. Importer is exercised on an authorized or clean-room representative slice. Visual/performance budgets and locale screenshots/font coverage are checked on the target profile.

Исторические сценарии — режим фазы: фиксированные силы на retail-картах, доступ из меню без прохождения кампании
(`WOTR-D-035`), аддитивно; итоговый объём фиксируется ТЗ миссии при подготовке. Крепости Tier-1 среза — Хельмова
Падь, Эдорас, Ортант, Минас-Тирит; механика событийной замены моделей по stable ID презентуется в этом срезе, само
событие — контент фазы 7 (`WOTR-D-035`). Засады с бонусом инициативы и блокада поселений — уточнение механик фазы 3
(`WOTR-D-037`), контекст боя через `BattleRequest` расширяется по ADR.

## Фаза 5 · Full Middle-earth

**Цель:** масштабировать проверенный slice до полной карты и основного состава сторон.

### WOTR-5.1 · Middle-earth campaign pack

Полная topology, основные factions, стартовые сценарии, map art/LOD/streaming, regional encounters and balanced strategic data. Legacy-authored dataset enters the distributable pack only with recorded license/provenance authorization; otherwise equivalent clean-room data replaces it. Content validation, locale coverage и deterministic simulation остаются зелёными на полном объёме.

Барад-дур и Минас-Моргул входят в крепости Tier-1 полной карты (`WOTR-D-035`); линии снабжения из пака — уточнение
supply-механики фазы 3 (`WOTR-D-037`).

## Фаза 6 · Mods и custom content

**Цель:** публичный, безопасный и воспроизводимый content pipeline.

### WOTR-6.1 · Mod-aware Campaign Packs

Profile inheritance/overrides, dependency and conflict diagnostics, licensed custom tactical maps with declared starts/capacity and content hashes, custom asset provenance/licensing, pack discovery, deterministic overlay order and compatibility matrix. Vanilla RotWK 2.01 не меняется при установке mod pack.

Здесь же открывается community locale tier (`WOTR-D-032`): `pl`, `sv`, `tr`, `da`, `no`, `pt-BR` добавляются в селектор и в coverage report, production-claim для них запрещён, статус в UI остаётся видимым; `tr` и `da` требуют правил плюрализации и формата чисел во всех трёх рантаймах, то есть карточка с `build: focused` минимум один раз.

## Фаза 7 · Advanced systems и narrative/hybrid

**Цель:** глубина кампании поверх стабильного ядра.

### WOTR-7.1 · Events, diplomacy and hybrid campaigns

Versioned event system, quests/objectives, diplomacy, heroes and persistent consequences, narrative/hybrid layers, authoring support and replay-safe event decisions. Контент остаётся данными; core не ветвится под один сценарий.

Кампания/сюжет — приоритетный режим №1 по идентичности продукта (`WOTR-D-035`); sandbox-кампания существует с фазы 3,
здесь добавляется нарративный слой. Здесь же: механика Кольца Всевластья подсистемой данных и событий (`WOTR-D-037`),
progression героев с вещами/артефактами (расширение контрактов при необходимости — через ADR), действия агентов
(дипломатия, шпионаж) и события смены крепостей (Минас-Итиль → Минас-Моргул, `WOTR-D-035`).

## Фаза 8 · Advanced AI и multiplayer

**Цель:** сложный AI и сетевой strategic режим после зрелого determinism/save.

### WOTR-8.1 · Lockstep strategy, reconnect and advanced AI

Advanced planning AI, deterministic lockstep command stream, desync detection, replay, save/resume/reconnect, host migration policy and mixed strategic/tactical multiplayer compatibility. Ни один сетевой путь не обходит обычную command validation.

Редактор мультиплеера владеет только данными баланса фракций; синхронизация, desync detection и replay-структура —
инфраструктура движка, не контент редактора (`WOTR-D-034`).

## Активность

Активная фаза: **1**. Текущий принимаемый результат: **WOTR-1.1**. `WOTR-1.2` остаётся черновиком без живых карточек в «Запросах», включая B0; её преждевременно созданная B0 архивирована. Сначала Coordinator нарезает реализацию WOTR-1.1, и команда завершает эту миссию. Даже принятие WOTR-1.1 не создаёт новую WOTR-1.2 B0 автоматически: подготовка WOTR-1.2 ждёт отдельного будущего решения. Остальные миссии остаются черновиками до прохождения зависимостей своей фазы и отдельного решения об их подготовке.

Рекомендуемый release milestone после фазы 3 — публичный alpha только при зелёных retail-history, license/provenance, signing и fresh-profile package gates. Это не календарное обещание и не разрешение публикации из ранних фаз.
