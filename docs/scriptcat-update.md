# ScriptCat для WOTR Remaster

Сейчас **1.16.2** (WOTR-D-008), локальная адаптация upstream 1.16.1.

## Источник

- Vault historical tree: `fe8ec9368fc2f5cb9a78a5cdbc47b66e30c07cf6`.
- Следующий commit `eecb68739dbb66a83a6cd9a3d0dbec9650a78ade` удалил ScriptCat в пользу другого локального контура, поэтому parent является последней полной версией.
- Предварительно найденный `63486b4d47078b90bd8466d9306b74aefbc20644` старше окончательного snapshot: между ним и parent удаления менялись contract/docs tests.
- Upstream version: 1.16.1, MIT. Полная provenance: `tools/script-cat/UPSTREAM.md`.

## Выпуск 1.16.2 (WOTR-D-008)

Поведение доставки 1.16.1 сохранено. Изменён проектный слой:

- один target repository `Creative36711/open-bfme`;
- canonical pilot profile `pilot.wotr` и WOTR defaults/examples;
- WOTR roles, phases 1–8, retail boundary and deterministic/tactical gates in the embedded digest;
- прежние project-specific history assertions читают frozen fixtures, а operational docs остаются WOTR-only;
- forbidden legacy project/pre-product wording удалено из runtime and repository docs;
- source change сопровождается plain-semver bump, rebuilt bundle and complete test gate.

## Установка

1. Импортировать `tools/script-cat/wotr-project.json` через окно проектов ScriptCat.
2. Сохранить Notion/GitHub/Gateway connections локально; secrets не входят в profile file.
3. Зарегистрировать Drive profile `wotr-remaster` только через Gateway flow.
4. Проверить подключения и разрешить профиль. Автоцикл остаётся выключен до ручного включения.
5. Userscript устанавливает человек из принятого `tools/script-cat/relay.user.js` после сравнения SHA-256 с build report.

## Gate

```bash
npm run scriptcat:check
npm run scriptcat:guard
npm run test:scriptcat
npm run test:scriptcat:regression
npm run test:scriptcat:tool-limit
```

Либо `npm run verify:scriptcat`. Успех: каждый процесс rc 0, Node summary `# fail 0`, bundle version equals runtime fallback, embedded digest equals source and coverage contains all rule IDs.

## Обновление

Transport Maintainer changes `src/**` only on a dedicated card. A source change must bump both `@version` and `SCRIPT_VERSION`, add/update decision and regression evidence, run `node tools/script-cat/build.mjs`, then complete the full gate. Browser installation is never performed silently by repository tooling.
