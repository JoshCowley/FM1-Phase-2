# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2 update 17: the third batch of factory presets, made for this unit: the everyday sounds a player expects to find
# on any synth (pads, strings, brass, choir, leads, basses, a pluck, a bell, an electric piano, an organ, a sync lead, an
# evolving texture and a riser), each kept alive by the modulation routing, most of them subtly. presets_fm1.py puts them at
# bank positions 33 to 48, straight after batch 2 and before the older presets.
"""One grammar for every preset, as batch 2 (presets_batch2.py has the long form):
  K1 MORPH    one dial, many areas at once: source timbre and colour, the filter, an envelope time, a modulation speed, an effect
  K2 MOTION   how much and how fast the modulation moves (the routes whose "via" is M2 are K2's depth; a rate is K2's speed)
  K3 SPACE    the effects: reverb, delay, chorus, width
  K4 TEXTURE  grit, resonance, harmonic density, drift
Each preset has a movement level: SUBTLE (felt more than heard: a few cents of pitch, a few percent of cutoff, chorus and pan
motion, slow timbre drift), MEDIUM (audible but supporting) or BOLD (the movement is the point). The matrix has 12 rows
(engine/rack.cc NROUTE 18: 12 matrix rows, then two direct targets for each LFO): the four macros and a preset's own routes
share them. The LFOs' six direct targets and the filter's own envelope (ENV SRC OWN) cost no row. Values stay on a 0.05 grid
and reuse numbers the bank already holds (the packed float table has 256 slots: make_presets.py)."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from presets_granular import REV, DLY, TI
from presets_batch2 import B, CHO, FN, AM, LF


def FL(t="LP", cut=0.5, reso=0.1, drive=0.1, env=0.0, key=0.5, vel=0.0, src="AMP", fa=0.01, fd=0.4, fs=0.3, fr=0.35, **kw):
    d = {"TYPE": t, "CUTOFF": cut, "RESO": reso, "DRIVE": drive, "ENV AMT": env, "KEY TRK": key, "VEL AMT": vel, "ENV SRC": src,
         "F ATK": fa, "F DEC": fd, "F SUS": fs, "F REL": fr}
    d.update(kw)
    return d


def PK(k1=0.3, k2=0.0, func="LFO", **kw):
    d = {"FUNC": func, "K1": k1, "K2": k2, "GATE": "NOTE", "SUSTAIN": 0.0, "INVERT": "OFF"}
    d.update(kw)
    return d


def TD(mode="LOOP", f=0.2, shape=0.5, slope=0.0, smooth=0.5, rng="MID", **kw):
    d = {"MODE": mode, "FREQ": f, "SHAPE": shape, "SLOPE": slope, "SMOOTH": smooth, "RANGE": rng, "GATE": "NOTE", "INVERT": "OFF"}
    d.update(kw)
    return d


S5 = {"SRC": "BRAIDS", "SHP": "FILTER"}
LEVEL = {"WARM PAD": 0.45, "STRING ENS": 0.35, "BRASS SECT": 0.35, "CHOIR AAH": 0.35, "SAW LEAD": 0.45, "HOLLOW LEAD": 0.4,
         "SUB BASS": 0.45, "ANALOG BASS": 0.4, "ACID BASS": 0.4, "SYNTH PLUCK": 0.6, "FM BELL": 0.45, "E PIANO": 0.5,
         "ROTARY ORGAN": 0.4, "HARD SYNC": 0.45, "ATMOSPHERE": 0.35, "NOISE SWEEP": 0.4}      # AMP LEVEL per preset, set from the measured loudness (0.05 grid): the batch2 way, without level_fm1.py


def batch3():
    L = []
    a = L.append

    # 33 -------------------------------------------------------------------------------------------------- WARM PAD
    a(B("WARM PAD", "PAD", {**S5, "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "////", "TIMBRE": 0.25, "COLOR": 0.4, "DRIFT": 0.2, "LEVEL": 1.0},
         "SHP": FL("LP", 0.45, 0.1, 0.1, 0.0, 0.5, 0.1),
         "MODA": TD("LOOP", 0.2, 0.5), "FX1": CHO(0.4, "CHORUS", 0.2, 0.4, 0.9), "FX2": REV("HALL", 0.7, 0.25)},
        AM(ATTACK=0.4, DECAY=0.5, SUSTAIN=0.9, RELEASE=0.6, VINTAGE=0.2, **{"VEL AMT": 0.2, "SPREAD": 0.7}),
        [[("SRC.COLOR", 0.2, 0.7), ("SHP.CUTOFF", 0.35, 0.6), ("SRC.TIMBRE", 0.2, 0.4), ("FX 1.MIX", 0.3, 0.55)],
         [("MOD A.FREQ", 0.15, 0.45)],
         [("FX 2.MIX", 0.1, 0.45), ("FX 2.SIZE", 0.5, 0.9)],
         [("SHP.RESO", 0.05, 0.35)]],
        [["A TIDES", "SHP.CUTOFF", 0.3, "M2"], ["A TIDE+", "SRC.COLOR", 0.35, "M2"], ["A TIDE+", "PAN", 0.35, "M2"]],
        LF(("DRIFT", 0.15), ("SINE", 0.1), ("TRI", 0.1)),
        [("PITCH", 0.006), ("PAN", 0.1), ("FX 1.DEPTH", 0.1), None, ("SHP.CUTOFF", 0.03)], voices=6, md=(0.5, 0.15, 0.5, 0.5)))
    L[-1]["move"] = "subtle"

    # 34 ----------------------------------------------------------------------------------------------- STRING ENS
    a(B("STRING ENS", "PAD", {**S5, "MODA": "PEAKS", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "//-_", "TIMBRE": 0.25, "COLOR": 0.4, "DRIFT": 0.15, "LEVEL": 1.0},
         "SHP": FL("LP", 0.5, 0.1, 0.1, 0.0, 0.6, 0.2),
         "MODA": PK(0.35), "FX1": CHO(0.55, "CHORUS", 0.25, 0.5, 0.9), "FX2": REV("HALL", 0.65, 0.2)},
        AM(ATTACK=0.3, DECAY=0.5, SUSTAIN=0.9, RELEASE=0.5, VINTAGE=0.1, **{"VEL AMT": 0.3, "SPREAD": 0.7}),
        [[("SRC.COLOR", 0.3, 0.7), ("SHP.CUTOFF", 0.35, 0.65), ("SRC.TIMBRE", 0.2, 0.45), ("AMP.ATTACK", 0.2, 0.5), ("FX 1.MIX", 0.35, 0.65)],
         [("MOD A.K1", 0.2, 0.6)],
         [("FX 2.MIX", 0.1, 0.45), ("FX 1.WIDTH", 0.5, 0.95)],
         [("SHP.RESO", 0.05, 0.3), ("SRC.DRIFT", 0.1, 0.4)]],
        [["A PEAKS", "SHP.CUTOFF", 0.3, "M2"], ["A PEAKS", "AMP.LEVEL", 0.1, "M2"]],
        LF(("SINE", 0.3), ("DRIFT", 0.15), ("TRI", 0.2)),
        [("FX 1.DEPTH", 0.1), ("PAN", 0.1), ("PITCH", 0.006), None, ("SHP.CUTOFF", 0.03)], voices=6, md=(0.5, 0.15, 0.5, 0.5)))
    L[-1]["move"] = "subtle"

    # 35 ----------------------------------------------------------------------------------------------- BRASS SECT
    a(B("BRASS SECT", "OTHER", {**S5, "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.4, "COLOR": 0.4, "DRIFT": 0.1, "LEVEL": 1.0},
         "SHP": FL("LP", 0.25, 0.15, 0.2, 0.3, 0.5, 0.3, "OWN", 0.1, 0.4, 0.5, 0.3),
         "MODA": TD("AR", 0.4, 0.5, 0.0, 0.5, "MID"), "FX1": CHO(0.2, "CHORUS", 0.3, 0.3, 0.7), "FX2": REV("PLATE", 0.4, 0.15)},
        AM(ATTACK=0.05, DECAY=0.4, SUSTAIN=0.85, RELEASE=0.2, **{"VEL AMT": 0.5, "SPREAD": 0.4}),
        [[("SHP.CUTOFF", 0.25, 0.55), ("SRC.TIMBRE", 0.25, 0.6), ("SHP.F ATK", 0.1, 0.3), ("SHP.DRIVE", 0.1, 0.5)],
         [("LFO.RATE 1", 0.65, 0.8), ("SHP.ENV AMT", 0.3, 0.7)],
         [("FX 1.MIX", 0.1, 0.4), ("FX 2.MIX", 0.05, 0.4)],
         [("SHP.RESO", 0.1, 0.4), ("SRC.COLOR", 0.2, 0.7)]],
        [["LFO 1", "PITCH", 0.02, "M2"], ["LFO 1", "SRC.TIMBRE", 0.2, "M2"]],
        {**LF(("SINE", 0.7), ("DRIFT", 0.2), ("TRI", 0.15)), "FADE 1": 0.5},
        [None, None, ("SHP.CUTOFF", 0.03), ("PAN", 0.1), ("SRC.COLOR", 0.05)], voices=6, md=(0.5, 0.5, 0.5, 0.5)))
    L[-1]["move"] = "medium"

    # 36 ------------------------------------------------------------------------------------------------ CHOIR AAH
    a(B("CHOIR AAH", "PAD", {**S5, "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "VOWL", "TIMBRE": 0.4, "COLOR": 0.5, "DRIFT": 0.15, "LEVEL": 1.0},
         "SHP": FL("LP", 0.6, 0.1, 0.1, 0.0, 0.5, 0.1),
         "MODA": TD("LOOP", 0.15, 0.5), "FX1": CHO(0.4, "CHORUS", 0.2, 0.5, 0.9), "FX2": REV("HALL", 0.8, 0.3)},
        AM(ATTACK=0.4, DECAY=0.5, SUSTAIN=0.9, RELEASE=0.6, VINTAGE=0.1, **{"VEL AMT": 0.3, "SPREAD": 0.7}),
        [[("SRC.COLOR", 0.2, 0.8), ("SRC.TIMBRE", 0.25, 0.6), ("SHP.CUTOFF", 0.45, 0.8), ("AMP.ATTACK", 0.2, 0.5)],
         [("MOD A.FREQ", 0.1, 0.4)],
         [("FX 1.MIX", 0.2, 0.6), ("FX 2.SIZE", 0.5, 0.95)],
         [("SHP.RESO", 0.05, 0.45), ("SHP.DRIVE", 0.1, 0.5)]],
        [["A TIDES", "SRC.COLOR", 0.3, "M2"], ["A TIDE+", "SHP.CUTOFF", 0.3, "M2"], ["A TIDES", "FX 2.MIX", 0.2, "M2"]],
        {**LF(("SINE", 0.7), ("DRIFT", 0.15), ("TRI", 0.1)), "FADE 1": 0.6},
        [("PITCH", 0.006), None, ("SRC.COLOR", 0.05), None, ("PAN", 0.1)], voices=6, md=(0.5, 0.15, 0.5, 0.5)))
    L[-1]["move"] = "subtle"

    # 37 ------------------------------------------------------------------------------------------------ SAW LEAD
    a(B("SAW LEAD", "LEAD", {**S5, "MODA": "TIDES", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.4, "COLOR": 0.4, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": FL("LP", 0.45, 0.2, 0.2, 0.3, 0.6, 0.2, "OWN", 0.0, 0.35, 0.6, 0.3),
         "MODA": TD("LOOP", 0.15, 0.5), "FX1": DLY("1/8D", 0.3, 0.2, "PING"), "FX2": REV("PLATE", 0.5, 0.15)},
        AM(ATTACK=0.0, DECAY=0.3, SUSTAIN=0.9, RELEASE=0.25, MODE="LEGATO", GLIDE=0.1, **{"VEL AMT": 0.5, "SPREAD": 0.4}),
        [[("SRC.TIMBRE", 0.2, 0.6), ("SRC.COLOR", 0.2, 0.6), ("SHP.CUTOFF", 0.25, 0.6), ("SHP.ENV AMT", 0.1, 0.5)],
         [("LFO.RATE 1", 0.65, 0.8)],
         [("FX 1.MIX", 0.05, 0.4), ("FX 2.MIX", 0.05, 0.4)],
         [("SHP.DRIVE", 0.1, 0.6), ("SHP.RESO", 0.1, 0.4)]],
        [["LFO 1", "PITCH", 0.02, "M2"], ["LFO 1", "SRC.TIMBRE", 0.2, "M2"], ["A TIDE+", "SHP.CUTOFF", 0.3, "M2"]],
        {**LF(("SINE", 0.7), ("DRIFT", 0.2), ("TRI", 0.2)), "FADE 1": 0.55},
        [None, None, ("PITCH", 0.006), ("SHP.CUTOFF", 0.03), ("PAN", 0.1)], voices=1, md=(0.5, 0.5, 0.5, 0.5),
        osc={"WAVE": 2, "OCTAVE": 0, "FINE": 0.07, "OSC LVL": 0.5, "MAIN LVL": 0.5}))
    L[-1]["move"] = "medium"

    # 38 ---------------------------------------------------------------------------------------------- HOLLOW LEAD
    a(B("HOLLOW LEAD", "LEAD", {**S5, "MODA": "TIDES", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "-_x3", "TIMBRE": 0.2, "COLOR": 0.3, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": FL("LP", 0.5, 0.15, 0.1, 0.2, 0.6, 0.2, "OWN", 0.0, 0.4, 0.6, 0.3),
         "MODA": TD("LOOP", 0.1, 0.5), "FX1": DLY("1/8D", 0.3, 0.15, "PING"), "FX2": REV("HALL", 0.5, 0.15)},
        AM(ATTACK=0.01, DECAY=0.3, SUSTAIN=0.9, RELEASE=0.3, MODE="LEGATO", GLIDE=0.1, **{"VEL AMT": 0.5, "SPREAD": 0.4}),
        [[("SHP.CUTOFF", 0.35, 0.7), ("SHP.F DEC", 0.25, 0.5), ("SHP.DRIVE", 0.1, 0.4)],
         [("MOD A.FREQ", 0.05, 0.4), ("SHP.ENV AMT", 0.1, 0.5)],
         [("FX 1.MIX", 0.05, 0.35), ("FX 2.MIX", 0.05, 0.4)],
         [("SHP.RESO", 0.1, 0.4), ("SRC.COLOR", 0.2, 0.6)]],
        [["A TIDES", "SHP.CUTOFF", 0.3, "M2"], ["LFO 1", "SRC.TIMBRE", 0.2, "M2"], ["LFO 1", "PITCH", 0.02, "M2"]],
        {**LF(("SINE", 0.7), ("DRIFT", 0.2), ("TRI", 0.2)), "FADE 1": 0.55},
        [None, None, ("PITCH", 0.006), ("SRC.COLOR", 0.05), ("PAN", 0.1)], voices=1, md=(0.5, 0.3, 0.5, 0.5)))
    L[-1]["move"] = "subtle"

    # 39 -------------------------------------------------------------------------------------------------- SUB BASS
    a(B("SUB BASS", "BASS", {**S5, "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "FOLD", "TIMBRE": 0.1, "COLOR": 0.2, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": FL("LP", 0.4, 0.1, 0.1, 0.0, 0.5, 0.2),
         "MODA": TD("LOOP", 0.1, 0.5), "FX1": CHO(0.0, "CHORUS", 0.2, 0.3, 0.6), "FX2": REV("PLATE", 0.3, 0.0)},
        AM(ATTACK=0.0, DECAY=0.3, SUSTAIN=0.95, RELEASE=0.15, MODE="LEGATO", GLIDE=0.1, VINTAGE=0.1, **{"VEL AMT": 0.4, "SPREAD": 0.3}),
        [[("SRC.TIMBRE", 0.1, 0.5), ("SHP.CUTOFF", 0.3, 0.65), ("SRC.COLOR", 0.2, 0.7)],
         [("MOD A.FREQ", 0.05, 0.4)],
         [("FX 2.MIX", 0.0, 0.2), ("FX 1.MIX", 0.0, 0.3)],
         [("SHP.DRIVE", 0.05, 0.6), ("SHP.RESO", 0.1, 0.4)]],
        [["A TIDES", "SRC.TIMBRE", 0.3, "M2"], ["A TIDES", "SHP.CUTOFF", 0.2, "M2"]],
        LF(("DRIFT", 0.15), ("SINE", 0.1), ("TRI", 0.1)),
        [("PITCH", 0.006), None, ("SHP.CUTOFF", 0.03)], voices=1, md=(0.5, 0.3, 0.5, 0.5),
        osc={"WAVE": 0, "OCTAVE": -1, "OSC LVL": 0.5, "MAIN LVL": 0.5}))
    L[-1]["move"] = "subtle"

    # 40 ----------------------------------------------------------------------------------------------- ANALOG BASS
    a(B("ANALOG BASS", "BASS", {**S5, "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.3, "COLOR": 0.3, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": FL("LP24", 0.3, 0.3, 0.3, 0.5, 0.5, 0.3, "OWN", 0.0, 0.3, 0.1, 0.2),
         "MODA": TD("LOOP", 0.1, 0.5), "FX1": CHO(0.1, "CHORUS", 0.2, 0.3, 0.6), "FX2": REV("PLATE", 0.3, 0.1)},
        AM(ATTACK=0.0, DECAY=0.4, SUSTAIN=0.9, RELEASE=0.2, MODE="LEGATO", GLIDE=0.1, VINTAGE=0.1, **{"VEL AMT": 0.5, "SPREAD": 0.3}),
        [[("SHP.F DEC", 0.2, 0.5), ("SHP.CUTOFF", 0.25, 0.5), ("SHP.ENV AMT", 0.3, 0.7), ("SRC.TIMBRE", 0.2, 0.6)],
         [("MOD A.FREQ", 0.05, 0.4)],
         [("FX 2.MIX", 0.0, 0.4), ("FX 1.MIX", 0.0, 0.4)],
         [("SHP.DRIVE", 0.1, 0.6), ("SHP.RESO", 0.2, 0.5)]],
        [["A TIDES", "SHP.CUTOFF", 0.15, "M2"], ["A TIDE+", "SRC.TIMBRE", 0.3, "M2"], ["RANDOM", "SHP.CUTOFF", 0.05, "M2"]],
        LF(("DRIFT", 0.15), ("SINE", 0.1), ("TRI", 0.1)),
        [("PITCH", 0.006), None, ("SHP.CUTOFF", 0.03)], voices=1, md=(0.5, 0.3, 0.5, 0.5),
        osc={"WAVE": 3, "OCTAVE": -1, "OSC LVL": 0.3, "MAIN LVL": 0.9}))
    L[-1]["move"] = "subtle"

    # 41 ------------------------------------------------------------------------------------------------- ACID BASS
    a(B("ACID BASS", "BASS", {**S5, "MODA": "TIDES", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.2, "COLOR": 0.2, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": FL("LP24", 0.25, 0.55, 0.3, 0.6, 0.4, 0.3, "OWN", 0.0, 0.25, 0.05, 0.15),
         "MODA": TD("LOOP", 0.2, 0.5), "FX1": DLY("1/8D", 0.3, 0.15, "PING"), "FX2": REV("PLATE", 0.3, 0.1)},
        AM(ATTACK=0.0, DECAY=0.3, SUSTAIN=0.9, RELEASE=0.15, MODE="LEGATO", GLIDE=0.15, **{"VEL AMT": 0.5, "SPREAD": 0.3}),
        [[("SHP.CUTOFF", 0.15, 0.4), ("SRC.TIMBRE", 0.1, 0.5), ("SHP.ENV AMT", 0.4, 0.8), ("SHP.F DEC", 0.15, 0.4)],
         [("MOD A.FREQ", 0.1, 0.5)],
         [("FX 1.MIX", 0.0, 0.35), ("FX 2.MIX", 0.0, 0.3)],
         [("SHP.RESO", 0.4, 0.7), ("SHP.DRIVE", 0.2, 0.7)]],
        [["A TIDES", "SHP.CUTOFF", 0.2, "M2"], ["AMP ENV", "SHP.RESO", 0.1, "M2"], ["VELO", "SHP.CUTOFF", 0.1]],
        LF(("DRIFT", 0.15), ("SINE", 0.3), ("TRI", 0.1)),
        [("PITCH", 0.006), None, ("SHP.CUTOFF", 0.05)], voices=1, md=(0.5, 0.5, 0.5, 0.5),
        osc={"WAVE": 3, "OCTAVE": -1, "OSC LVL": 0.3, "MAIN LVL": 0.9}))
    L[-1]["move"] = "medium"

    # 42 ----------------------------------------------------------------------------------------------- SYNTH PLUCK
    a(B("SYNTH PLUCK", "KEYS", {**S5, "MODA": "PEAKS", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.35, "COLOR": 0.35, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": FL("LP", 0.35, 0.2, 0.1, 0.5, 0.5, 0.3, "OWN", 0.0, 0.5, 0.05, 0.4),
         "MODA": PK(0.3), "FX1": DLY("1/8D", 0.3, 0.2, "PING"), "FX2": REV("PLATE", 0.4, 0.15)},
        AM(ATTACK=0.0, DECAY=0.6, SUSTAIN=0.0, RELEASE=0.5, **{"VEL AMT": 0.7, "SPREAD": 0.5}),
        [[("SHP.F DEC", 0.4, 0.7), ("SHP.CUTOFF", 0.25, 0.5), ("SHP.ENV AMT", 0.3, 0.7), ("SRC.TIMBRE", 0.2, 0.6)],
         [("MOD A.K1", 0.15, 0.5)],
         [("FX 1.MIX", 0.05, 0.4), ("FX 2.MIX", 0.05, 0.4)],
         [("SHP.RESO", 0.1, 0.45), ("AMP.DECAY", 0.5, 0.75)]],
        [["A PEAKS", "SHP.CUTOFF", 0.3, "M2"], ["RANDOM", "SRC.TIMBRE", 0.3, "M2"], ["A PEAKS", "PAN", 0.4, "M2"]],
        LF(("DRIFT", 0.2), ("SINE", 0.2), ("TRI", 0.15)),
        [None, None, ("SHP.CUTOFF", 0.03), None, ("PAN", 0.1)], voices=5, md=(0.5, 0.3, 0.5, 0.5)))
    L[-1]["move"] = "subtle"

    # 43 -------------------------------------------------------------------------------------------------- FM BELL
    a(B("FM BELL", "KEYS", {**S5, "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "FM", "TIMBRE": 0.4, "COLOR": 0.575, "DRIFT": 0.05, "LEVEL": 1.0, "AD ATK": 0.0, "AD DEC": 0.4, "AD>TIMB": 0.4},
         "SHP": FL("LP", 0.7, 0.1, 0.1, 0.0, 0.7, 0.3),
         "MODA": TD("LOOP", 0.15, 0.5), "FX1": CHO(0.2, "CHORUS", 0.2, 0.3, 0.8), "FX2": REV("HALL", 0.8, 0.3)},
        AM(ATTACK=0.0, DECAY=0.7, SUSTAIN=0.0, RELEASE=0.6, **{"VEL AMT": 0.7, "SPREAD": 0.6}),
        [[("SRC.TIMBRE", 0.15, 0.7), ("SHP.CUTOFF", 0.4, 0.95), ("AMP.DECAY", 0.7, 0.9), ("FX 2.MIX", 0.15, 0.4)],
         [("MOD A.FREQ", 0.1, 0.4)],
         [("FX 2.SIZE", 0.5, 0.95), ("FX 1.MIX", 0.1, 0.4)],
         [("SHP.DRIVE", 0.1, 0.5), ("SHP.RESO", 0.1, 0.4)]],
        [["A TIDES", "SRC.TIMBRE", 0.3, "M2"], ["A TIDE+", "SHP.CUTOFF", 0.3, "M2"], ["A TIDES", "PAN", 0.3, "M2"]],
        LF(("SINE", 0.2), ("DRIFT", 0.2), ("TRI", 0.1)),
        [("PAN", 0.1), ("FX 1.DEPTH", 0.1), ("SHP.CUTOFF", 0.03)], voices=5, md=(0.5, 0.3, 0.5, 0.5)))
    L[-1]["move"] = "subtle"

    # 44 ------------------------------------------------------------------------------------------------- E PIANO
    a(B("E PIANO", "KEYS", {**S5, "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "FM", "TIMBRE": 0.3, "COLOR": 0.425, "DRIFT": 0.05, "LEVEL": 1.0, "AD ATK": 0.0, "AD DEC": 0.3, "AD>TIMB": 0.3},
         "SHP": FL("LP", 0.65, 0.1, 0.1, 0.0, 0.6, 0.3),
         "MODA": TD("LOOP", 0.35, 0.5), "FX1": CHO(0.3, "CHORUS", 0.25, 0.4, 0.8), "FX2": REV("PLATE", 0.5, 0.15)},
        AM(ATTACK=0.0, DECAY=0.6, SUSTAIN=0.2, RELEASE=0.4, **{"VEL AMT": 0.8, "SPREAD": 0.5}),
        [[("SRC.TIMBRE", 0.15, 0.7), ("SHP.CUTOFF", 0.4, 0.95), ("AMP.DECAY", 0.5, 0.75), ("FX 1.MIX", 0.2, 0.5)],
         [("MOD A.FREQ", 0.25, 0.6)],
         [("FX 2.MIX", 0.05, 0.4), ("FX 1.WIDTH", 0.5, 0.95)],
         [("SHP.DRIVE", 0.05, 0.5), ("SRC.DRIFT", 0.05, 0.3)]],
        [["VELO", "SRC.TIMBRE", 0.2], ["A TIDES", "AMP.LEVEL", 0.3, "M2"], ["A TIDE+", "PAN", 0.4, "M2"]],
        LF(("SINE", 0.25), ("DRIFT", 0.2), ("TRI", 0.15)),
        [("PAN", 0.1), None, ("SHP.CUTOFF", 0.03)], voices=6, md=(0.5, 0.15, 0.5, 0.5)))
    L[-1]["move"] = "subtle"

    # 45 ------------------------------------------------------------------------------------------- ROTARY ORGAN
    a(B("ROTARY ORGAN", "KEYS", {**S5, "MODA": "PEAKS", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "HARM", "TIMBRE": 0.45, "COLOR": 0.5, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": FL("LP", 0.7, 0.1, 0.2, 0.0, 0.8, 0.0),
         "MODA": PK(0.4), "FX1": CHO(0.35, "CHORUS", 0.35, 0.3, 0.9), "FX2": REV("HALL", 0.6, 0.15)},
        AM(ATTACK=0.0, DECAY=0.1, SUSTAIN=1.0, RELEASE=0.1, **{"VEL AMT": 0.1, "SPREAD": 0.5}),
        [[("SRC.TIMBRE", 0.2, 0.7), ("SRC.COLOR", 0.2, 0.7), ("SHP.CUTOFF", 0.55, 0.9)],
         [("LFO.RATE 1", 0.3, 0.7), ("FX 1.RATE", 0.25, 0.6)],
         [("FX 2.MIX", 0.05, 0.4), ("FX 1.MIX", 0.2, 0.6)],
         [("SHP.DRIVE", 0.1, 0.6), ("SHP.RESO", 0.05, 0.45)]],
        [["LFO 1", "AMP.LEVEL", 0.2, "M2"], ["LFO 1", "PAN", 0.4, "M2"], ["LFO 1", "SHP.CUTOFF", 0.3, "M2"]],
        LF(("SINE", 0.4), ("DRIFT", 0.2), ("TRI", 0.2)),
        [None, None, ("SRC.TIMBRE", 0.05), ("SHP.CUTOFF", 0.03), ("PAN", 0.1)], voices=6, md=(0.5, 0.5, 0.5, 0.5),
        osc={"WAVE": 0, "OCTAVE": -1, "OSC LVL": 0.3, "MAIN LVL": 0.9}))
    L[-1]["move"] = "medium"

    # 46 ------------------------------------------------------------------------------------------------ HARD SYNC
    a(B("HARD SYNC", "LEAD", {**S5, "MODA": "TIDES", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "SYN/", "TIMBRE": 0.4, "COLOR": 0.4, "DRIFT": 0.05, "LEVEL": 1.0, "AD ATK": 0.0, "AD DEC": 0.3, "AD>TIMB": 0.2},
         "SHP": FL("LP", 0.6, 0.2, 0.2, 0.0, 0.6, 0.2),
         "MODA": TD("LOOP", 0.2, 0.5), "FX1": DLY("1/8D", 0.3, 0.15, "PING"), "FX2": REV("PLATE", 0.5, 0.15)},
        AM(ATTACK=0.0, DECAY=0.3, SUSTAIN=0.9, RELEASE=0.25, MODE="LEGATO", GLIDE=0.1, **{"VEL AMT": 0.5, "SPREAD": 0.4}),
        [[("SRC.TIMBRE", 0.2, 0.7), ("SRC.COLOR", 0.2, 0.6), ("SHP.CUTOFF", 0.4, 0.8), ("SRC.AD>TIMB", 0.1, 0.4)],
         [("MOD A.FREQ", 0.1, 0.5)],
         [("FX 1.MIX", 0.05, 0.4), ("FX 2.MIX", 0.05, 0.4)],
         [("SHP.DRIVE", 0.1, 0.6), ("SHP.RESO", 0.1, 0.4)]],
        [["A TIDES", "SRC.TIMBRE", 0.3, "M2"], ["A TIDE+", "SHP.CUTOFF", 0.3, "M2"], ["LFO 1", "PITCH", 0.02, "M2"]],
        {**LF(("SINE", 0.7), ("DRIFT", 0.2), ("TRI", 0.2)), "FADE 1": 0.55},
        [None, None, ("PITCH", 0.006), ("SHP.CUTOFF", 0.03), ("PAN", 0.1)], voices=1, md=(0.5, 0.5, 0.5, 0.5)))
    L[-1]["move"] = "medium"

    # 47 ------------------------------------------------------------------------------------------------ ATMOSPHERE
    a(B("ATMOSPHERE", "PAD", {**S5, "MODA": "TIDES", "MODB": "FUNC", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "WLIN", "TIMBRE": 0.4, "COLOR": 0.5, "DRIFT": 0.2, "LEVEL": 1.0},
         "SHP": FL("LP", 0.5, 0.2, 0.1, 0.0, 0.5, 0.1),
         "MODA": TD("LOOP", 0.2, 0.5, 0.0, 0.5, "MID"),
         "MODB": FN(RISE_1=0.78, FALL_1=0.78, CYCLE_1="ON", RISE_4=0.72, FALL_4=0.8, CYCLE_4="ON", TRIG_1="OFF", TRIG_4="OFF",
                    LVL_1=0.0, LVL_4=0.0, OUT_1="CH 1", OUT_2="CH 4"),
         "FX1": DLY("1/4", 0.45, 0.3, "TAPE", WOW=0.3), "FX2": REV("SHIMMER", 0.85, 0.35)},
        AM(ATTACK=0.45, DECAY=0.5, SUSTAIN=0.9, RELEASE=0.7, VINTAGE=0.2, **{"VEL AMT": 0.2, "SPREAD": 0.8}),
        [[("SRC.TIMBRE", 0.1, 0.9), ("SRC.COLOR", 0.1, 0.9), ("SHP.CUTOFF", 0.25, 0.8)],
         [("MOD A.FREQ", 0.1, 0.5)],
         [("FX 2.MIX", 0.05, 0.8), ("FX 1.MIX", 0.0, 0.7), ("FX 1.FEEDBACK", 0.2, 0.7)],
         [("SHP.RESO", 0.1, 0.45), ("SRC.DRIFT", 0.1, 0.5)]],
        [["A TIDES", "SRC.TIMBRE", 0.4, "M2"], ["A TIDE+", "SHP.CUTOFF", 0.3, "M2"], ["B OUT 1", "SRC.COLOR", 0.5, "M2"]],
        LF(("SINE", 0.3), ("DRIFT", 0.3), ("TRI", 0.2)),
        [("PAN", 0.2), ("FX 1.WOW", 0.2), ("SRC.TIMBRE", 0.1), ("SHP.CUTOFF", 0.1), ("FX 1.MIX", 0.1)], voices=4, md=(0.5, 0.5, 0.5, 0.5)))
    L[-1]["move"] = "bold"

    # 48 ----------------------------------------------------------------------------------------------- NOISE SWEEP
    a(B("NOISE SWEEP", "FX", {**S5, "MODA": "TIDES", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "NOIS", "TIMBRE": 0.7, "COLOR": 0.5, "DRIFT": 0.0, "LEVEL": 1.0},
         "SHP": FL("LP", 0.2, 0.35, 0.2, 0.0, 0.0, 0.0),
         "MODA": TD("AR", 0.25, 0.5, 0.5, 0.5, "LO"), "FX1": DLY("1/8D", 0.4, 0.2, "PING"), "FX2": REV("HALL", 0.8, 0.3)},
        AM(ATTACK=0.3, DECAY=0.5, SUSTAIN=0.9, RELEASE=0.6, **{"VEL AMT": 0.3, "SPREAD": 0.8}),
        [[("MOD A.FREQ", 0.25, 0.5), ("SHP.CUTOFF", 0.2, 0.4), ("SHP.RESO", 0.15, 0.5), ("SRC.COLOR", 0.2, 0.8)],
         [("LFO.RATE 1", 0.2, 0.6)],
         [("FX 2.MIX", 0.1, 0.5), ("FX 1.MIX", 0.05, 0.4)],
         [("SHP.DRIVE", 0.05, 0.5), ("SRC.TIMBRE", 0.5, 0.9)]],
        [["A TIDES", "SHP.CUTOFF", 0.4], ["A TIDES", "AMP.LEVEL", 0.2], ["A TIDES", "FX 2.MIX", 0.3]],
        LF(("SINE", 0.3), ("DRIFT", 0.3), ("TRI", 0.2)),
        [("SHP.CUTOFF", 0.1), None, ("PAN", 0.2), None, ("SRC.TIMBRE", 0.1)], voices=4, md=(0.5, 0.5, 0.5, 0.5)))
    L[-1]["move"] = "bold"
    for i, q in enumerate(L):
        q["bank"] = 3
        q["amp"]["LEVEL"] = LEVEL.get(q["name"], 0.5)
    return L


if __name__ == "__main__":
    print(len(batch3()))
