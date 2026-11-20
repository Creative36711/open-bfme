# Legacy WOTR map reference

Reference-only inputs from `Creative36711/wotr` commit
`fd12e3a3a52ad485f95af3ff559610fd2778570b`:

- `map-reference.jpg` — 5120×4096 visual/geographic registration reference;
- `hex-grid-v43.json` — only the old world's grid config and 1,412 authored
  cells, extracted from `public/mods/default/world.json` version 43.

These are not runtime React/Tauri assets and are not the canonical campaign
state. The remaster importer may use them to seed a new Godot/C++ map resource.
Any production conversion must be deterministic and checked in as a separate,
versioned output.
