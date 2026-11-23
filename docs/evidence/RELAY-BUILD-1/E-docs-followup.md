# RELAY-BUILD-1 · E — дописка по пункту 5 приёмки (docs, следующий ход)

Карточка `OPS · RELAY-BUILD-1` требовала пунктом 5: «точно поправить
`docs/agents/troubleshooting.md` и `docs/setup/notion-ids.md` там, где они ссылаются на `verify:scriptcat` как на
нашу команду или на репозиторий транспорта». Первый коммит шага (`57c7090`) этого не сделал — правки внесены
следующим коммитом того же хода; этот файл фиксирует, что именно и почему.

## Что было не так

| Путь | Было | Стало |
|---|---|---|
| `docs/agents/troubleshooting.md`, раздел «Digest/build mismatch» | «Run in `Creative36711/relay-wotr` … a regulation change needs a sync commit and a rebuild on that side» + команды `node tools/script-cat/build.mjs [--check]` — то есть предписание идти в репозиторий транспорта и путь, которого у нас нет | команды `node tools/relay/build.mjs` / `--check` / `--verify` здесь; явно сказано, что синхрон-коммит и пересборка в `relay-wotr` больше не нужны (WOTR-D-033), что единственный источник версии транспорта — `tools/relay/pin.json`, и что при расхождении правят пин, а не кэш и не проверку; предложение про `Версия N.N` / ≤9000 знаков / `@version` сохранено и дополнено тем, что узел проверяет все три сам и отказывает |
| `docs/setup/notion-ids.md`, абзац «Canonical import file» | файл-источник импорта — `tools/script-cat/wotr-project.json` в `relay-wotr` | `tools/relay/project-profile.json` этого репозитория, со ссылкой на то, где он лежал раньше и почему переехал; добавлено, что версия транспорта пинится в `tools/relay/pin.json`; «metadata only» и запрет на коммит токенов/ключа GateWay сохранены дословно по смыслу |
| `docs/ARCHITECTURE.md`, раздел о границах (вне списка Scope — правка вызвана появлением `tools/relay/**`) | «Каталога транспорта в этом дереве нет…» — после шага 1 формулировка читается как опровержение: узел сборки у нас появился | то же утверждение + уточнение: `tools/relay/**` — проектный вход в транспорт (пин + наши `docs/**` → `build/relay/relay.user.js`), а не копия транспорта, и транспорт это дерево не читает |

Исторические записи не правятся задним числом: `docs/DECISIONS.md` (WOTR-D-024/-029/-031/-033), `docs/RELEASE.md`
и `docs/agents/acceptance-report.md` остались как есть — они описывают состояние на момент решения, а пункт 5
карточки на них не распространялся.

## Влияние на артефакт

`docs/REGULATION.md` и `docs/regulation-digest.md` в этом коммите **не изменены**, поэтому входы сборки не
подвижны и бандл остаётся прежним: `build/relay/relay.user.js` → `633dbd953c8e89bf455924e12c3dc34f05cc8e76897e458424f8e257e34b43b9`.
Пересборка после этого коммита не требуется; `node tools/relay/build.mjs --verify` прогнан на этом дереве — rc 0
(«артефакт годится: регламент 1.5 (7828 зн.), @version 1.16.9»).

## Локальные гейты этого коммита

| Проверка | Результат |
|---|---|
| `python3 tools/precommit.py` | `commit gate ok (4 changed files checked)`, rc 0 (прогон до коммита; после коммита — `0 changed files checked`, потому что индекс чист) |
| `git diff --cached --check` | пусто |
| `python3 tools/release/pr_scope.py --base origin/main --event pull_request` | `SCOPE: engine build not needed - all 4 changed path(s) are documents, so the merge's engine inputs are identical to the base's` |
| `node tools/relay/build.mjs --verify` | rc 0 |
| `node --test tools/relay/relay_test.mjs` | 18/18, fail 0 |
| `python3 tools/release/pr_audit.py --tree HEAD^{tree} --base HEAD^1` | `PR AUDIT ok: 7318d6c1… (4313 files)` |

## Почему `Relay bundle` в этом PR не запускался

Это не сломанный триггер. У `.github/workflows/relay-bundle.yml` стоит фильтр `paths:` из четырёх значений —
`tools/relay/**`, `docs/REGULATION.md`, `docs/regulation-digest.md` и сам workflow: ровно то, что входит в бандл.
Ни один из этих путей в PR #25 не изменён, поэтому пересборка ничего бы не проверила. На PR #24 тот же job
прошёл (`Relay bundle … pass`, 32 с): `node --test`, `--check`, `--verify`, `sha256sum` и загрузка артефакта —
см. `A-build.md`. При `push` в `main` и `workflow_dispatch` фильтр `paths:` не действует, так что сборка
воспроизводится на main в любом случае.