# Последние наблюдения закрыты: контракт AI & Simulation Engineer и имя `AI/**`

Приёмка (Warden) · 2026-11-23 · ветка `arena/9f95e1e2-open-bfme` · решение `WOTR-D-042`.

Повод: «доброкачественные наблюдения» из ответа по аудиту `WOTR-D-041` — отсутствие контракта
`ai-simulation-engineer.md` и provisional-статус поддерева `AI/**`. Оба сняты.

## Сделано

1. **`docs/agents/roles/ai-simulation-engineer.md`** — контракт роли (структура — как у
   `localization-cultural-qa-engineer.md`): миссия по `docs/PHASES.md` (базовый AI/autoresolve/
   20-turn simulation — фаза 2, advanced planning AI — фаза 8), зона записи, обязанности
   детерминизма (ordinary commands only, именованный seeded RNG, fixed-seed repeatability,
   no hidden mutation; autoresolve с явной причиной — REGULATION §12), гейт и «не принимает
   собственную работу».
2. **Имя `AI/**` закреплено, provisional-лимбо снято.** Контракт + строка «Роли» фиксируют
   `engine/src/GameLogic/WotrRemaster/AI/**` и `engine/tests/WotrRemaster/AI/**`; ADR Strategic
   MVP (`WOTR-D-037`) обязан их придерживаться, переименование — только явной правкой строки и
   контракта по `R-ZONES`. «Гейт» Notion-строки AI & Simulation Engineer и ячейка Primary write
   zone в `docs/agents/roles/specializations.md` приведены к тому же тексту (Notion —
   2026-11-24T07:31Z).

## Итог по реестру

Открытых наблюдений по зонам и контрактам не остаётся: все 14 строк «Роли» — glob-списки,
обязательные пути покрыты, контракты критичных ролей на месте, журнал `WOTR-D-039…042`
непротиворечив. Следующая смена имени `AI/**` — событие явной правки, а не ожидаемый штатный шаг.
