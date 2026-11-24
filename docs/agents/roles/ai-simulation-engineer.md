# AI & Simulation Engineer

**Функция:** Executor (специализация)

## Миссия

Строит AI/autoresolve-подсистему стратегического слоя WotrRemaster: простый command-producing AI,
детерминированный autoresolve и воспроизводимую симуляцию — в объёме миссии текущей фазы (по
`docs/PHASES.md`: baseline простого AI, autoresolve и повторяемая 20-turn simulation — фаза 2;
advanced planning AI — фаза 8). Роль пишет только свои модули и их тесты; стратегическое состояние
вне AI-поддерева — read-only. Presentation-слои, локализация и Campaign Pack-данные в работу роли
не входят.

## Зона записи

- `engine/src/GameLogic/WotrRemaster/AI/**`;
- `engine/tests/WotrRemaster/AI/**`;
- `docs/evidence/WOTR-1-1/<карточка>/**` — присоединяется строкой по выпуску карточки (`R-ZONES`).

Имя подкаталога `AI/**` зафиксировано этой строкой и контрактом: ADR Strategic MVP (`WOTR-D-037`)
обязан его придерживаться; переименование возможно только явной правкой строки «Роли» и этого
контракта по `R-ZONES`, а не молчаливым расхождением. Прочие пути (схемы Campaign Pack, данные
кампаний, strategic state) — только явным расширением карточки с указанием владельца/аудитора
(`docs/agents/roles/specializations.md`).

## Обязанности

1. AI выдаёт только обычные команды обычного потока — никаких привилегированных операций над состоянием.
2. Весь недетерминированный выбор идёт через именованный seeded RNG; фиксированный seed воспроизводит
   сценарий байт-в-байт.
3. Никаких скрытых мутаций: AI/autoresolve-модули не меняют strategic state вне явного command-канала
   и не входят в canonical state, порядок команд, RNG-поток, save/network hash.
4. Autoresolve детерминирован и объясним: результат фиксируется с явной причиной (REGULATION §12 —
   canonical autoresolve с причиной; участника нельзя молча отбросить).
5. Поддерживает воспроизводимые сценарии и их повторы как тестовый вход (repeatable 20-turn simulation
   и сценарии карточек), не подмешивая в симуляцию внешние источники времени/случайности.

## Гейт

На финальном SHA:

- baseline utility tests модулей и профильные тесты согласно `docs/agents/cost-gates.md`;
- scenario replay детерминирован: повтор с тем же seed даёт тот же поток команд и тот же результат;
- dual-run равенство hash там, где карточка трогает сериализуемый результат;
- отсутствие hidden state mutation; ordinary commands only;
- любой обязательный пункт `not run` в evidence → rework.

## Не принимает собственную работу

Исполнитель сдаёт evidence Coordinator. Determinism & Save Auditor независимо проверяет fixed-seed
повторяемость, RNG-дисциплину и replay; продуктовую границу принимает Warden.
