# RELAY-REPIN-1 · B. Гейты: что прогнано, с каким rc и сколько длилось

**Дата:** 2026-11-23 · **Роль:** Warden · **Карточка:** `4216f0cf-5705-81bd-b6a3-d9ebeebf7430` · **Ветка:** `arena/453d99ed-open-bfme` · **PR:** open-bfme#27 (commits `0c8ed02`, `3611552`, + evidence).

## Локальные прогоны (на этом дереве, до PR)

| Команда | rc | Длительность | Что напечатано |
|---|---|---|---|
| `node --test tools/relay/relay_test.mjs` | 0 | 0,14 с | `# tests 21 / # pass 21 / # fail 0` |
| `node tools/relay/build.mjs` (первая полная сборка после перепина) | 0 | 26,8 с | `сборка допущена: транспорт 1.17.0 @ c38ac37, регламент 1.6, отпечаток d393863a3480db40, sha256 0c9847b89e38… → build/relay`; в stderr — `Coverage: 18 ruleId …` и `Build-manifest: digest 1.6, sha256 3f7bce75…` (это их вывод) |
| `node tools/relay/build.mjs --check` | 0 | 51,9 с | `детерминизм ok: relay.user.js, prompt-coverage.json, build-manifest.json байт-в-байт равны в двух запусках` |
| `node tools/relay/build.mjs --verify` | 0 | 0,07 с | `артефакт годится: регламент 1.6 (7828 зн.), @version 1.17.0, отпечаток d393863a3480db40, sha256 0c9847b89e38…` |
| `python3 tools/precommit.py --staged-only` | 0 | <1 с | `commit gate ok (10 changed files checked)`, затем `ok (1 changed files checked)` на docs-коммите |
| `python3 tools/precommit.py` (на индексе PR) | 0 | <1 с | то же |

Время `--check` не выросло по сравнению с прошлым механизмом (52 с против 54 с на 1.16.9), хотя сборок теперь четыре (нейтральная + с входом, дважды): снимок читается из кэша, а их `build.mjs --check` внутри прогона работает по уже развёрнутому дереву.

## Отрицательные проверки (что узел обязан отклонять)

| Случай | Ожидаемое | Результат |
|---|---|---|
| `docs/regulation-digest.md` без шапки `Версия N.N`, поданный как `--digest` их сборке | rc 1 | rc 1 (проверялось на дереве их PR #5) |
| `--digest` на несуществующий файл | rc 1 | rc 1 |
| неизвестный флаг их CLI | rc 1 + `Usage:` | rc 1 |
| пин с `artifact.sha256`, не совпадающим с нейтральной сборкой | отказ «нейтральная сборка … не равна запиненному артефакту» | тест `нейтральная сборка обязана воспроизвести запиненный артефакт` |
| пин с `embedsRegulation: true` или без поля | rc 1 («выпуск транспорта несёт регламент» / «обязан быть false») | 2 новых негатива в паре `pin/ожидание` |
| их `contracts/build-manifest.json` отсутствует / сломан / без поля `digest` | rc 2 | тест `их build-manifest: отсутствие файла и отсутствие поля digest — отказ, а не пропуск` |
| `digest: null` при переданном входе | отказ «вход не был принят» | тест `verifyBundle: …` |
| manifest фиксирует другой номер или другой sha256 | две отдельные причины | тест `verifyBundle: …` |
| бандл с регламентом совпал с запиненным нейтральным артефактом | отказ (вход не подставился) | тест `verifyBundle: …` |
| артефакт, собранный до D-003 (нет `project.transportManifest`) | `--verify` → rc 2 с указанием «пересоберите бандл» | `verifyArtifact` + тест `verifyArtifact: собранный бандл сверяется с манифестом и с docs` |
| наш вход не найден в пине | rc 3 «источники или входы не получены» | тест `вход сборки: путь обязана существовать…` |
| кэш внутри рабочего дерева | rc 1 | прежний `assertCacheOutsideRepo`, не ослаблен |

Старый негатив «наши docs ≠ docs снимка» удалён вместе с классом проверки: после D-003 в снимке нет `docs/`, и сравнение нашло бы только сам себя.

## CI (open-bfme#27)

Раннер-набор 14 checks. Специфичный для карточки `Relay bundle (pinned transport, our regulation)` — pass 41 с:
`--check` + `--verify` на чистом дереве, шаг `Report the artifact identity` печатает
`transport 1.17.0 @ c38ac37, regulation 1.6, transport accepted digest 1.6/3f7bce757597, neutral 66454080fd88, fingerprint d393863a3480db40`,
`actions/download-artifact` поднят только для сверки, `cross-repo guard` (`grep -REl 'open-bfme|WOTR-|41e6f0cf'` по их дереву) — 0 совпадений.

Остальные 13 checks на head `3611552`: `pass` — `CI` (rollup), `Commit gate, public tree audit, Python tests`,
`Workflow lint (actionlint, zizmor)`, `Python tests lane 2 of 4`, `Python tests lane 3 of 4`, `Python tests lane
4 of 4` (lane 1 of 4 входит в commit-gate-проверку того же файла), `Reference MP3 fixtures`, `Linux engine build
and unit tests`, `Linux address and undefined sanitizer`, `Linux thread sanitizer`, `Windows MSVC build and unit
tests (build.bat)`, `Windows cross build (llvm-mingw, compile only)`, `Windows tests (the shipped compiler's
binaries on real Windows)`, `macOS arm64 build and unit tests`. `fail` и `pending` в наборе отсутствуют,
`mergeStateStatus: CLEAN`, `reviewDecision` пуст (репозиторий не требует approve).

Evidence-коммит (`docs/evidence/RELAY-REPIN-1/**`) не меняет ни одного проверяемого пути, кроме `docs/**`:
`relay-bundle.yml` на PR запускается только по `tools/relay/**`, `docs/REGULATION.md`,
`docs/regulation-digest.md` и самому workflow, поэтому для него релевантен тот же `Relay bundle`-прогон, что и
выше. Итог по фактическому head на момент мержа — `14 pass / 0 fail / 0 pending`.

## Не запускалось (не то же самое, что «прошло»)

| Пункт | Статус |
|---|---|
| Установка userscript 1.17.0 в браузер и включение цикла | **not run** — шаг человека (§15), песочница не имеет доступа к браузеру мастера |
| Перенос четырёх изменённых абзацев (§1, §6, §13, §15) в тело страницы Notion «Регламент» | **not run** — см. `C-docs.md`, главный callout и запись редакции синхронизированы, тело остаётся за человеком |
| `npm run verify:scriptcat` их репозитория отдельным прогоном | **not run** как отдельная команда: тот же набор запускает наш узел через их `tools/relay-build.mjs` (179 + 22 + 2, rc 0) — дублирование ради галочки не считаем проверкой |
| Розничные (retail) прогоны | **not run** — cost-гейт карточки `retail: none`, розничные данные не трогались |
| `gh pr merge` в `Creative36711/relay-wotr` | **not run** песочницей (403 Resource not accessible by integration); merge #5 выполнен мастером, отсюда и новый пин |
