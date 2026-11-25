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

## §5.1 Audio fixtures (pre-§9.2/§9.13)

- **Tier:** none (per cost-gates §5.1)
- **Bundle source:** `Arena/wotr-remaster/artifacts/wotr-audio-fixtures.tar.gz` (Drive file id `1AQYEUURZn3dZJ8WJp8DjnmVfa0RGMzku`, per §5.1/cost-gates; not reproduced in evidence per §7)
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
- **Result:** PASS

## §9.1 Verify exact OpenBFME pin and clean source tree

- **Tier:** static/T0 (verification)
- **Commands/rc:**
  ```text
  $ git rev-parse HEAD
  a26c2425e372fe28904b2b5ab1cc2ab6581aa27f
  rc=0

  $ git branch --show-current
  arena/741a3d19-open-bfme
  rc=0

  $ git status --short
  (empty)
  rc=0

  $ git rev-parse origin/main
  bdc6df54720ea4ad633fb6adf3ba3d79000e2128
  rc=0

  $ cat OPENBFME_UPSTREAM
  # Machine-readable upstream pin for WOTR Remaster.
  repository=https://github.com/Open-BFME/openbfme-godot.git
  tag=v0.4.12-nightly.261118100
  commit=4a8d82727c2664c381b7b1c07344881760c4765b
  license=GPL-3.0
  pinned_at=2026-11-20
  rc=0
  ```
- **Pin check:** matches required pin `v0.4.12-nightly.261118100` / `4a8d82727c2664c381b7b1c07344881760c4765b`. **PASS (pin/source tree verified).**

## §9.2 Configure/build C++ GDExtension and focused tests

- **Tier:** build: full (T2)
- **Bootstrap re-run (sandbox /tmp cleared):** `tools/arena/bootstrap.sh --runtime` rc=0 (cmake 4.4.5, ninja 1.13.2, Godot 4.7.2.stable.custom_build.ed1daf0bf).
- **Build command:**
  ```bash
  export PATH="/tmp/buildenv/bin:$PATH"
  tools/arena/build_engine.sh --tests
  ```
- **Progress:** configure OK (mode=tests, Godot=ON, tests=ON), godot-cpp `507ed9d840c01a3c5b2a39af8bb4000bfac30bf5` populated. Incremental ninja resumes: ~2075/2618, ~2368/2618 (still compiling, no failure observed).
- **Status:** IN PROGRESS

## §9.3 Catalogs/locale hash invariance

- **Status:** PENDING

## §9.4 Retail BFME2 1.06 / RotWK 2.01 validation

- **Tier:** retail: hydrate
- **Status:** PENDING

## §9.5 Launch automated WOTR Remaster fixture through Godot/OpenBFME

- **Status:** PENDING (must record pass/fail, not "not run")

## §9.6–§9.13

- **Status:** PENDING

## Acceptance record

- **Status:** PENDING
