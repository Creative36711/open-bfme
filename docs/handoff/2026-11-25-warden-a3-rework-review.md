# Разбор Warden: A3 · Integration audit (Check=fail → условия возврата в Ready)

**Дата:** 2026-11-25 · **Решение:** `WOTR-D-045` · **Карточка:** `WOTR-1.1 · A3 · Integration audit`
(Notion `41f6f0cf-5705-8170-bd35-c3a330ee8e37`, Sequence 8, Кому — Integration Auditor).

## 1. Снимок (состоялся, дополнительных снимков не требуется)

| Поле | Значение |
|---|---|
| Run ID | `985421ec-7a33-4ed2-8520-5f29fe8420da` |
| Started / Submitted | 2026-11-24T19:40Z / 19:45Z |
| Журнал (Output Drive File ID) | `1aKAZP-5n-StEvPyvaOSPsb_gG7MW7BNP` |
| Ответ агента | `01a20f13-8ef2-7a9f-a805-7cb236e4edc5`, записан в лог выше |
| CF_AGENT_RESULT | `check=1` |
| Check / Dispatch State | `fail` / `Rework` |

Вердикт Integration Auditor (дословно, из «Свободно» карточки): «ВЕРДИКТ REWORK — A3 не принят;
§9 требует прогона оракула на стыке с зафиксированным SHA, в прогоне §9.2 (C++ GDExtension +
focused tests) не выполнен, §9.4 retail-пути BFME2 1.06 / RotWK 2.01 не предоставлены, §9.5 запуск
не выполнен; отчёт не является артефактом A3-прогона».

Run id, journal id, `CF_AGENT_RESULT` и вердикт с именами непокрытых пунктов зафиксированы в самой
карточке — снимок достаточен для разбора и для повторного прогона.

## 2. Проверка вердикта: REWORK обоснован

`docs/missions/WOTR-1.1.md` §9 требует прогона оракула «on one accepted SHA, from a clean checkout
in Arena» и прямо говорит: «Any required item marked `not run` means `rework`». Пункты прогона по
нумерации §9:

- **§9.2** (п.2: configure/build the C++ GDExtension and focused tests) — не выполнен;
- **§9.4** (п.4: `BFME2_INSTALL`/`ROTWK_INSTALL` на провалидированные retail-деревья, retail archive
  validation passes) — фактические retail-пути BFME2 1.06 / RotWK 2.01 не предоставлены;
- **§9.5** (п.5: launch the automated WOTR Remaster fixture through Godot/OpenBFME) — запуск не выполнен;
- «Report SHA» = `inline` — отчёт не является артефактом A3-прогона: якорь отчёта обязан быть
  repo-путём `docs/evidence/WOTR-1-1/A3/**` (Scope карточки; `WOTR-D-022` п.3, `WOTR-D-044`; WOTR-1.1 §10),
  а «inline» и Drive-копия доказательством приёмки не считаются.

Воспроизводимый дефект налицо (три непокрытых обязательных пункта + отчёт без repo-якоря), оракул
§8/§9 при этом не менялся и менять не требуется. Нарушений дисциплины (test hooks, injected result,
mocked asset provider) не зафиксировано и не требовалось фиксировать — прогон до них не дошёл.

## 3. Что снимает §9.4-блокер

Принятая K7 (`WOTR-1.1 · K7 · SCCPointer.cur availability for the retail Godot smoke`,
`4216f0cf-5705-815f-8124-ca2aa4f40097`, Accepted/pass, «Проверка» 2026-11-25) закрыла ровно тот узел,
на котором падал retail-прогон: Godot smoke с retail-путями проходит на зафиксированном коде
(PR #44, squash `14afddff96d2aa0a14854fd541c2bdfe056f9ad0`, merged 2026-11-25T08:49:14Z, 15/15
check-runs success), `tools/arena/smoke_godot.sh --retail` rc 0
(«SMOKE PASS: extension and scripts loaded with retail paths»), retail-дерево провалидировано
(57/57 томов, 213 BIG-архивов). §9.2 от K7 не зависит и выполняется аудитором на месте.

## 4. Условия возврата в Ready (фиксирует Warden; повторная выдача — Coordinator)

1. **Та же карточка** (A3, Sequence 8); новая карточка не заводится.
2. Прогон §9 целиком «on one accepted SHA», SHA записывается в отчёт; §9.2 выполнен
   (C++ GDExtension + focused tests); §9.4 — фактические retail-пути BFME2 1.06 и RotWK 2.01
   в Arena с прошедшей валидацией архивов; §9.5 — запуск выполнен, результат записан
   (pass или fail, но не `not run`).
3. «Report SHA» = `docs/evidence/WOTR-1-1/A3/report.md` — отчёт-артефакт именно A3-прогона:
   точные команды, rc, логи, хеши, пути артефактов (WOTR-1.1 §10, Acceptance record §9).
4. Cost gate по Acceptance карточки: `build: full`, `retail: hydrate`.
5. Никаких test hooks, убивающих юнитов, никаких injected results и mocked asset providers;
   любой такой shortcut — провал финального критерия (инварианты карточки, п.3–4).
6. Оракул §8/§9 не меняется; настоящий разбор его не правит.

## 5. Порядок

С возвратом в Ready A3 пере-выдаётся Integration Auditor **впереди Def-A1-1** (Sequence 8 < 17;
та же карточка) — решение о маршрутизации за Coordinator'ом, как он и зафиксировал в ответе
`cf769071`. Def-A1-1/2/3 остаются в очереди после A3.
