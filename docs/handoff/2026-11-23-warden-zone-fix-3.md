# Реестр зон: итоговая зачистка по наблюдениям (tools/release, prose-строки, Transport)

Приёмка (Warden) · 2026-11-23 · ветка `arena/9f95e1e2-open-bfme` · решение `WOTR-D-040`.

Повод: наблюдения из `2026-11-23-warden-zone-fix-2.md` и санкция Мастера «если нужны исправления — делай».
Попутно выполнен полный аудит «Зона записи» всех 14 строк «Роли».

## Сделано (Notion «Роли», 2026-11-24T06:41Z; human-managed поля не тронуты)

| Строка | Было | Стало |
|---|---|---|
| Warden | 6 отдельных `tools/release/*` файлов | `tools/release/**` (одна семья — один владелец, `WOTR-D-022`; итого 17 строк) |
| AI & Simulation Engineer | prose «WotrRemaster AI/autoresolve modules…» | `engine/src/GameLogic/WotrRemaster/AI/**`, `engine/tests/WotrRemaster/AI/**` (имя подкаталога provisional — фиксирует ADR Strategic MVP, `WOTR-D-037`); exclusion перенесён в «Гейт» |
| Campaign Editor Engineer | одна строка «glob, campaign schemas/migrations…» (целиком нечитаема для гейта) | `godot/wotr_remaster/editor/**`; схемы/миграции/валидаторы/паки — строкой по выпуску карточки WOTR-2.1 (R-ZONES) |
| Technical Artist | «art/**, design/**, tools/art/** and …; no retail asset copies.» | `art/**`, `design/**`, `tools/art/**`; назначения и retail-запрет — в «Гейт» |
| Transport Maintainer | open-bfme-эпохальный список (вкл. `docs/**`, `tools/*.mjs`, `WARDEN-SCRIPTCAT-MIGRATION.md`) | 11 путей **relay-wotr** (контекст поля «Репозиторий» = `Creative36711/relay-wotr`): `tools/script-cat/**`, `tools/relay-*.mjs`, `package.json`, `.github/**`, `README.md`, `docs/REGULATION.md`, `docs/regulation-digest.md`, `docs/agents/**`, `docs/DECISIONS.md`, `docs/DECISIONS-UPSTREAM.md`, `docs/scriptcat-update.md` |

## Аудит остальных строк (без правок)

- Localization & Cultural QA Engineer — исправлена ранее (`WOTR-D-039`), glob-список корректен.
- Upstream Integrator, Tactical Bridge Engineer, Strategic Core Engineer, Godot World Engineer — зоны
  соответствуют ТЗ §10 и `WOTR-D-022`/`WOTR-D-023`.
- Аудиторы A1–A3 — по одному glob evidence-пути, соответствует §10.
- Coordinator — зона описана прозой (Notion-базы; «Продуктовый код и Git history — read-only»):
  роль не получает карточек с path-токенами в Scope, для гейта строка неактивна. Оставляю без правки.

## Наблюдения (не тронуты)

1. В репозитории нет контракта `docs/agents/roles/ai-simulation-engineer.md` (у остальных
   специализаций контракты есть); при его появлении зона сверяется с ним.
2. Семейство `engine/cmake/*.cmake` в строке Tactical Bridge (`engine/cmake/WotrRemaster1.cmake`)
   соответствует `WOTR-D-022`; новые лейны заводят свои строки по тому же узору.
