# ScriptCat troubleshooting

## No card is issued

Check in order: active project profile, Notion connection, exactly one active Coordinator/Reviewer where required, assignee role row, Agent URL, repository route, mission status, dependency UUID/state, sequence and Ready state. Do not assign an executor card to Warden to bypass the roster.

## Marker is ignored

Confirm exact marker name, one-line JSON, current dispatch/run or mission/turn IDs, all required fields and no prose after the marker. A PR claim must be internally consistent. Use the bounded reminder; do not redo the work.

## Composer contains text but Send is unavailable

ScriptCat distinguishes registered editor input from fallback DOM text. Let the bounded 1.16.1+ healing path reinsert only when `writePrompt` recorded `registered:false`. Do not repeatedly click Send or clear text by hand while a delivery is pending.

## Arena says “Something went wrong” or tool limit

A rejected submission and a tool-call limit are different continuation reasons. Both preserve IDs, but the continuation prompt names the real cause. More than the configured limit becomes an explicit pause.

## Running card has no live turn

Do not manually mint a run ID. Recovery compares durable local state, Notion and Arena page identity; an orphan can return to Ready only through the transport's guarded path.

## Merge cannot be verified

Check GitHub connection, target repo/base, PR URL, merge state and merge commit. A network error is `unknown`, not rejection and not acceptance. A PR from another repo or base is not the card's merge.

## Digest/build mismatch

Run:

```bash
node tools/script-cat/build.mjs
node tools/script-cat/build.mjs --check
```

Confirm `docs/REGULATION.md` and `docs/regulation-digest.md` declare the same `Версия N.N`, the digest is ≤9000 characters and `@version` equals runtime `SCRIPT_VERSION`.

## Portability guard fails

Remove former-project names/default routes from executable source, prompts and operational documentation. Historical regression evidence belongs only in `tools/script-cat/upstream-fixtures/`, which must not be injected into prompts.

## Retail end-to-end cannot run

Report exact missing prerequisite and command/rc. A synthetic DTO test may keep a card moving but cannot close a criterion that explicitly requires retail BFME2/RotWK. Never invent a pass or start bulk asset transfer before a verified canary.

## Human-only cases

Escalate only browser userscript installation, captcha/login, access/account creation, paid quota, secrets, license/ToS or product boundary decisions. Code defects, queue mistakes, missing evidence and recoverable network faults stay with the named agent/maintainer path.
