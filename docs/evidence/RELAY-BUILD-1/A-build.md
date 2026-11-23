# RELAY-BUILD-1 · A. Сборка бандла проектом: что реализовано и проверено

**Дата:** 2026-11-23 · **Роль:** Warden · **Карточка:** Notion `4216f0cf-5705-81e2-9947-cc7c75898728` · **Решение:** `WOTR-D-033`.

Реализовано в этом репозитории: `tools/relay/{pin.json,build.mjs,project-profile.json,deploy.json,README.md,relay_test.mjs}` и
`.github/workflows/relay-bundle.yml`. Дерево `Creative36711/relay-wotr` не изменено ни на байт — проверено ниже.

## Содержимое

| Файл | Назначение |
|---|---|
| `tools/relay/pin.json` | запиненный выпуск транспорта: `Creative36711/relay-wotr` @ `ad2e68412339a9df32f4fc855bff53621b069c62`, `@version 1.16.9`, `contractVersion 1.11`, sha256 архива снимка `4ad55a14…`, sha256 артефакта `89bee68f…`, номер вшитого регламента в нём `1.4`, пути входов и артефактов |
| `tools/relay/build.mjs` | узел сборки: скачать снимок → сверить sha256 → положить наши `docs/` в рабочий каталог → собрать ЕГО `build.mjs` → прогнать ЕГО узел `relay-build.mjs` → проверить бандл → `build/relay/` |
| `tools/relay/project-profile.json` | копия их `tools/script-cat/wotr-project.json`, `sha256 34d8630adc1e47c9…` — проверено `sha256sum` по обоим деревом; нужна, чтобы шаг 2 перенёс файл, а не переписывал значения |
| `tools/relay/deploy.json` | целевой Apps Script и назначение артефакта; идентификаторов окружения и секретов нет |
| `.github/workflows/relay-bundle.yml` | тесты узла → `--check` (две сборки) → `--verify` → `sha256sum` → artifact `relay-bundle` |

## Проверено командами (2026-11-23, node v22.22.3)

| Команда | Результат |
|---|---|
| `rm -rf $TMPDIR/wotr-relay-cache && node tools/relay/build.mjs --check` | `детерминизм ok: relay.user.js, prompt-coverage.json, build-manifest.json байт-в-байт равны в двух запусках`, rc 0, 53.8 с на два цикла (скачивание + 5 проверок транспорта в каждом) |
| `node tools/relay/build.mjs` (docs 1.5) | `сборка допущена: транспорт 1.16.9 @ ad2e684, регламент 1.5, отпечаток cd0b5b8c6fc5a793, sha256 633dbd953c8e…`, rc 0 |
| `node tools/relay/build.mjs --verify` | `артефакт годится: регламент 1.5 (7828 зн.), @version 1.16.9, sha256 633dbd953c8e…`, rc 0 |
| `node --test tools/relay/relay_test.mjs` | `# tests 18 / # pass 18 / # fail 0` |
| `cat build/relay/build-manifest.json` | `checks`: `scriptcat:check` 0, `scriptcat:guard` 0, `test:scriptcat` 179/179, `test:scriptcat:regression` 22/22, `test:scriptcat:tool-limit` 2/2 — т. е. фиксированный список транспорта прогнан целиком, ничего не выключено |

## Главный инвариант: узел не искажает транспорт

Отдельный прогон на тех же кодах, но с docs, равными текстам транспорта (снимок `ad2e684`, `docs/*` = их 1.4):

```
$ node /tmp/relay-identity/run.mjs          # buildOnce({ root: <scratch с их docs>, write: false })
{
 "bundleSha256": "89bee68f6c3f5685ccc48ca0b636ddbeeeb35c8aee4a749dec59a219643defca",
 "pinnedSha256": "89bee68f6c3f5685ccc48ca0b636ddbeeeb35c8aee4a749dec59a219643defca",
 "coincide": true, "regulation": "1.4", "digestChars": 7673
}
```

Совпадение байт-в-байт означает: наш контур ничего не добавляет и не теряет — бандл проекта = бандл транспорта при
одних и тех же входах. При наших docs 1.5 получается другой файл (`633dbd95…`, `coincide: false`), и это тоже
проверяется: равенство бандла с запиненным при различающихся docs — отказ (см. C-refusals).

## Что сознательно не является входом сборки

`tools/relay/project-profile.json` сегодня не читается их `build.mjs` (проектные Notion/Drive ID в бандле отсутствуют;
`grep 41e6f0cf|d9f9bf1b` по их `relay.user.js` = 0). Профиль — вход деплоя и будущий аргумент `--profile`, который
вводит шаг 2; его sha256 фиксируется в манифесте уже сейчас, чтобы перенос был проверяемым.
