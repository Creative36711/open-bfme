#!/usr/bin/env python3
"""Lane GHPR-1: the Python tests the public pull request check runs (.github/workflows/pr.yml, CONTRIBUTING.md "What we check").

SUBSET is the test files that pass on a clean GitHub runner: Python, pytest and PyYAML (PUBLICCI-1; no capstone / pefile), no game folders, no
built engine, no Godot. It was measured by running every test file of the public tree that way (GHPR-1, 2026-10-18: 56 files of 89
passed with no failure; the rest need capstone / pefile, the game files or a build, or only skip there, and the full gate runs them all).
The operations-only tests under tools/net are left out (they leave the public tree with PUBLISH-2). Fails closed: a listed file that does not exist, a pytest failure, or a
listed file in which no test passed (it collected nothing or only skipped) is a failure.

    python3 tools/release/pr_pytest.py [--shard LANE/COUNT] [pytest args]

--shard gives one lane of SUBSET to one runner. The lanes are the same tests redistributed over equally sized GitHub runners (a job gets
four CPUs; a pull request's wall time is one runner's queue plus the tests of its lane), because narrowing what runs by the changed paths is
not sound here: tests read this repository's documents both directly (tools/test_stops_count.py, tools/release/test_release_tools.py) and
through helpers (tools/release/readme_testers.py), and read paths named docs/ or README.md that belong to their own throwaway repositories,
so no static rule tells the two apart (measured 2026-11-22, docs/evidence/ci-cost-2026-11-22.md). An unknown lane, a lane count below one
or a missing flag is an error, not a wider or narrower run.
"""
from __future__ import annotations

import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
SUBSET = [
    "tools/test_gdscript.py",
    "tools/audio/test_mp3_configure.py",
    "tools/audio/test_mp3_fixtures.py",
    "tools/audio/test_miles_model.py",
    "tools/audio/test_playback_bus_guard.py",
    "tools/fx/test_fx_ini_scan.py",
    "tools/horde_oracle/test_dump_tables.py",
    "tools/horde_oracle/test_rng_reference.py",
    "tools/hover/test_hover_inventory.py",
    "tools/l10n/test_l10n.py",
    "tools/maps/oracle/test_census.py",
    "tools/maps/oracle/test_resolve.py",
    "tools/menus/test_menu_inventory.py",
    "tools/move/test_jitter_report.py",
    "tools/net/test_cross_os_lockstep.py",
    "tools/net/test_mp3_perf_table.py",
    "tools/net/test_online_client.py",
    "tools/net/test_user_agents.py",
    "tools/object_oracle/test_object_oracle.py",
    "tools/opts/test_opts1_hotkeys.py",
    "tools/opts/test_ping_wheel_keys.py",
    "tools/path/test_no_libm.py",
    "tools/path/test_path_oracle.py",
    "tools/perf3/test_render_frame_no_wait.py",
    "tools/perf3/test_renderwait1_per_frame.py",
    "tools/platform_fake/test_platform_contract.py",
    "tools/platformicon/test_make_icons.py",
    "tools/qa/test_qa_matrix.py",
    "tools/qa/test_scenario_players.py",
    "tools/release/test_actions_build.py",
    "tools/release/test_aio.py",
    "tools/release/test_autorelease.py",
    "tools/release/test_big_entries.py",
    "tools/release/test_community_prs.py",
    "tools/release/test_public_push_guard.py",
    "tools/release/test_export_checks.py",
    "tools/release/test_launcher_tools.py",
    "tools/release/test_net_guard_files.py",
    "tools/release/test_pdb_tools.py",
    "tools/release/test_pr_scope.py",
    "tools/release/test_release_tools.py",
    "tools/release/test_windows_player_walk.py",
    "tools/release/test_wine_suite.py",
    "tools/render/test_d3d9_disasm.py",
    "tools/render/test_sharpness.py",
    "tools/render/test_standing_water_grid_oracle.py",
    "tools/replay/test_rotwk_replay.py",
    "tools/retail_oracle/test_helpers.py",
    "tools/rw_object_model/test_module_coverage.py",
    "tools/rw_object_model/test_rw_object_model.py",
    "tools/save/test_closed_sets.py",
    "tools/save/test_retail_save.py",
    "tools/script/test_script_tools.py",
    "tools/sim/test_float_chars.py",
    "tools/sim/test_sim_audit.py",
    "tools/skirmish_ai/test_tactic_draws.py",
    "tools/test_coverage_report.py",
    "tools/test_doctest_counts.py",
    "tools/test_precommit_containers.py",
    "tools/test_precommit_audio.py",
    "tools/release/test_external_open.py",
    "tools/test_precommit_pins.py",
    "tools/test_precommit_tolerance.py",
    "tools/test_stops_count.py",
    "tools/weapon/test_weapon_binary_facts.py",
    "tools/weapon/test_weapon_dump_tables.py",
    "tools/weapon/test_weapon_no_libm.py",
]
# single tests of listed files that need more than a pull request's checkout has: {node id: why}
DESELECT = {
    "tools/release/test_autorelease.py::test_bridge_rules": "reads published launchers from release tags (a shallow checkout has no tags)",
}

