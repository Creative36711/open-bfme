# WOTR Remaster · Godot world (card WOTR-1.1-K4)

The minimal 2.5D strategic map of the War of the Ring remaster, the Godot
side of the mission (`docs/missions/WOTR-1.1.md`, §4.3 and §8 K4).

## What is here

```text
godot/wotr_remaster/
├── core/                     the boundary to the strategic core
│   ├── wotr_core_port.gd        the contract (command in, snapshot out)
│   ├── wotr_core_native_port.gd the adapter to the native core
│   └── native/                  the GDExtension that binds the C++ core
│       ├── wotr_core_native.cpp         the adapter (this card)
│       ├── wotr_battle_bridge_native.h  the native tactical bridge
│       ├── wotr_battle_bridge_native.cpp the native tactical bridge
│       ├── profiles/                     the tactical profiles (product data)
│       ├── CMakeLists.txt                builds the core + the binding
│       ├── wotr_core.gdextension         the manifest Godot loads
│       └── build_core_extension.sh       the build command
├── battle/                   the strategic -> tactical boundary
│   ├── wotr_battle_port.gd        the contract
│   ├── wotr_battle_bridge_port.gd the binding to the native bridge
│   ├── wotr_battle_fixture_port.gd the deterministic stand-in
│   └── wotr_battle_port_factory.gd picks the port of this build
├── world/                    the 2.5D map and its HUD
│   ├── wotr_world.gd              the world: input -> commands, state -> view
│   ├── wotr_cell_view.gd          one region (a 2.5D hex plate)
│   ├── wotr_army_view.gd          one army (a 2.5D banner)
│   ├── wotr_route_view.gd         the routes of the accepted moves
│   ├── wotr_world_hud.gd          the readout panel (Control/Container)
│   ├── wotr_world_theme.tres      the Theme with the control states
│   └── wotr_world.tscn            the scene
└── scenario/                 the automated route
    ├── wotr_headless_scenario.gd  select -> move -> conflict -> battle -> save
    └── run_scenario.sh             the command that runs it
```

## The rules the code keeps

1. **The core owns the state.** The world never writes a campaign field. It
   sends command intents (`playerId`, `clientSequence`, `commandId`, `armyId`,
   `targetRegionId`) and draws the state the core returns. The canonical C++
   core (`engine/src/GameLogic/WotrRemaster`, card WOTR-1.1-K2) is compiled
   into `core/native` unchanged; only the adapter is this card's work.
2. **No checksum is computed in GDScript.** Envelope, request and result
   checksums come from the core, so a Godot-side mistake cannot forge one.
3. **No ambient input.** The world reads no wall clock, no locale and no
   filesystem order for the campaign; the presentation only sorts by the ids
   the core reported.
4. **The text is a catalog id with an English fallback.** The eight product
   locales are card WOTR-1.1-K5 (`tools/l10n/catalog`). Until an id is in the
   catalog, the English source in `world/wotr_text.gd` is shown. A locale never
   changes a command, a state byte or a hash.
5. **One game process.** The battle port runs in the same process; nothing is
   launched for a battle.

## Build and run

```bash
# the native core extension (once; godot-cpp comes from the engine build)
godot/wotr_remaster/core/native/build_core_extension.sh

# the automated route, its screenshots and its report
godot/wotr_remaster/scenario/run_scenario.sh
```

The runner writes `report.json`, `run.log` and the three screenshots
(`01-initial.png`, `02-conflict.png`, `03-after-battle.png`) into its output
directory (default `/tmp/wotr-k4`).

## The battle port

`battle/wotr_battle_port_factory.gd` prefers the native bridge of this card
(`WotrBattleBridge.run_battle(request_json, profile_json) -> Dictionary`).

The bridge is a GDExtension class that runs OpenBFME's **own** War of the Ring
battle for the core's `BattleRequestV1`: it mounts the retail archives, builds
the armies of the request from the retail store data and calls the production
`LivingWorld::AutoResolveBattle`. The result it returns is the native
`BattleResultV1` — a Godot `Dictionary`, so the canonical integers of the
result reach the core without a JSON round-trip.

It needs the validated retail trees; without `ROTWK_INSTALL` and
`BFME2_INSTALL` it refuses the request instead of faking a fight:

```bash
BFME2_INSTALL=<repo>/.retail/BFME2 ROTWK_INSTALL=<repo>/.retail/RotWK \
  godot/wotr_remaster/scenario/run_scenario.sh
```

The tactical profile is product data: `core/native/profiles/vanilla_rotwk_201.json`
names the retail stack template every catalog unit of the pack fights as. The
bridge reads the retail side, the profile names the product side; neither
invents the other.

When the retail trees are absent, the deterministic fixture answers instead,
and every run records which port it used in `report.json`
(`battle_port.native`). The fixture is not a native tactical capture: it
resolves the BattleRequestV1 with a fixed rule and reports no tactical
statistics and no capture frame.
