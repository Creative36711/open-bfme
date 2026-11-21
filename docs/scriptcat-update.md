# ScriptCat для WOTR Remaster

Сейчас **1.16.4** (WOTR-D-017), локальная адаптация upstream 1.16.1.

## Источник и граница provenance

Точный source repository, imported tree `fe8ec936…`, removal parent
`eecb687…`, MIT license и команда воспроизведения находятся только в
`tools/script-cat/UPSTREAM.md`. Это не operational target и не источник
продуктовых правил. Текущий target задаёт только WOTR profile:
`Creative36711/open-bfme@main`.

Regression fixtures сохраняют transport-семантику snapshot, но с 1.16.4
обезличены до `CASE-*`/`fixture-*`: прежние product names, repositories и card
prefixes не должны попадать ни в prompts, ни в comments/tests/contracts, где их
может принять за текущие Transport Maintainer.

## Выпуск 1.16.2 (WOTR-D-008)

Поведение доставки upstream 1.16.1 сохранено. Добавлены один WOTR target,
`pilot.wotr`, WOTR roles/phases/retail/determinism gates и regulation 1.0.

## Выпуск 1.16.3 (WOTR-D-014)

Transport protocol не менялся. Regulation 1.1 закрепил Upstream Integrator как
Executor, единственного Transport Maintainer, declared build/retail Cost gates,
точные локали `en`, `ru`, `zh-Hans`, `ja`, `pt-BR`, `de`, `fr`, `ko` и WOTR
bootstrap preflight.

## Выпуск 1.16.4 (WOTR-D-017)

Transport protocol остаётся 1.11. Полная WOTR-адаптация source layer:

- embedded regulation 1.2 включает ADR-004 player entry, адаптацию существующего
  launcher/one-archive pipeline и миссию portable playtest;
- после отдельного решения начать подготовку миссии обязательная B0-репетиция
  до её рабочих карточек закреплена в prompts/regulation;
- former-project names, repositories и card prefixes удалены из runtime source
  comments, contracts, tests, browser/regression fixtures;
- точный imported repository разрешён только в явно неоперационном
  `tools/script-cat/UPSTREAM.md`;
- portability guard сканирует всё дерево ScriptCat, включая tests,
  regression-tests, browser-tests, contracts и sanitized upstream fixtures;
- bundle regenerated из 45 source files и regulation digest; полный gate и
  artifact hash записаны в `docs/evidence/scriptcat-1.16.4.md`.

## Установка

1. Импортировать `tools/script-cat/wotr-project.json` через окно проектов
   ScriptCat.
2. Убедиться, что активная вкладка показывает **War Of The Ring: Remaster**,
   `Creative36711/open-bfme@main` и Drive folder `wotr-remaster`.
3. Сохранить Notion/GitHub/Gateway connections локально; secrets не входят в
   profile file.
4. Зарегистрировать Drive profile `wotr-remaster` только через Gateway flow.
5. Проверить подключения и вручную разрешить профиль. Автоцикл остаётся
   выключен до ручного включения.
6. Userscript устанавливает человек из принятого
   `tools/script-cat/relay.user.js` после сравнения SHA-256 с build report.
7. Старый browser profile можно хранить изолированно, но вкладка WOTR не должна
   быть открыта под ним; ScriptCat fail-closed не подменяет отсутствующий профиль.

## Gate

```bash
npm run scriptcat:check
npm run scriptcat:guard
npm run test:scriptcat
npm run test:scriptcat:regression
npm run test:scriptcat:tool-limit
```

Либо `npm run verify:scriptcat`. Успех: каждый процесс rc 0, Node summary
`# fail 0`, bundle version equals runtime fallback, embedded digest equals source,
coverage contains all rule IDs, а full-tree guard не находит прежних product/card
идентификаторов вне строго разрешённой provenance-записи.

## Обновление

Transport Maintainer changes `src/**` only on a dedicated card. A source change
must bump both `@version` and `SCRIPT_VERSION`, add/update decision and regression
evidence, run `node tools/script-cat/build.mjs`, then complete the full gate.
Browser installation is never performed silently by repository tooling.