# seconds of test time per file, measured on 2026-11-22 (`pytest -q --durations=0` over SUBSET, one process, no build, no game folders:
# 3407 s of test time in 1094 s of wall time on four workers). The gate spends about ten minutes of a pull request's eleven because one
# runner holds four CPUs, and 91 % of that time is three files of tools/release. SHARDS splits the same tests over that many runners of the
# same free size, so the wall time follows the CPUs GitHub gives a job (measured here, 2026-11-22: four lanes of 4 workers ~= 213 s of test
# time each). The numbers only balance the lanes: no test is dropped or added by them, and the union of the lanes is SUBSET (a test of
# tools/release/test_community_prs.py says so). A file missing from COST is charged COST_DEFAULT, which cannot hide it from a lane.
COST = {
    "tools/release/test_autorelease.py": 2406.2,
    "tools/release/test_actions_build.py": 405.8,
    "tools/release/test_community_prs.py": 301.9,
    "tools/save/test_closed_sets.py": 50.2,
    "tools/perf3/test_renderwait1_per_frame.py": 45.2,
    "tools/sim/test_float_chars.py": 30.4,
    "tools/net/test_cross_os_lockstep.py": 30.4,
    "tools/release/test_release_tools.py": 29.3,
    "tools/release/test_launcher_tools.py": 23.6,
    "tools/release/test_public_push_guard.py": 13.9,
    "tools/release/test_net_guard_files.py": 11.4,
    "tools/audio/test_playback_bus_guard.py": 11.4,
    "tools/l10n/test_l10n.py": 7.4,
    "tools/test_precommit_tolerance.py": 7.1,
    "tools/test_precommit_pins.py": 6.9,
    "tools/net/test_online_client.py": 6.6,
    "tools/release/test_export_checks.py": 5.4,
    "tools/platformicon/test_make_icons.py": 3.3,
    "tools/test_precommit_containers.py": 2.0,
    "tools/release/test_windows_player_walk.py": 1.8,
    "tools/release/test_wine_suite.py": 1.6,
}
COST_DEFAULT = 1.0
SHARDS = 4


def cost_of(rel: str) -> float:
    """the measured seconds of test time of `rel` (a file nobody measured is charged little, which the balance check tolerates)"""
    return COST.get(rel, COST_DEFAULT)


def lanes(shards: int) -> list[list[str]]:
    """SUBSET split into `shards` lists of about equal measured cost (longest processing time first). Every listed file lands in exactly one
    lane, so a pull request that runs all the lanes runs the whole SUBSET and nothing twice."""
    if shards < 1:
        raise SystemExit(f"PR PYTEST FAILED: --shard needs a lane count of 1 or more, not {shards}")
    buckets: list[list[str]] = [[] for _ in range(shards)]
    load = [0.0] * shards
    for rel in sorted(SUBSET, key=cost_of, reverse=True):
        i = load.index(min(load))
        buckets[i].append(rel)
        load[i] += cost_of(rel)
    return [sorted(b) for b in buckets]


def take_shard(argv: list[str]) -> list[str]:
    """the files `--shard <i>/<n>` picks out of SUBSET, and `argv` with the flag removed; without the flag, the whole SUBSET"""
    if "--shard" not in argv:
        return list(SUBSET)
    i = argv.index("--shard")
    if i + 1 >= len(argv) or "/" not in argv[i + 1]:
        raise SystemExit("PR PYTEST FAILED: --shard wants LANE/COUNT, e.g. --shard 1/4")
    spec = argv[i + 1]
    del argv[i:i + 2]
    lane, _, count = spec.partition("/")
    try:
        lane, count = int(lane), int(count)
    except ValueError:
        raise SystemExit(f"PR PYTEST FAILED: --shard wants two integers, got {spec!r}") from None
    buckets = lanes(count)
    if not 1 <= lane <= count:
        raise SystemExit(f"PR PYTEST FAILED: --shard {spec} has no lane in 1..{count}")
    print(f"PR PYTEST: lane {lane} of {count}, {len(buckets[lane - 1])} of {len(SUBSET)} files "
          f"({cost_of_lane(buckets[lane - 1]):.0f} s of measured test time)")
    return buckets[lane - 1]


def cost_of_lane(files: list[str]) -> float:
    return sum(cost_of(f) for f in files)


def main(argv: list[str]) -> int:
    files = take_shard(argv)
    missing = [f for f in files if not (REPO / f).is_file()]
    if missing or not files:
        print(f"PR PYTEST FAILED: listed test files missing: {', '.join(missing) or '(the list is empty)'}")
        return 1
    with tempfile.TemporaryDirectory() as tmp:
        junit = Path(tmp, "junit.xml")
        rc = subprocess.run([sys.executable, "-m", "pytest", "-q", "-p", "no:cacheprovider", "-o", "junit_family=xunit1",
                             f"--junitxml={junit}", *(f"--deselect={d}" for d in DESELECT), *argv, *files], cwd=REPO).returncode
        try:
            root = ET.parse(junit).getroot()
        except (OSError, ET.ParseError):
            print("PR PYTEST FAILED: pytest wrote no results")
            return 1
    cases = list(root.iter("testcase"))
    passed = [c for c in cases if not any(c.find(k) is not None for k in ("failure", "error", "skipped"))]
    # every listed file ran at least one test that passed (a file that collects nothing, or only skips, is not checking anything)
    idle = [f for f in files if not any(c.get("file") == f for c in passed)]
    print(f"PR PYTEST: {len(passed)} of {len(cases)} tests passed in {len(files)} files (pytest exit {rc})")
    for f in idle:
        print(f"PR PYTEST: no test of {f} passed")
    if rc != 0 or idle:
        print("PR PYTEST FAILED")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
