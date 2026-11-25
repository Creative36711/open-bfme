# WOTR-1.1 · A3 · Integration audit — report (living)

## Попытка 4, BASE f3eec31 (continuation)

- **New BASE_SHA:** `f3eec31f8f8cbb62d6cf500d9348eaf47bd8b20e` (origin/main after PR #47, #48; includes Def-A1-1). Prior base bdc6df5 no longer valid.
- **Branch:** `arena/741a3d19-open-bfme` (rebased once at start onto origin/main; base will not be moved after build start).
- **Continuation:** preserves prior evidence (§5.1 PASS, §9.1 PASS on bdc6df5, retail metadata pin PASS). Prior verdict attempt 3: rework (build not completed due to 30-min tool limit; §5.2 detached method required).
- **Date (UTC):** 2026-11-25
- **Role:** Integration Auditor (Notion `41e6f0cf-5705-81aa-a59b-ed5a2cac6d08`, Active)
- **Card:** WOTR-1.1 · A3 · Integration audit (Notion `41f6f0cf-5705-8170-bd35-c3a330ee8e37`, seq 8)
- **Scope/zone:** `docs/evidence/WOTR-1-1/A3/**` only
- **Cost gate:** `build: full`, `retail: hydrate` (+ fixtures step per §5.1, tier none)
- **Oracle:** WOTR-1.1 §8 A3 / §9, unchanged. No test hooks / injected results / mocked asset providers / hand-authored results / --write-golden. No `*.mp3.bin` commits. No "download all 57 volumes for future".

## Baseline (prior, attempt 3)
- **BASE_SHA:** `bdc6df54720ea4ad633fb6adf3ba3d79000e2128` (origin/main, PR #46 merge)
- **HEAD at first commit:** `a26c2425e372fe28904b2b5ab1cc2ab6581aa27f` (skeleton commit)
- **git status --short (before first commit):** clean

## §5.1 Audio fixtures (pre-§9.2/§9.13) — Attempt 4

- **Tier:** none (per cost-gates §5.1)
- **Bundle source:** `Arena/wotr-remaster/artifacts/wotr-audio-fixtures.tar.gz` (per §5.1/cost-gates; Drive id not recorded in evidence per §7)
- **Archive SHA-256:**
  ```text
  $ sha256sum google_drive/wotr-audio-fixtures.tar.gz
  1e58106afd3ebf8f535b8aa11a040a29665051b0266caddc1984ccdfdf6ea052  google_drive/wotr-audio-fixtures.tar.gz
  rc=0
  ```
  Expected pin: `1e58106afd3ebf8f535b8aa11a040a29665051b0266caddc1984ccdfdf6ea052` — **MATCH**.
- **Unpack:** to `$HOME/.cache/wotr-audio-fixtures` (outside `engine/build`), `FILES=17`.
- **Check-only:**
  ```text
  $ python3 tools/audio/gen_mp3_fixtures.py --check-only --out $HOME/.cache/wotr-audio-fixtures
  Verified 17 MP3 fixtures (sizes and SHA-256)
  RC=0
  ```
- **Log:** `docs/evidence/WOTR-1-1/A3/logs/fixtures_check_only.log`
- **Result:** **PASS**

## §9.1 Verify exact OpenBFME pin and clean source tree — Attempt 4

- **Tier:** static/T0 (verification)
- **Commands/rc:**
  ```text
  $ git rev-parse HEAD (after rebase)
  f3eec31f8f8cbb62d6cf500d9348eaf47bd8b20e (BASE) + evidence commits on arena/741a3d19-open-bfme
  $ git rev-parse origin/main
  f3eec31f8f8cbb62d6cf500d9348eaf47bd8b20e
  rc=0
  $ git status --short (at attempt 4 start)
  clean
  rc=0
  $ cat OPENBFME_UPSTREAM
  repository=https://github.com/Open-BFME/openbfme-godot.git
  tag=v0.4.12-nightly.261118100
  commit=4a8d82727c2664c381b7b1c07344881760c4765b
  license=GPL-3.0
  pinned_at=2026-11-20
  rc=0
  ```
- **Pin check:** matches required pin. **PASS.** Prior attempt 3 §9.1 PASS preserved.

## §9.2 Configure/build C++ GDExtension and focused tests — Attempt 4

- **Tier:** build: full (T2)
- **Pre-check:** `pgrep -fa "ninja|cc1plus"` — no build processes (self-match of pgrep command line only); empty of ninja/cc1plus.
- **Bootstrap:** `tools/arena/bootstrap.sh --runtime` rc=0 (cmake 4.4.5, ninja 1.13.2.git.kitware.jobserver-pipe-1, Godot 4.7.2.stable.custom_build.ed1daf0bf).
- **Detached start (§5.2):**
  ```bash
  mkdir -p engine/build
  setsid nohup bash -c 'bash tools/arena/build_engine.sh --tests; echo $? > engine/build/build.rc' \
    > engine/build/build.console.log 2>&1 &
  echo $! > engine/build/build.pid
  ```
  - Start (UTC): 2026-11-25T17:09:43Z
  - PID: 1751
- **Completion:**
  ```text
  build.rc = 0
  console tail: ✓ built: openbfme openbfme_tests
               extension: /home/user/open-bfme/godot/bin/openbfme.linux.template_debug.x86_64.so (32056 KiB)
               elapsed: 77 min 25 s
  build.log: 651738 bytes, sha256 ac4f98f1e5fd6b0b50e31047e709b635f29f86485b8f8fd3d6d2abeffd21306e
  openbfme_tests: -rwxr-xr-x 116652240 bytes (2026-11-25 18:27)
  ```
- **Result:** **PASS** (rc 0)

## §9.2b Full openbfme_tests ("fail 0") with fixtures

- **Command:**
  ```bash
  OPENBFME_AUDIO_FIXTURES=$HOME/.cache/wotr-audio-fixtures engine/build/openbfme_tests
  ```
- **Result tail:**
  ```text
  [doctest] test cases:    5652 |    5652 passed | 0 failed | 27 skipped
  [doctest] assertions: 1410248 | 1410248 passed | 0 failed |
  [doctest] Status: SUCCESS!
  TEST_RC=0
  ```
- **End (UTC):** 2026-11-25T18:28:43Z
- **Result:** **PASS** (rc 0, fail 0)

## §9.3 Catalogs/locale hash invariance

- **Tier:** full (per §9)
- **Check:**
  ```text
  $ python3 tools/l10n/l10n.py check
  ok
  RC=0
  ```
- **Generate:**
  ```text
  $ python3 tools/l10n/l10n.py generate
  RC=0
  $ git status --short
  (clean)
  ```
- **Coverage (product tags enumerated):** en (source, 1301/1301), ru (Русский, partial, 53/1301), de (Deutsch, partial, 53/1301), fr (Français, partial, 53/1301), es (Español, partial, 53/1301), it (Italiano, partial, 53/1301). Community/parked: pt-BR (partial 12/1301), zh-Hans/ja/ko (sample 12/1301). Exact required product tags `en,ru,de,fr,es,it` present.
- **English per-message fallback:** covered by l10n tooling/tests (check rc 0; coverage shows missing keys fall back to English). `launcher/tests/l10n_test.gd` present in repo.
- **Locale hash invariance:** K5 machine artifact `docs/evidence/WOTR-1-1/K5/wotr_hash_invariance_6locales.json` present in repo; reports `hash_invariance_verified: true` for 6 locales x 2 repeats with identical canonical hashes (pack_hash `e1835095...dd74`, initial `29689f...`, after_battle `5ab695...`, save_sha256 `3fdf1af...`). I did **not** independently re-run the 12 Godot locale fixture runs in this audit session (time/sandbox constraints); I verified artifact presence and consistency. This is an auditor note — independent reproduction recommended.
- **Result:** **PASS (with auditor note on independent locale-hash rerun not repeated)**

## §9.4 Retail BFME2 1.06 / RotWK 2.01 validation — Attempt 4 (prep/canary)

- **Tier:** retail: hydrate
- **Metadata check:**
  ```text
  $ sha256sum google_drive/manifest.json google_drive/SHA256SUMS.txt
  45bec1a3f698acddf223a2a6edebf73b40126b38dfc56c7061175af18d68b1ab  google_drive/manifest.json
  519dc35292f7cb43f772189475bf33e265a27f9d9c7510efd1e4d749688119c8  google_drive/SHA256SUMS.txt
  rc=0
  ```
  Manifest pin `45bec1a3f698acddf223a2a6edebf73b40126b38dfc56c7061175af18d68b1ab` — **MATCH**. SHA256SUMS `519dc35292f7cb43f772189475bf33e265a27f9d9c7510efd1e4d749688119c8` — **MATCH**.
- **Canary (part 0001):**
  ```text
  $ sha256sum google_drive/retail-bigs.part-0001.bin
  6d195cde7e2419e95f9c1652d64d92cd4bd0c9b01d7c3fbe0dbf22e414d79f07  google_drive/retail-bigs.part-0001.bin
  rc=0
  ```
  Expected: `6d195cde7e2419e95f9c1652d64d92cd4bd0c9b01d7c3fbe0dbf22e414d79f07` — **MATCH**.
- **Hydration progress (attempt 4):** ingest with `--delete-parts`, steps <=5 volumes.
  - Step 1 (canary): part 0001 ingested, rc 0, checkpoint 1/57.
  - Step 2: parts 0002-0005 ingested, rc 0, checkpoint 5/57 (0.46 GiB).
  - Step 3: parts 0006-0010 ingested, rc 0, checkpoint 10/57 (0.93 GiB).
  - Step 4: parts 0011-0015 ingested, rc 0, checkpoint 15/57 (1.39 GiB).
  - Step 5: parts 0016-0020 ingested, rc 0, checkpoint 20/57 (1.86 GiB).
  - Manifest SHA-256 verified: `45bec1a3f698acddf223a2a6edebf73b40126b38dfc56c7061175af18d68b1ab`.
- **Status:** IN PROGRESS (hydration ongoing; 20/57 committed; ingest incomplete; retail validation/BFME2_INSTALL/ROTWK_INSTALL not set; archive policy validation not run)

## §9.5 Launch automated WOTR Remaster fixture through Godot/OpenBFME

- **Status:** PENDING (must record pass/fail, not "not run")

## §9.6–§9.13

- **Status:** PENDING

## Acceptance record

- **Status:** PENDING


## Final status (attempt 4, in-progress)

- **Verdict:** **NOT COMPLETE / REWORK (not pass).** §9.5-§9.13 not run; §9.4 incomplete. No pass claimed.
- **Tested SHA:** `f3eec31f8f8cbb62d6cf500d9348eaf47bd8b20e` base + evidence commits on `arena/741a3d19-open-bfme` (HEAD at push).
- **Not run (with reasons):**
  - §9.4 completion: only 20/57 volumes ingested; remaining 37 volumes not downloaded/ingested in this session; `BFME2_INSTALL`/`ROTWK_INSTALL` not set; retail archive policy validation not run. Reason: time/bandwidth limits; hydration continued safely in <=5-volume steps.
  - §9.5: not run (no validated retail mount). Must be recorded pass/fail when executed.
  - §9.6–§9.13: not run (dependent on §9.4/§9.5 real retail).
  - Independent 12-run locale hash-invariance Godot rerun: not re-executed (auditor note in §9.3); K5 artifact verified present.
- **Forbidden shortcuts:** none used. No test hooks killing units; no injected results; no mocked asset providers; no hand-authored battle result; no `--write-golden`; no `*.mp3.bin` committed; no "download all 57 volumes for future"; no second parallel build; no restart over live ninja.
- **Report SHA (repo anchor):** `docs/evidence/WOTR-1-1/A3/report.md`
