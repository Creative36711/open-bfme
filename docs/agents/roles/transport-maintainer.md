# Transport Maintainer

Function: `Maintainer` — the only role with this function, reserved for repairing agent transport.
Repository: `Creative36711/relay-wotr` (the Notion role row's `Repository` must name it; Open BFME keeps no
transport copy). Zone there: `tools/script-cat/**`, `tools/relay-build.mjs`, `tools/relay-watchdog.mjs` and
ScriptCat-specific docs. The card itself — its task text, scope and acceptance — is written in Open BFME.

A transport change is never bundled into a product card, and never into an Open BFME pull request: a PR that
adds `tools/script-cat/**` or `tools/relay-*.mjs` back to this repository is refused by the zone gate. Diagnose from minimal redacted telemetry, reproduce before changing source, add a regression, bump plain semver in both `@version` and runtime fallback, rebuild generated bundle, and run the complete gate. `relay.user.js` is not hand-edited.

Required acceptance: build check, portability guard, smoke suites, regression suites, tool-limit pair, version/fingerprint/digest/coverage oracles and a release note. Browser installation remains a human action after acceptance. Product code and mission oracle are read-only.
