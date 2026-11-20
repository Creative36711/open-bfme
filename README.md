# WOTR Remaster

A Godot-native War of the Ring remaster built as a maintained downstream of
[OpenBFME](https://github.com/Open-BFME/openbfme-godot). The strategic state and
deterministic rules live in C++; Godot owns presentation, input, camera, and
editor tooling. The project does not use Win32 menu automation, coordinate
clicks, or OCR.

The current engine baseline is OpenBFME `v0.4.12-nightly.261118100`
(`4a8d82727c2664c381b7b1c07344881760c4765b`). It currently has **842 open stops**,
tracked in `docs/STOPS.md`. See the architecture records in
`docs/architecture/` and the update procedure in `docs/UPSTREAM_SYNC.md`.

## Agent workflow

This is one repository for product code, OpenBFME integration, process documents,
and the ScriptCat transport. Start with `AGENTS.md`, `docs/REGULATION.md`,
`docs/PHASES.md`, the active specification in `docs/missions/`, and the tiered
build/retail policy in `docs/agents/cost-gates.md`. Visual cards also follow the
hybrid Godot/SVG/generated-raster contract in `docs/tooling/ART-PIPELINE.md`.
ScriptCat 1.16.3 and its WOTR profile live in `tools/script-cat/`; its complete
local gate is `npm run verify:scriptcat`.

## Arena bootstrap

The repository pins a Godot 4.7.2 build and SwiftShader by source commit and
checksums. The bootstrap hydrates them only when requested, so an Arena sandbox
can render without a GPU or access to GitHub release assets while the public
repository stays free of the 63-MiB tool archives.

```bash
tools/arena/bootstrap.sh --runtime
tools/arena/build_engine.sh
```

Art tooling (Blender as `bpy`, Pillow, and chromakey):

```bash
tools/arena/bootstrap.sh --art
```

## Retail assets

BFME2 and RotWK installations are proprietary runtime inputs and must never be
committed or remain in reachable Git history. OpenBFME requires the original BIG
files byte-for-byte and validates their sizes and MD5 hashes. Local data belongs
in the ignored `.retail/` directory. Agents must not hydrate the 5.249-GiB Drive
snapshot unless the card declares `retail: hydrate`; reuse a verified local copy
when possible and follow `docs/agents/cost-gates.md`. The current history audit
and safe purge runbook are in `docs/RETAIL-HISTORY-AUDIT.md`.

```text
.retail/BFME2
.retail/RotWK
```

### Pack on the machine with BFME2 1.06 and RotWK 2.01

```bash
python tools/retail_big_transfer.py pack \
  --bfme2 "/path/to/BFME2" \
  --rotwk "/path/to/RotWK" \
  --out "/path/to/openbfme-retail-transfer"
```

This creates `manifest.json`, `SHA256SUMS.txt`, and 95-MiB
`retail-bigs.part-*.bin` volumes suitable for Arena's 100-MiB connector limit.
The current retail set contains 213 archives in 57 volumes (5.249 GiB).

On Windows PowerShell the same pack step is:

```powershell
python tools/retail_big_transfer.py pack `
  --bfme2 "D:\Games\BFME2" `
  --rotwk "D:\Games\RotWK" `
  --out "D:\openbfme-retail-transfer"
```

Upload `manifest.json` and a single first volume as a canary before starting the
bulk transfer. Validate that volume and the ingest path in Arena; only then
upload the remaining volumes. `SHA256SUMS.txt` is optional but recommended.

### Reset-safe, low-disk restoration in Arena

Place `manifest.json` and one or more consecutive volumes in the same staging
folder, then run:

```bash
python tools/retail_big_transfer.py ingest \
  --manifest google_drive/manifest.json \
  --out .retail \
  --delete-parts
```

`ingest` authenticates a volume before writing it, atomically checkpoints after
each volume, and then deletes that local volume. A retry resumes at the next
volume; only a small streaming buffer and one 95-MiB input volume are needed.
Archive MD5/SHA-256 and volume SHA-256 are checked before completion.

If all volumes are already local, the original all-at-once command remains
available:

```bash
python tools/retail_big_transfer.py unpack \
  --manifest /path/to/manifest.json \
  --out .retail \
  --delete-parts
```

At runtime:

```bash
export BFME2_INSTALL="$PWD/.retail/BFME2"
export ROTWK_INSTALL="$PWD/.retail/RotWK"
```
