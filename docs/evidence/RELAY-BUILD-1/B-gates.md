# RELAY-BUILD-1 · B. Гейты этого репозитория и границы

**Дата:** 2026-11-23 · **Роль:** Warden · **Карточка:** `4216f0cf-5705-81e2-9947-cc7c75898728`.

## Проверки, прогнанные локально перед PR

| Команда | Результат |
|---|---|
| `python3 tools/precommit.py` (после `git add -A`, 9 файлов) | `commit gate ok (9 changed files checked)`, rc 0 |
| `git diff --cached --check` | пусто |
| `python3 tools/release/pr_scope.py --base origin/main --event pull_request` | `build: full` (правки вне `docs/*` корня: `tools/relay/**`, workflow) |
| `python3 - <<PY` c `public_workflows.workflow_problems` по всем пяти workflow | все `clean`, включая новый `.github/workflows/relay-bundle.yml` |
| `node --test tools/relay/relay_test.mjs` | 18/18, fail 0 |
| `node tools/relay/build.mjs --check`, `--verify` | rc 0 (A-build.md) |

Новый workflow соответствует политике `.github/` репозитория: только `pull_request` / `push` / `workflow_dispatch`,
`permissions: contents: read`, ни одной ссылки на `secrets.`, `uses:` по 40-hex SHA, `runs-on: ubuntu-24.04`,
`persist-credentials: false`, `timeout-minutes`. Отдельно вычитан формат YAML: `name:` без `": "` — ровно та
причина, по которой lane PUBLICCI-1 требует, чтобы файл парсился `safe_load` (у меня был `name: … (offline: pin, …)`
и PyYAML честно отказал; формулировка исправлена, а не проверка ослаблена).

`actions/setup-node` не подключён: узлу нужен node ≥ 20, он есть на образе, и шаг проверяет это явным `node -e` —
новый за-pin-енный экшен был бы лишней поставочной зависимостью ради одного условия.

## Границы (что осталось нетронутым)

| Путь | Состояние |
|---|---|
| `Creative36711/relay-wotr` | ни одного изменения: PR не открывался, `gh api repos/Creative36711/relay-wotr/commits/main` = `ad2e6841…` до и после работы |
| их `tools/script-cat/**`, `wotr-project.json`, зеркало их `docs/**` | не трогались; зеркало по-прежнему 1.4 — расхождение на одну запись закрывается шагом 2, а не синхрон-PR (см. `WOTR-D-033`) |
| `tools/script-cat/**`, `tools/relay-*.mjs` в этом репозитории | не появились: `git ls-tree -r HEAD --name-only` по этим префиксам пуст (пункт 4 `WOTR-D-024` в силе) |
| `engine/**`, `godot/**`, `launcher/**`, `tools/l10n/**`, `tools/arena/**` | не изменялись |
| оракулы и гейты | ни один не удалён, не ослаблен, не переведён в skip; `not run` нигде не записан как `pass` |

## Приёмка, которую это позволяет проверить независимо

1. `git ls-tree` по `tools/relay/**` даёт ровно шесть файлов; `pin.json` содержит четыре sha/номера, `build.mjs` — ни одного хардкода `ad2e684` (значение только из пина): `grep -c ad2e684 tools/relay/build.mjs` = 0.
2. `node tools/relay/build.mjs --check` на свежем дереве → rc 0 и строка `детерминизм ok`.
3. `node tools/relay/build.mjs --verify` → номер во вшитом регламенте равен шапке `docs/REGULATION.md`.
4. Удаление `pin.json` → rc 3 («пин не найден»), порча sha в пине → rc 2, — т. е. fail closed, а не тихий пропуск (C-refusals.md).
