# Правки «Зона записи» по отчёту Координатора: release-гейт и Localization

Приёмка (Warden) · 2026-11-23 · ветка `arena/9f95e1e2-open-bfme` · решение `WOTR-D-039`.

## Отчёт Координатора (повод)

Два дефекта реестра, найденных при выпуске K5 (R-ZONES, прецедент `WOTR-D-022`), — на Warden,
не на исполнителей:

1. `tools/release/audit_package.py` и `tools/release/public_binaries.txt` не покрыты ни одной
   «Зона записи» — правка, без которой PR #19 не прошёл бы lane 4, формально нелегальна.
2. У Localization & Cultural QA Engineer зона содержит `godot/wotr_remaster/l10n/**`
   (такого пути в репозитории нет) и не содержит `launcher/scripts/l10n/**`, `godot/scripts/l10n/**`,
   `launcher/tests/l10n_test.gd` — прямо названных Scope K5 и роль-контрактом.

## Проверенные факты

- `gh pr view 19`: PR #19 (K4, Godot World Engineer, заявленная зона `godot/wotr_remaster/**`,
  `docs/evidence/WOTR-1-1/K4/**`) внёс оба файла `tools/release/*` — дефект 1 подтверждён.
- `ls`/`find`: `godot/wotr_remaster/l10n` отсутствует (фантомный путь); `tools/l10n/**`,
  `launcher/scripts/l10n/**`, `godot/scripts/l10n/**`, `launcher/tests/l10n_test.gd` существуют;
  `engine/src/GameClient/OpenBFMEText*` существует (`.cpp`, `.h`, `.Catalog.inc`) — дефект 2 подтверждён.
- ТЗ §10 (таблица владельцев, строка K5) и `docs/agents/roles/localization-cultural-qa-engineer.md`
  сходятся: реальная зона роли — пять glob-путей + `engine/src/GameClient/OpenBFMEText*`.
- Полный инвентарь «Роли» (14 строк) перечитан: release-инструментарию владельца нет; из семейства
  `public_*` в строке Warden уже числятся `tools/release/public_workflows.py` и
  `tools/release/test_community_prs.py`.

## Сделано (Notion «Роли», «Зона записи»; human-managed поля не тронуты)

1. **Warden** (`41e6f0cf-5705-814c-bd35-ddf6c639a618`): добавлены `tools/release/audit_package.py`
   и `tools/release/public_binaries.txt` (итого 22 строки). Основание выбора владельца —
   `WOTR-D-022` (гейт-скрипт держит роль, отвечающая за границу: здесь — «публичный релиз без retail»).
2. **Localization & Cultural QA Engineer** (`41e6f0cf-5705-81b5-8013-c27b1033fd8c`):
   было `tools/l10n/**` / `godot/wotr_remaster/l10n/**` (фантом) / `docs/evidence/WOTR-1-1/K5/**`;
   стало `tools/l10n/**`, `launcher/scripts/l10n/**`, `godot/scripts/l10n/**`,
   `engine/src/GameClient/OpenBFMEText*`, `launcher/tests/l10n_test.gd`, `docs/evidence/WOTR-1-1/K5/**`.

Запись `WOTR-D-039` в `docs/DECISIONS.md` содержит обоснование и откат.

## Наблюдения (не тронуты)

1. Остальной каталог `tools/release/**` (~120 файлов legacy-пайплайна сборки/подписи/launcher-тестов,
   включая `test_export.sh`, `aio_*`, `test_autorelease.py`) по-прежнему без владельца; закрывается
   карточками по мере надобности (§10) либо отдельным решением о консолидации.
2. Прежние follow-up остаются в силе: зона AI & Simulation Engineer — проза (§10 требует glob-список);
   зона Transport Maintainer описывает pre-`WOTR-D-033` пути.
