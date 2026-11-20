# ScriptCat provenance

## Imported snapshot

| Field | Value |
|---|---|
| Source repository | `Creative-textile/vault` |
| Last complete tree | `fe8ec9368fc2f5cb9a78a5cdbc47b66e30c07cf6` |
| Removal commit | `eecb68739dbb66a83a6cd9a3d0dbec9650a78ade` |
| Upstream version | `1.16.1` |
| License | MIT (`LICENSE` in this directory) |
| Imported paths | `tools/script-cat/**`, `tools/relay-build.mjs`, `tools/relay-watchdog.mjs` |

`fe8ec936…` is the parent of the commit that removed the Arena/Notion/Drive transport and is therefore the latest full historical snapshot. Earlier candidate `63486b4d47078b90bd8466d9306b74aefbc20644` had the same runtime version but was not final: four transport/contract test files and related process evidence changed before removal.

## WOTR adaptation

Local version `1.16.2` changes project-facing defaults/prompts/profile naming, separates frozen upstream regression documents from operational WOTR documents, removes obsolete project lifecycle wording, and embeds WOTR regulation 1.0. Transport delivery behavior through upstream D-122 remains covered by the imported tests.

Frozen historical documents used only as regression fixtures live under `upstream-fixtures/`. They are never embedded in prompts and are not product authority.

## Integrity

Reproduce the source snapshot:

```bash
git clone https://github.com/Creative-textile/vault /tmp/vault

git -C /tmp/vault rev-parse eecb687^ \
git -C /tmp/vault archive eecb687^ \
  tools/script-cat tools/relay-build.mjs tools/relay-watchdog.mjs
```

The local delta from the archive is intentionally reviewable with ordinary Git. `relay.user.js` is generated from `src/**` plus `docs/regulation-digest.md`; use `node tools/script-cat/build.mjs --check` to verify it.
