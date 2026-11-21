#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""Writes cases.h for the FM1 Rack benchmark firmware from the factory presets' engine calls
(firmware/tests/presets-fm1-calls.json: the FM-1's own set, written by rack/fm1_presets.py).

  python3 make_cases.py            (from this folder; cases.h is committed, so only after the presets change)
  python3 make_cases.py --full     cases_full.h instead: every preset whole, for bench_host's check of the above

INIT is stored whole. Each other preset is stored as what it changes from INIT: its six modules first, then every
call whose value differs from INIT's (a parameter, its range, a matrix row, a macro, ...). Loading INIT and then
the difference gives the same engine state as the preset's own calls (bench_host checks this)."""
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SRC = HERE.parent.parent / "tests" / "presets-fm1-calls.json"

# the presets the benchmark plays: only modules the Lite firmware has (no Elements, Warps, Marbles)
# The cases are built on eight presets of the first (1.9) set, kept in tests/bench-base-calls.json so the module
# measurements stay comparable from run to run; then every current factory preset is played (Nov 2026: the new 57)
BASE = ["INIT", "FOLDED BASS", "KARPLUS", "TIDAL FM", "GRID GATE", "CH GLASS PNO", "BS SUB MONO", "LD SQUARE CHIP"]
BASE_SRC = HERE.parent.parent / "tests" / "bench-base-calls.json"
P2INIT = "P2 INIT"
PRESETS = list(BASE)
# the factory presets played: the FM-1's own set (14 Nov 2026), its heaviest 20 by the 13 Nov run's figures
# (presets_fm1.py), and the three Rings ones, as all 67 don't fit the app slot (27 didn't: 1.4 KB over)
# FM1 Phase 2 (14 Nov 2026): CH VIBES, PD TEMPO GATE, CH DRAWBAR, PD RESONANT, CH SOFT PIANO, PD CHOIR and CH BRIGHT EP
# left out to make room for the GRAIN cases (4.5 KB); all seven were measured on 13 Nov (results/fm1-bench-2026-11-13.csv)
# FM1 Phase 2 (21 Nov 2026, update 59): the PRESETS group plays the CURRENT factory bank (the 45 presets after INIT, in the firmware's order), not the 25 of
# the earlier sets (their names are gone from firmware/tests/presets-fm1-calls.json); the heaviest-first cut that kept the image in the app slot is no longer made
PLAYED = [p["name"] for p in json.loads(SRC.read_text()) if p["name"] != "INIT"]
LITE_OUT = {3, 11, 22}   # Elements, Warps, Marbles (Clouds is in the Phase 2 firmware: the CLOUDS group; Plaits is out, see below)

# op codes (bench.c applies them)
FN = {"set_module": 1, "set_param": 2, "set_meta": 3, "set_route": 4, "set_macro": 5, "set_master": 6,
      "set_master_fx": 7, "set_legato": 8, "set_vintage": 9, "set_ext_slew": 10, "set_tune": 11}
KEYLEN = {"set_module": 1, "set_param": 2, "set_meta": 2, "set_route": 1, "set_macro": 1, "set_master": 0,
          "set_master_fx": 0, "set_legato": 0, "set_vintage": 0, "set_ext_slew": 1, "set_tune": 1}


def op(c):
    """one call as (fn, a, b, c, d, x, y, z, w): small ints in a..d, values in x..w"""
    f, args = c[0], c[1:]
    if f == "set_module":
        return (FN[f], args[0], args[1], 0, 0, 0, 0, 0, 0)
    if f == "set_param":
        return (FN[f], args[0], args[1], 0, 0, args[2], 0, 0, 0)
    if f == "set_meta":
        return (FN[f], args[0], args[1], 0, 0, args[2], args[3], args[4], 0)
    if f == "set_route":                        # i, src, slot, param, amount, via
        return (FN[f], args[0], args[1], args[2], args[3], args[4], args[5], 0, 0)
    if f in ("set_macro", "set_ext_slew", "set_tune"):
        return (FN[f], args[0], 0, 0, 0, args[1], 0, 0, 0)
    if f == "set_master_fx":
        return (FN[f], 0, 0, 0, 0, *args[:4])
    if f in ("set_master", "set_legato", "set_vintage"):
        return (FN[f], 0, 0, 0, 0, args[0], 0, 0, 0)
    raise SystemExit(f"make_cases: unknown call {f}")


def final_state(calls):
    """the last value of each (call, key), in first-seen order"""
    st = {}
    for c in calls:
        if c[0] == "set_hw":
            continue
        if c[0] == "set_tune" and c[2] == 0:    # the engine's init() leaves every note in tune
            continue
        k = (c[0], *c[1:1 + KEYLEN[c[0]]])
        st.pop(k, None)
        st[k] = c
    return st


def fnum(v):
    v = float(v)
    s = repr(v)
    return s + ("f" if ("." in s or "e" in s) else ".0f")


def main():
    full = "--full" in sys.argv
    sfx = "_FULL" if full else ""
    data = {p["name"]: p for p in json.loads(SRC.read_text()) if p["name"] != "INIT"}
    data_p2init = next(p for p in json.loads(SRC.read_text()) if p["name"] == "INIT")
    data.update({p["name"]: p for p in json.loads(BASE_SRC.read_text())})
    for p in data.values():                      # the knob layer's targets are not in cases.h (the knobs are at rest in the PRESETS group; modsum_cases.h has them)
        p["calls"] = [c for c in p["calls"] if c[0] != "set_macro_target"]
    osc = {n: p["fm1"]["osc"] for n, p in data.items() if p.get("fm1", {}).get("osc")}
    cap = {n: p["fm1"]["fm1_voices"] for n, p in data.items() if p.get("fm1", {}).get("fm1_voices")}
    OSC_KEYS = ["WAVE", "OCTAVE", "SEMI", "FINE", "PW", "OSC LVL", "MAIN LVL"]   # the firmware's OSC: AMP 24-30
    for n, o in osc.items():
        if n in data and n not in BASE:
            data[n]["calls"] += [["set_param", 6, 24 + OSC_KEYS.index(k), float(v)] for k, v in o.items()]
    # FM1 Phase 2 (15 Nov 2026): the firmware's own INIT (a Braids tone: Plaits is out), kept under another name as the
    # base of the CLOUDS group; the eight BASE presets above still ask for Plaits and play Braids in this build
    data[P2INIT] = dict(data_p2init, name=P2INIT)
    PRESETS[:] = BASE + [P2INIT] + PLAYED
    init = final_state(data["INIT"]["calls"])
    out = ["// SPDX-License-Identifier: GPL-3.0", "/* Generated by make_cases.py from firmware/tests/presets-fm1-calls.json: do not edit. */",
           "/* An op: x, the ints y z w, then fn (make_cases.py FN) and the small ints a..d (16 bytes: update 59; every y z w of the presets is a whole number). */", ""]
    table = []
    for name in PRESETS:
        p = data[name]
        st = final_state(p["calls"])
        mods = {c[1]: c[2] for c in p["calls"] if c[0] == "set_module"}
        if any(m in LITE_OUT for m in mods.values()):
            raise SystemExit(f"make_cases: {name} uses a module the Lite firmware leaves out")
        if name == "INIT" or full:
            ops = list(st.values())
        else:
            ops = [["set_module", s, mods.get(s, 0)] for s in range(6)]
            ops += [c for k, c in st.items() if c[0] != "set_module" and init.get(k) != c]
            missing = [k for k in init if k not in st and k[0] != "set_module"]
            if missing:
                raise SystemExit(f"make_cases: {name} leaves {missing[:3]} as INIT has them; can't diff")
        ident = "OPS_" + "".join(ch if ch.isalnum() else "_" for ch in name) + sfx
        out.append(f"static const bop_t {ident}[] = {{")
        for c in ops:
            o = op(c)
            out.append("    {%s, %d, %d, %d, %d, %d, %d, %d, %d}," % (fnum(o[5]), *(int(v) for v in (o[6], o[7], o[8])), *o[:5]))
        out.append("};")
        mode = int(p.get("mode", 0))
        v = min(int(p["voices"]), cap.get(name, 99))   # presets.json "fm1_voices": as the firmware starts it
        table.append(f'    {{"{name}", {ident}, {len(ops)}, {v}, {mode}}},')
    out += ["", f"static const bpreset_t PRESETS{sfx}[] = {{", *table, "};",
            f"#define NPRESETS{sfx} {len(PRESETS)}", ""]
    name = "cases_full.h" if full else "cases.h"
    (HERE / name).write_text("\n".join(out))
    print(f"{name}: {len(PRESETS)} presets, {sum(1 for l in out if l.startswith('    {') and 'OPS_' not in l)} ops")


if __name__ == "__main__":
    sys.exit(main())
