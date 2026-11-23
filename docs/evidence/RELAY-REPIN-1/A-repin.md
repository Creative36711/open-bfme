# RELAY-REPIN-1 · A. Перепин: на что и как проверено

**Дата:** 2026-11-23 · **Роль:** Warden · **Карточка:** `4216f0cf-5705-81bd-b6a3-d9ebeebf7430` · **PR:** open-bfme#27 · **База:** `77fef86`.

## Значения пина (`tools/relay/pin.json`) и способ их получения

| Поле | Значение | Откуда взято |
|---|---|---|
| `transport.repo` | `Creative36711/relay-wotr` | — |
| `transport.kind` / `commit` | `commit` / `c38ac3700a550a977adcb194f6b67546e43aacb8` | merge-коммит их `main` после влития PR #5 (`gh pr view 5` → MERGED 2026-11-23T20:49:26Z) |
| `transport.userscriptVersion` | `1.17.0` | `@version` в `src/00-header.js` скачанного снимка |
| `transport.contractVersion` | `1.12` | `protocolVersion` из их `tools/script-cat/contracts/agent-protocol.json` в снимке |
| `snapshot.url` | `https://codeload.github.com/Creative36711/relay-wotr/tar.gz/c38ac37…` | codeload по полному SHA |
| `snapshot.sha256` | `debf8da028ee0dce586fc43b4544a9a285236a820a8bc1e6f754cf240c475dba` | `sha256sum` скачанного архива (1 720 670 Б) |
| `transport.artifact.path` | `tools/script-cat/relay.user.js` | их путь артефакта (в коммите не лежит — артефакт сборки) |
| `transport.artifact.sha256` | `66454080fd88c222c76e2de4863a0e77126233490a10bdbbbbae780adeaeac98` | **нейтральная** сборка их CLI на развёрнутом снимке (1 479 168 Б, отпечаток `d393863a3480db40`) |
| `transport.artifact.embedsRegulation` | `false` | `docs/regulation-digest.md` в снимке отсутствует; их `contracts/build-manifest.json` = `{"schema":"scriptcat-build-manifest","version":1,"digest":null}` |
| `inputs.digest` | `docs/regulation-digest.md` | единственный вход сборки с проектной стороны |

Поле `transport.artifact.regulationVersion` удалено: после их `D-003` выпуск не несёт регламента, поэтому «номер
регламента в артефакте» — свойством, которого не существует. `validatePin` требует теперь `embedsRegulation === false`
(строка в отказе: «выпуск транспорта несёт регламент»), а нейтральность подтверждается не обещанием в JSON, а
фактической нейтральной сборкой в `buildOnce` → `verifyNeutralBundle`.

## Что изменилось в механизме (их D-003)

| Было (1.16.9, регламент 1.5) | Стало (1.17.0) |
|---|---|
| `stageInputs` клал `docs/regulation-digest.md` и `docs/REGULATION.md` в каталог `docs/` снимка | вход передаётся их же сборке флагом `--digest <абсолютный путь>`; ни одного файла в снимок не пишется |
| `docsCoincide` сверял наши docs с их зеркалом | класса нет: в снимке зеркала нет, сравнение стало бы «наш файл против самого себя» |
| бандл сверялся с пином через `artifact.regulationVersion` | `verifyNeutralBundle`: sha256 нейтральной сборки == `transport.artifact.sha256`, `@version` совпадает, в нейтральном бандле нет поля digest |
| «вшитый регламент = наш» выводился из текста бандла | `readTransportManifest` читает их `contracts/build-manifest.json`: `digest.version` == `Версия N.N` нашего файла, `digest.sha256` == sha256 нашего файла; отсутствие записи или `digest: null` при переданном входе — отказ |
| `promptDigest` срезал текст по маркеру `<!-- build:meta -->` | `promptDigest` повторяет их `normalizeDigest`: вырезает строки-комментарии и схлопывает три и более переноса. Разойтись правилами означало бы «вшитый регламент ≠ наш» на каждом CI |
| профиль проекта упоминался как вход | профиль в сборку не подаётся: `project-profile.json` — импорт-файл рантайма ScriptCat (их `publicProjectFiles`) |

Нейтральная сборка и сборка с входом делаются в **двух разных копиях** снимка: их `build.mjs` пишет
`tools/script-cat/contracts/build-manifest.json` в проверяемое дерево, и их же `relay-build.mjs` при
`--check` внутри сборки отказался бы на дереве с уже вшитым регламентом. Одна копия на два прогона — это
ложный `git status`, а не находка.

## Ключевые числа нашего входа (после правки docs)

| Файл | sha256 | примечание |
|---|---|---|
| `docs/regulation-digest.md` | `3f7bce7575973df6296ce0f282552a12f0687b982e98cbedf647bf3efd1825d1` | 11 603 Б; в промпт попадает 7828 знаков после нормализации; ровно то же число фиксирует их manifest |
| `docs/REGULATION.md` | `426b61fa343e3daf16ced8a5de7b1b54075e3ea800b247e1012d3ff0e7da0b91` | версия 1.6 |
| `build/relay/relay.user.js` | `0c9847b89e38bae7189a69fbe79dac469c492f3ed7ba79f6f5a101d2047829d3` | 1 490 053 Б, `@version 1.17.0`, отпечаток `d393863a3480db40` |
| `build/relay/prompt-coverage.json` | `c71a9152d76ba279ba21a042cbf60f58e793600b5b71595bd952992da2fe3aad` | 18 `ruleId` → блоки → профили |
| `build/relay/build-manifest.json` | `bec8e40dd11d84959f7479d66c2c7903b2981ea27d259864e1aff98600651db2` | несёт `transport.neutralBundleSha256` и `project.transportManifest` |

Артефакты в Git не коммитятся (`.gitignore` для `build/`): идентичность сборки доказывается хешем в evidence и
шагом `Report the artifact identity` в `relay-bundle.yml`, который на CI печатает те же числа.
