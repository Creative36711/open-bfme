# Синхрон 1.3 и CI-гейт транспорта: что проверено и чем (2026-11-22)

Проверка pull request `Creative36711/relay-wotr#2` («Add ScriptCat CI and sync operational mirror 1.3», head
`f8ccb9fb`, база `220f2e0e`, 18 файлов, +430/−231) против отчёта исполнителя. Основание для правил — `WOTR-D-028`,
формулировка зеркала — `docs/agents/pr-rules.md` Р11.

## Утверждения отчёта против замера

| утверждение | чем проверено | итог |
| --- | --- | --- |
| newline-PR не нужен, `src/**` равен базе | `build.mjs --check` на head: `ScriptCat bundle соответствует 45 исходным файлам` (rc 0) | подтверждено |
| бандл пересобран под digest 1.3, sha256 `4d10f303…9924ac` | `sha256sum tools/script-cat/relay.user.js` = `4d10f303cbfd9672daf3917c2f6c6a9a9aaeb6b22a980c7bc4ed66b2fa9924ac` | подтверждено байт в байт |
| версию не поднимал, `src/**` не менялся | preflight на head: `версия 1.16.8, src/** не менялся`; `@version` в бандле = 1.16.8 (`package.json.version` = 0.1.0 — это версия npm-пакета, правило её не читает) | подтверждено |
| 12 файлов синхронизированы | `git ls-tree -r HEAD -- docs` (27 файлов) против `main`: 24 побайтово равны, `docs/REGULATION.md` и `docs/agents/pr-rules.md` разнятся руками, `docs/DECISIONS.md` выведен из зеркала, `docs/DECISIONS-UPSTREAM.md` и `docs/scriptcat-update.md` — только транспорта | подтверждено + `docs/missions/TEMPLATE.md` привезён корректно (в базе `220f2e0` его не было: `contents` → 404) |
| журнал разведён без потерь | `^## WOTR-D` : база 23 → `DECISIONS-UPSTREAM.md` 23, diff отсортированных заголовков пуст; `^## D-[0-9]+` в базе 0 → в `docs/DECISIONS.md` 0 | подтверждено, потерь нет |
| лимит digest выдержан | `assertDigestVersionMatches` внутри `build.mjs --check` (rc 0) + `normalizeDigest` = 7588 знаков при `DIGEST_MAX_CHARS = 9000` | подтверждено |
| `git diff --check` чист | `git diff --check origin/main HEAD` на свежей копии head | rc 0 |
| «YAML отдельным парсером проверить не удалось» | `python3 tools/release/public_workflows.py --git-dir … --tree …` (оракул `open-bfme`) + в самом workflow шаг `pip install PyYAML==6.0.3` + `yaml.safe_load` | блокер снят исполнителем: шаг в workflow есть |
| «проверки прошли, GitHub Actions пока queued» | `actions/runs` репозитория: run 45030434922 = **failure** | **не подтверждено**: удалённый гейт красный |

## Падение гейта: причина воспроизведена локально

Шаг `Transport Maintainer zone preflight`, `annotation: Process completed with exit code 1`
(`.github/workflows/scriptcat.yml:17`). Все шаги до него зелёные (`checkout`, `setup-node`,
`npm run verify:scriptcat`, `yaml.safe_load`).

Воспроизведение точного состояния CI (`pull_request`, `actions/checkout` с `fetch-depth` по умолчанию, merge-commit
`refs/pull/2/merge`): `origin/main` отсутствует, `git rev-parse --is-shallow-repository` = true, и preflight даёт

```
  FAIL  клон не усечён — клон shallow (1 граничных коммитов) …
  FAIL  ветка не отстала от origin/main — нет ref origin/main — выполните git fetch origin main
блокеров: 2 — push не делать        # rc 1
```

