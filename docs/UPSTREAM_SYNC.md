# Synchronising WOTR Remaster with OpenBFME nightly

WOTR Remaster is a downstream source tree, not a copied `upstream/` directory.
The repository keeps OpenBFME commit ancestry so normal Git merges identify
what changed upstream and where our integration points conflict.

## Remotes

```bash
git remote add upstream https://github.com/Open-BFME/openbfme-godot.git
git fetch upstream --tags
git merge --no-ff upstream/main
```

The first import joins the formerly unrelated repository histories. Later
updates are ordinary merges.

## Pin, do not float at runtime

Development may track nightly frequently, but every WOTR commit and build is
validated against one explicit upstream SHA/tag. Record the accepted SHA in the
release notes and update it only through a tested sync change. Never download
an unknown `main` at build or runtime.

## Conflict-minimising rules

1. Put new deterministic code under `engine/src/GameLogic/WotrRemaster/` and
   new presentation under `godot/wotr_remaster/` where possible.
2. Keep registration/binding changes in upstream files small and mechanical.
3. Do not reformat or rename unrelated upstream code.
4. Separate an upstream sync merge from WOTR feature commits.
5. If a fix is generally useful and preserves retail behaviour, propose it
   upstream; keep intentional remaster rules downstream.
6. Never vendor a second OpenBFME snapshot under another directory.

## Update gate

An upstream merge is accepted only after these stages pass:

1. configure and compile core/GDExtension;
2. upstream unit tests not requiring retail data;
3. deterministic WOTR rule/replay/save hash tests;
4. retail archive policy check (`107/107` BFME2 and `106/106` RotWK);
5. boot/menu strategic-map smoke test through SwiftShader;
6. one tactical setup → battle result → strategic apply round-trip;
7. save/resume and hero rank/experience persistence;
8. screenshot comparison for the strategic map/HUD.

If retail data has to be restored after an Arena reset, run all retail-dependent
stages in one session rather than redownloading for each test.

## Automation target

A scheduled GitHub Action should eventually:

- fetch `upstream/main` and report the new SHA;
- create or update a dedicated sync pull request;
- run source-only checks automatically;
- mark retail/runtime checks as required manual validation until a legal,
  private runner with retail assets is available.

Do not let automation merge nightly changes without the update gate.
