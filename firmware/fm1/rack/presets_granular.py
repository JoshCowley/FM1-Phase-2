# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2: the macro grammar and the effect helpers the preset batches share (_param, _raw, _span, REV, DLY, TI,
PADAMP, SLOWLFO). Until 2.47 this file also held the granular batch (Braids into Clouds); 2.48 has no Clouds and the batch is gone.

A macro is written as (destination, value at 0, value at 1) in the parameter's own units (an option's name for a list):
the matrix amount follows (value at 1 - value at 0) / span, and the base is the value at 0.
Values stay on a 0.05 grid where they can (the preset table holds 256 different floats in all: make_presets.py)."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

_D = None
SLOTN = {"SRC": "SRC", "SHP": "SHP", "MOD A": "MODA", "MOD B": "MODB", "FX 1": "FX1", "FX 2": "FX2"}


def defs():
    global _D
    if _D is None:
        import fm1_presets
        _D = fm1_presets.defs()
    return _D


def _param(slots, dest):
    """(slot key or 'AMP'/'LFO', label, parameter def) for a destination such as "FX 1.DENSITY" """
    D = defs()
    sl, lab = dest.split(".", 1)
    if sl == "AMP":
        mod = D["AMP"]
    elif sl == "LFO":
        mod = D["LFOS"]
    else:
        mod = D["MODULES"][slots[SLOTN[sl]]]
    for pg in mod["pages"]:
        for p in pg:
            if p["l"] == lab or p.get("alias") == lab:
                return ("AMP" if sl == "AMP" else "LFO" if sl == "LFO" else SLOTN[sl]), lab, p
    raise SystemExit(f"presets_granular: no parameter {dest}")


def _raw(p, v):
    if p["t"] == "list":
        return float(p["opts"].index(v)) if isinstance(v, str) else float(v)
    return float(v)


def _span(p):
    t = p["t"]
    return float(len(p["opts"]) - 1) if t == "list" else 2.0 if t == "bip" else float(p["max"] - p["min"]) if t == "int" else 1.0


# ----------------------------------------------------------------------------------------------------------------------
def REV(typ="HALL", size=0.8, mix=0.3, damp=0.5, **kw):
    return {"TYPE": typ, "SIZE": size, "DAMP": damp, "DIFFUSE": 0.75, "MIX": mix, "PRE": 0.1, "LO CUT": 0.25, **kw}


PADAMP = {"ATTACK": 0.3, "DECAY": 0.5, "SUSTAIN": 0.9, "RELEASE": 0.6, "VEL AMT": 0.3, "SPREAD": 0.6, "LEVEL": 0.5}
SLOWLFO = {"SHAPE 1": "SINE", "RATE 1": 0.2, "SYNC 1": "FREE", "SHAPE 2": "DRIFT", "RATE 2": 0.2, "SYNC 2": "FREE",
           "SHAPE 3": "TRI", "RATE 3": 0.1, "SYNC 3": "FREE"}


def TI(f=0.25, rng="MID", shape=0.5):
    return {"MODE": "LOOP", "FREQ": f, "SHAPE": shape, "SLOPE": 0.0, "SMOOTH": 0.5, "RANGE": rng}


def DLY(t="1/8D", fb=0.3, mix=0.2, mode="PING", **kw):
    return {"TIME": t, "FEEDBACK": fb, "TONE": 0.5, "MIX": mix, "MODE": mode, "LO CUT": 0.3, **kw}
