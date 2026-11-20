# Agent tools

Use repository scripts before ad-hoc commands.

- ScriptCat build/check: `node tools/script-cat/build.mjs [--check]`.
- Portability: `node tools/script-cat/portability-guard.mjs`.
- Transport tests: root `npm run verify:scriptcat`.
- PR preflight: `node tools/script-cat/preflight.mjs --zone "<paths>"`.
- Upstream pin: `OPENBFME_UPSTREAM`; sync is an Upstream Integrator card.
- Retail BIG transfer: `tools/retail_big_transfer.py`; byte-exact and canary-first.
- GitHub facts: `git` locally and `gh` for PR/check state.

Do not install a heavy tool manually in `/tmp` and call it reproducible. A required tool is pinned by version/hash and installed through a repository script or the accepted OpenBFME build instructions. Do not reveal credentials in commands or reports.