Третий FAIL в моём прогоне — мой собственный лог-файл в рабочем дереве: `changedFiles()` складывает `git diff
--name-only origin/main` **и** `git status --porcelain`, то есть unmatched untracked-файл тоже попадает в зону. В CI
дерево чистое, поэтому это не артефакт прогона, а особенность правила (полезно помнить: если `verify:scriptcat`
начнёт писать в дерево, зона-проверка покраснеет без вины diff'а).

После `git config remote.origin.fetch "+refs/heads/*:refs/remotes/origin/*" && git fetch --unshallow origin` — то же
дерево, тот же preflight: `можно пушить`, rc 0. Отсюда единственно нужная правка:

```yaml
      - uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
        with:
          persist-credentials: false
          fetch-depth: 0
```

Правило версии, `preflight.mjs` и regex журнала под это не правятся: краснел не гейт, а его предпосылка (Р10
«клон полный, `origin/main` виден»).

## Форма workflow против оракула

`.github/workflows/scriptcat.yml` (30 строк): `pull_request` + `push` на `main`, `permissions: contents: read`,
оба `uses` по полному 40-hex SHA, `runs-on: ubuntu-24.04`, `timeout-minutes: 15`, без `secrets`, без `id-token`,
никаких `: write`. Оракул `public_workflows.py` на дереве head — `PUBLIC WORKFLOWS ok` (то же подтверждает зелёный
шаг YAML в самом прогоне). Замечания не блокирующие: `node-version: '>=20'` = declared floor из `engines`, но
резолвится по-разному со временем (при желании — точная версия), и аудит `tools/release/pr_audit.py` в preflight
отчитывается `ok` при отсутствии файла («нет tools/release/pr_audit.py — проверка пропущена»), то есть в CI транспорта
эта строка зелёная всегда.

## Раунд 2: head `f425a78` (14:28) — гейт зелёный, зеркало раздуто тем же правилом

| утверждение транспорта | замер | итог |
| --- | --- | --- |
| `fetch-depth: 0`, прогон зелёный | workflow: шаг `checkout` с `persist-credentials: false` + `fetch-depth: 0`; run `45033019753` = `success` | подтверждено |
| зеркало синхронизировано по Р11 с актуальным `main` | все процессные файлы (`REGULATION.md`, `regulation-digest.md`, `PHASES.md`, `agents/**`, `missions/**`) побайтово равны `open-bfme@main`, включая текст Р11 | подтверждено |
| `DECISIONS-UPSTREAM.md` — побайтная копия upstream-журнала | sha256 `f641679b…` == sha256 `main:docs/DECISIONS.md`, 28 ключей `WOTR-D-*` | подтверждено |
| `D-001` + ключ в контракте | `docs/DECISIONS.md:5` = `## D-001 (1.16.8) — …` (матчится с `^##\s+D-\d+`); в `contracts/agent-protocol.json` добавлен блок `d001` с `decision`/`safety`/`evidence` | подтверждено: проверка «одно решение» впервые стала живой |
| фиктивный зелёный пункт аудита убран + тест | `preflight.mjs`: строка `public-tree` печатается только при наличии `tools/release/pr_audit.py`; тест `pr-preflight-smoke.mjs` переписан с `assert.match(…пропускается…)` на `assert.doesNotMatch(/публичное дерево проходит аудит/)` | подтверждено: исчез ложный PASS, обязательность самой проверки при наличии инструмента не ослаблена |
| бандл и версия не менялись | `sha256sum relay.user.js` = `4d10f303cbfd9672…`; `build.mjs --check` rc 0 | подтверждено |
| их preflight на чистом дереве | `можно пушить`, rc 0, «160 файлов, все в зоне» | подтверждено |
| `git diff --check` находит только `docs/STOPS.md:959` | ровно одна строка вывода: `new blank line at EOF.` | подтверждено, и корень — в нашем файле |

Размер PR стал **160 файлов / +31 588**, потому что Р11 в редакции `d14be11` сказал «every file under `docs/`». Вне
процессного набора оказались 139 файлов: 83 корневых страниц `docs/`, 25 `docs/re/rotwk/**`, 13 `docs/evidence/**`,
7 `docs/coverage-1/*`, 5 `docs/architecture/**`, 3 `docs/tooling/**`, по одному `docs/setup/notion-ids.md`
(5 ссылок на Notion + список data source'ов), `docs/performance/**`, `docs/handoff/**`. `relay-wotr` — публичный
репозиторий (`private: false`), как и `open-bfme`, поэтому нового секрета там не появилось, но содержательного
переноса продукта в транспорт быть не должно: он ломает и аудит зеркала (167 файлов вместо 28), и обязывает транспорт
следить за файлами, которые он не читает. Исправление — в самом правиле (`WOTR-D-029`, Р11 закрытым набором),
транспорту остаётся удалить 139 файлов из PR; остальное из этого пуша принято.

## Что из этого следует

1. Merge blocked: без `fetch-depth: 0` собственный гейт `relay-wotr` красен на любом PR, включая тот, что его
   заводит.
2. `mergeable_state` GitHub = `unstable`, а не `blocked`: чек «ScriptCat required gate» не назначен required, то
   есть название пока описывает намерение. Назначение required check'а на `main` — действие владельца (`master`),
   обход не допускается.
3. Две ручные правки в зеркальных файлах (`docs/REGULATION.md`, `docs/agents/pr-rules.md`) переносятся в
   транспорт-only файл: иначе следующий синхрон их сносит, а аудит «24 файла равны» превращается в «24 минус два,
   которые всегда разные».
4. PR, который ввёл гейт и развёл журналы, не оставил записи в `docs/DECISIONS.md` транспорта — при том что правило
   «одно решение на PR» он же и вводит. Формально новое правило условное («A PR that records a decision»), поэтому
   не нарушено; содержательно первое `## D-001` должно принадлежать именно ему.
