# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 update 15: the second batch of factory presets, made for this unit: playable instruments (keys, plucks,
mallets, bells, basses, leads, strings, pads and two rhythmic ones) whose point is the modulation: Tides, Peaks, Grids,
FUNC, the LFOs and the matrix. presets_fm1.py puts them at bank positions 17 to 32, straight after the granular batch.

One grammar for every preset (every number is a design choice, to be judged by ear):
  K1 MORPH    one dial, many areas at once: the source's timbre and colour, the filter or resonator, the speed or shape of a
              modulator, an envelope time and an effect, each with its own range and sign
  K2 MOTION   how much and how fast the modulation moves: a rate, and the depth of modulation routes (their "via" is M2)
  K3 SPACE    the effects: delay, reverb, chorus, spread
  K4 TEXTURE  grit, harmonic density, resonance, stereo spread
A macro is written as (destination, value at 0, value at 1) in the parameter's own units: the matrix amount follows
(value at 1 - value at 0) / span and the base is the value at 0 (presets_granular.G does the same). The matrix has 12 rows in
all (engine/rack.cc NROUTE 18: 12 matrix rows, then two direct targets for each LFO), so a preset's macros and its modulation
routes share them: each preset below spends all twelve. A route whose via is M2 is both a modulation route and K2's depth for
it. Values stay on a 0.05 grid (the preset table holds 256 different floats in all: make_presets.py)."""
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
from presets_granular import _param, _raw, _span, REV, DLY, TI    # the same macro grammar, effect and Tides helpers


def B(name, cat, slots, p, amp, macros, mods=(), lfo=None, lfo_tgt=None, voices=4, md=(0.5, 0.5, 0.5, 0.5), osc=None):
    """One preset. macros: four lists of (destination, value at 0, value at 1); mods: more matrix rows
    [source, destination, amount (, via)]; lfo: the LFOs' own settings; lfo_tgt: up to six direct LFO routes
    (destination, amount): the first two follow LFO 1, the next two LFO 2, the last two LFO 3."""
    import presets_fm1 as F
    p = {k: dict(v) for k, v in p.items()}
    amp = dict(amp)
    lfo = dict(lfo or {})
    rows = []
    for i, ms in enumerate(macros):
        for dest, lo, hi in ms:
            key, lab, d = _param(slots, dest)
            a, b = _raw(d, lo), _raw(d, hi)
            tgt = amp if key == "AMP" else lfo if key == "LFO" else p.setdefault(key, {})
            tgt[lab] = d["opts"][int(a)] if d["t"] == "list" else a
            rows.append([f"M{i + 1}", dest, round((b - a) / _span(d), 4)])
    rows += [list(m) for m in mods]
    if len(rows) > 12:
        raise SystemExit(f"{name}: {len(rows)} matrix rows (12 at most)")
    if len(name) > 12 or name != name.upper() or len(name.split()) > 2:
        raise SystemExit(f"{name}: two words at most, 12 characters or fewer")
    q = F.S.P(name, slots, p, amp, (), rows, None, None, voices)
    q["matrix"] = rows
    q["macros"] = list(md)
    if lfo:
        q["lfo"] = lfo
    tgt = [None] * 6
    for j, t in enumerate(lfo_tgt or []):
        tgt[j] = t
    q["lfo_tgt"] = tgt
    if osc:
        q["osc"] = osc
    q["bank"] = 2
    q["cat"] = cat
    q["fm1_voices"] = voices
    return q


def CHO(mix=0.4, mode="CHORUS", rate=0.3, depth=0.5, width=0.7, fb=0.0):
    return {"MODE": mode, "RATE": rate, "DEPTH": depth, "WIDTH": width, "MIX": mix, "FEEDBACK": fb, "DELAY": 0.0}


def FN(**kw):
    """FUNC's parameters by their page names (RISE and FALL are the knob position: seconds = 0.001 x 20000^p)"""
    return {k.replace("_", " "): v for k, v in kw.items()}


def AM(**kw):
    d = {"ATTACK": 0.02, "DECAY": 0.4, "SUSTAIN": 0.7, "RELEASE": 0.35, "VEL AMT": 0.6, "SPREAD": 0.4, "LEVEL": 0.5}
    d.update(kw)
    return d


def LF(a=("SINE", 0.2), b=("DRIFT", 0.2), c=("TRI", 0.1)):
    """the three LFOs, free-running: (shape, rate)"""
    out = {}
    for n, (sh, r) in enumerate((a, b, c), 1):
        out.update({f"SHAPE {n}": sh, f"RATE {n}": r, f"SYNC {n}": "FREE"})
    return out


def batch2():
    L = []
    a = L.append

    # 17 -------------------------------------------------------------------------------------------------- TINE EP
    # Electric-piano tine: FM with a bark (Braids' own AD on TIMBRE) that dies away, a looping Tides as the tremolo, panned.
    a(B("TINE EP", "KEYS", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "WTBL", "TIMBRE": 0.35, "COLOR": 0.3, "DRIFT": 0.1, "LEVEL": 1.0, "AD ATK": 0.0, "AD DEC": 0.45, "AD>TIMB": 0.3},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.65, "RESO": 0.1, "DRIVE": 0.1, "ENV AMT": 0.0, "KEY TRK": 0.6, "VEL AMT": 0.25},
         "MODA": TI(0.45, "MID", 0.5), "FX1": CHO(0.3, "CHORUS", 0.25, 0.4, 0.8), "FX2": REV("PLATE", 0.5, 0.2)},
        AM(ATTACK=0.0, DECAY=0.6, SUSTAIN=0.15, RELEASE=0.4, **{"VEL AMT": 0.8, "SPREAD": 0.5}),
        [[("SRC.TIMBRE", 0.2, 0.55), ("SRC.COLOR", 0.15, 0.5), ("SHP.CUTOFF", 0.45, 0.85), ("AMP.DECAY", 0.45, 0.75),
          ("MOD A.SHAPE", 0.2, 0.8)],
         [("MOD A.FREQ", 0.3, 0.6)],
         [("FX 2.MIX", 0.1, 0.4), ("FX 1.MIX", 0.1, 0.5)],
         [("SHP.DRIVE", 0.05, 0.9), ("SHP.AUDIO FM", 0.0, 0.6)]],
        [["A TIDES", "AMP.LEVEL", 0.2, "M2"], ["A TIDES", "SHP.CUTOFF", 0.8, "M2"]],
        LF(("SINE", 0.15), ("DRIFT", 0.25), ("SINE", 0.6)),
        [("MOD A.FREQ", 0.15), ("SRC.COLOR", 0.1), ("SHP.CUTOFF", 0.15), ("PAN", 0.3)], voices=6))

    # 18 ---------------------------------------------------------------------------------------------- TONEWHEEL
    # Organ: additive Braids, key click from the AD, a rotating speaker made of a Peaks LFO (pan and brightness) and a
    # chorus. K1 slides registration, rotor speed and drive together.
    a(B("TONEWHEEL", "KEYS", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "PEAKS", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "HARM", "TIMBRE": 0.3, "COLOR": 0.4, "DRIFT": 0.05, "LEVEL": 1.0, "AD ATK": 0.0, "AD DEC": 0.15, "AD>COLR": 0.2},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.7, "RESO": 0.1, "DRIVE": 0.2, "ENV AMT": 0.0, "KEY TRK": 0.8, "VEL AMT": 0.0},
         "MODA": {"FUNC": "LFO", "K1": 0.45, "K2": 0.0, "GATE": "NOTE", "SUSTAIN": 0.0, "INVERT": "OFF"},
         "FX1": CHO(0.4, "CHORUS", 0.4, 0.4, 0.9), "FX2": REV("HALL", 0.6, 0.15)},
        AM(ATTACK=0.0, DECAY=0.1, SUSTAIN=1.0, RELEASE=0.1, **{"VEL AMT": 0.1, "SPREAD": 0.5}),
        [[("SRC.TIMBRE", 0.15, 0.7), ("SRC.COLOR", 0.2, 0.7), ("SHP.CUTOFF", 0.55, 0.9), ("MOD A.K1", 0.3, 0.65), ("FX 1.DEPTH", 0.2, 0.6)],
         [("MOD A.K2", 0.0, 0.6)],
         [("FX 2.MIX", 0.1, 0.45), ("FX 2.SIZE", 0.3, 0.8)],
         [("SHP.DRIVE", 0.05, 0.6), ("SHP.RESO", 0.05, 0.3)]],
        [["A PEAKS", "AMP.LEVEL", 0.05, "M2"], ["A PEAKS", "SHP.CUTOFF", 0.35, "M2"]],
        LF(("DRIFT", 0.3), ("DRIFT", 0.35)),
        [("SHP.CUTOFF", 0.25), ("SRC.TIMBRE", 0.15), ("PAN", 0.3), None, ("MOD A.K1", 0.08)], voices=6))

    # 19 -------------------------------------------------------------------------------------------------- TAPE KEYS
    # Chamberlin-style keys: a FUNC pluck on each note opens the timbre, a slow looping channel is the wow on the tape echo.
    a(B("TAPE KEYS", "KEYS", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "FUNC", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.35, "COLOR": 0.4, "DRIFT": 0.2, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.55, "RESO": 0.1, "DRIVE": 0.2, "ENV AMT": 0.0, "KEY TRK": 0.5, "VEL AMT": 0.2},
         "MODA": FN(RISE_1=0.2, FALL_1=0.65, CURVE_1=0.5, CYCLE_1="OFF", RISE_4=0.75, FALL_4=0.8, CYCLE_4="ON", TRIG_1="NOTE",
                    TRIG_4="OFF", LVL_1=0.0, LVL_4=0.0, OUT_1="CH 1", OUT_2="CH 4"),
         "FX1": DLY("1/8D", 0.35, 0.2, "TAPE", WOW=0.3), "FX2": REV("PLATE", 0.55, 0.2)},
        AM(ATTACK=0.05, DECAY=0.5, SUSTAIN=0.5, RELEASE=0.5, **{"VEL AMT": 0.5, "SPREAD": 0.5}),
        [[("SRC.TIMBRE", 0.1, 0.8), ("SRC.COLOR", 0.1, 0.8), ("SHP.CUTOFF", 0.3, 0.9), ("AMP.ATTACK", 0.0, 0.25)],
         [("MOD A.FALL 4", 0.65, 0.9)],
         [("FX 2.MIX", 0.1, 0.4), ("FX 1.MIX", 0.1, 0.4)],
         [("SHP.DRIVE", 0.1, 0.6), ("SRC.DRIFT", 0.1, 0.4)]],
        [["A OUT 1", "SRC.TIMBRE", 0.35], ["A OUT 2", "FX 1.WOW", 0.6, "M2"], ["A OUT 2", "SHP.CUTOFF", 0.4, "M2"]],
        LF(("SINE", 0.25), ("DRIFT", 0.3)),
        [("SRC.COLOR", 0.1), None, ("SHP.CUTOFF", 0.1)], voices=6))

    # 20 -------------------------------------------------------------------------------------------- SOFT MALLET
    # Mallets and a struck string: Braids' pluck excites Rings' string model (kept near harmonic, so it stays in tune); a Peaks
    # envelope on each strike lifts the brightness.
    a(B("SOFT MALLET", "KEYS", {"SRC": "BRAIDS", "SHP": "RINGS", "MODA": "PEAKS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "PLUK", "TIMBRE": 0.4, "COLOR": 0.3, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": {"MODEL": "STRING", "STRUCT": 0.0, "BRIGHT": 0.5, "DAMPING": 0.7, "POSITION": 0.25, "EXCITE": 0.8, "MIX": 0.95, "SPREAD": 0.5, "FINE": -0.1},
         "MODA": {"FUNC": "ENV", "K1": 0.0, "K2": 0.5, "GATE": "NOTE", "SUSTAIN": 0.0, "INVERT": "OFF"},
         "FX2": REV("PLATE", 0.6, 0.2)},
        AM(ATTACK=0.0, DECAY=0.55, SUSTAIN=0.0, RELEASE=0.45, **{"VEL AMT": 0.8, "SPREAD": 0.5}),
        [[("SRC.TIMBRE", 0.2, 0.7), ("SHP.BRIGHT", 0.45, 0.9), ("SHP.SPREAD", 0.2, 0.9), ("SHP.POSITION", 0.1, 0.5),
          ("MOD A.K2", 0.3, 0.7), ("AMP.DECAY", 0.55, 0.8)],
         [("LFO.RATE 1", 0.2, 0.6)],
         [("FX 2.MIX", 0.1, 0.45), ("FX 2.SIZE", 0.3, 0.8)],
         [("SHP.DAMPING", 0.9, 0.3), ("SHP.EXCITE", 0.5, 1.0)]],
        [["A PEAKS", "SHP.BRIGHT", 0.5, "M2"]],
        LF(("SINE", 0.2), ("DRIFT", 0.2)),
        [("SHP.POSITION", 0.1), ("SHP.BRIGHT", 0.1)], voices=4))

    # 21 ------------------------------------------------------------------------------------------------ GLASS BELLS
    # Struck bells: Braids' bell model, a Tides AD slope on every note sweeps the colour, an echo and a hall.
    a(B("GLASS BELLS", "KEYS", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "TIDES", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "WTBL", "TIMBRE": 0.6, "COLOR": 0.5, "DRIFT": 0.05, "LEVEL": 1.0, "AD DEC": 0.3, "AD>TIMB": 0.3},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.65, "RESO": 0.15, "DRIVE": 0.1, "ENV AMT": 0.0, "KEY TRK": 0.7, "VEL AMT": 0.25},
         "MODA": {"MODE": "AD", "FREQ": 0.5, "SHAPE": 0.5, "SLOPE": -0.3, "SMOOTH": 0.3, "RANGE": "MID", "GATE": "NOTE", "INVERT": "OFF"},
         "FX1": DLY("1/8D", 0.35, 0.2, "PING"), "FX2": REV("HALL", 0.8, 0.3)},
        AM(ATTACK=0.0, DECAY=0.7, SUSTAIN=0.0, RELEASE=0.6, **{"VEL AMT": 0.7, "SPREAD": 0.6}),
        [[("SRC.TIMBRE", 0.2, 0.9), ("SRC.COLOR", 0.1, 0.9), ("SHP.CUTOFF", 0.4, 0.9), ("MOD A.FREQ", 0.3, 0.7), ("AMP.DECAY", 0.65, 0.9)],
         [("LFO.RATE 1", 0.2, 0.6)],
         [("FX 2.MIX", 0.15, 0.5), ("FX 1.FEEDBACK", 0.15, 0.55)],
         [("SHP.AUDIO FM", 0.0, 0.7), ("SHP.RESO", 0.1, 0.7)]],
        [["A TIDES", "SRC.COLOR", 0.6, "M2"], ["A TIDE+", "SRC.TIMBRE", 0.5, "M2"]],
        LF(("SINE", 0.3), ("DRIFT", 0.3)),
        [("SHP.CUTOFF", 0.3), ("SRC.COLOR", 0.1), ("SRC.TIMBRE", 0.15), None, ("FX 1.MIX", 0.1)], voices=5))

    # 22 ------------------------------------------------------------------------------------------- SYMPATH HARP
    # A harp whose sympathetic strings (Rings) are brightened in time by Grids: each step of the pattern lifts them.
    a(B("SYMPATH HARP", "KEYS", {"SRC": "BRAIDS", "SHP": "RINGS", "MODA": "GRIDS", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "PLUK", "TIMBRE": 0.3, "COLOR": 0.3, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": {"MODEL": "SYMPATH", "STRUCT": 0.0, "BRIGHT": 0.6, "DAMPING": 0.55, "POSITION": 0.3, "EXCITE": 0.8, "MIX": 0.95, "SPREAD": 0.6, "FINE": -0.1},
         "MODA": {"X": 0.4, "Y": 0.6, "FILL": 0.6, "OUT": "KIT", "CHAOS": 0.1, "DECAY": 0.3},
         "FX1": DLY("1/8D", 0.3, 0.15, "PING"), "FX2": REV("HALL", 0.7, 0.25)},
        AM(ATTACK=0.0, DECAY=0.6, SUSTAIN=0.0, RELEASE=0.55, **{"VEL AMT": 0.8, "SPREAD": 0.6}),
        [[("SRC.COLOR", 0.1, 0.7), ("SHP.BRIGHT", 0.5, 0.85), ("SHP.DAMPING", 0.7, 0.4), ("SRC.TIMBRE", 0.2, 0.6), ("MOD A.DECAY", -0.3, 0.5)],
         [("MOD A.FILL", 0.2, 1.0)],
         [("FX 1.MIX", 0.0, 0.5), ("FX 2.MIX", 0.0, 0.7)],
         [("SHP.EXCITE", 0.5, 1.0), ("SHP.SPREAD", 0.1, 1.0)]],
        [["A GRID", "SHP.POSITION", 0.5, "M2"], ["A ACCENT", "SRC.TIMBRE", 0.4, "M2"]],
        LF(("SINE", 0.2), ("DRIFT", 0.2)),
        [("SHP.POSITION", 0.1), ("SHP.BRIGHT", 0.1)], voices=4))

    # 23 -------------------------------------------------------------------------------------------------- TIDAL BASS
    # A mono bass whose cutoff and timbre are swung by a looping Tides; an LFO nudges the Tides' own speed (modulation of
    # modulation), and K1 changes that speed and the slope's shape as well as the colour.
    a(B("TIDAL BASS", "BASS", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.4, "COLOR": 0.4, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP24", "CUTOFF": 0.4, "RESO": 0.35, "DRIVE": 0.35, "ENV AMT": 0.0, "KEY TRK": 0.5, "VEL AMT": 0.3},
         "MODA": TI(0.4, "MID", 0.5), "FX1": CHO(0.2, "CHORUS", 0.2, 0.3, 0.6), "FX2": REV("PLATE", 0.4, 0.1)},
        AM(ATTACK=0.0, DECAY=0.4, SUSTAIN=0.9, RELEASE=0.2, MODE="LEGATO", GLIDE=0.1, **{"VEL AMT": 0.5, "SPREAD": 0.3}),
        [[("SRC.TIMBRE", 0.2, 0.7), ("SHP.CUTOFF", 0.25, 0.55), ("SHP.RESO", 0.2, 0.55), ("MOD A.FREQ", 0.2, 0.6), ("MOD A.SHAPE", 0.2, 0.8)],
         [("LFO.RATE 1", 0.3, 0.6)],
         [("FX 2.MIX", 0.0, 0.6), ("FX 1.MIX", 0.1, 0.7)],
         [("SHP.DRIVE", 0.1, 0.9), ("SHP.AUDIO FM", 0.0, 0.4)]],
        [["A TIDES", "SHP.CUTOFF", 0.45, "M2"], ["A TIDE+", "SRC.TIMBRE", 0.25, "M2"]],
        LF(("SINE", 0.3), ("DRIFT", 0.3)),
        [("MOD A.FREQ", 0.15), None, ("SHP.CUTOFF", 0.1)], voices=1, osc={"WAVE": 0, "OCTAVE": -1, "OSC LVL": 0.3, "MAIN LVL": 0.9}))

    # 24 --------------------------------------------------------------------------------------------------- FOLD BASS
    # A mono bass with a wavefolder: a FUNC pluck on every note folds the timbre, a second FUNC channel on the eighths
    # pumps the cutoff.
    a(B("FOLD BASS", "BASS", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "FUNC", "FX2": "REVERB"},
        {"SRC": {"MODEL": "FOLD", "TIMBRE": 0.3, "COLOR": 0.3, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP24", "CUTOFF": 0.35, "RESO": 0.25, "DRIVE": 0.3, "ENV AMT": 0.0, "KEY TRK": 0.5, "VEL AMT": 0.3},
         "MODA": FN(RISE_1=0.15, FALL_1=0.55, CURVE_1=0.6, CYCLE_1="OFF", RISE_4=0.25, FALL_4=0.5, CURVE_4=0.3, CYCLE_4="OFF",
                    TRIG_1="NOTE", TRIG_4="1/8", LVL_1=0.0, LVL_4=0.0, OUT_1="CH 1", OUT_2="CH 4"),
         "FX2": REV("PLATE", 0.4, 0.1)},
        AM(ATTACK=0.0, DECAY=0.4, SUSTAIN=0.9, RELEASE=0.2, MODE="LEGATO", GLIDE=0.1, **{"VEL AMT": 0.5, "SPREAD": 0.3}),
        [[("SRC.TIMBRE", 0.25, 0.65), ("SHP.CUTOFF", 0.25, 0.5), ("MOD A.FALL 1", 0.4, 0.7), ("SRC.COLOR", 0.2, 0.7), ("AMP.RELEASE", 0.1, 0.3)],
         [("MOD A.FALL 4", 0.3, 0.8)],
         [("FX 2.MIX", 0.0, 0.6), ("FX 2.SIZE", 0.3, 0.9)],
         [("SHP.DRIVE", 0.1, 0.9), ("SHP.RESO", 0.1, 0.7)]],
        [["A OUT 1", "SRC.TIMBRE", 0.35], ["A OUT 2", "SHP.CUTOFF", 0.9, "M2"]],
        LF(("SINE", 0.3), ("DRIFT", 0.3)),
        [("SRC.COLOR", 0.1), ("SHP.CUTOFF", 0.2), ("SHP.CUTOFF", 0.1)], voices=1, osc={"WAVE": 0, "OCTAVE": -1, "OSC LVL": 0.3, "MAIN LVL": 0.9}))

    # 25 --------------------------------------------------------------------------------------------------- SYNC LEAD
    # A mono hard-sync lead: FUNC channel 1 sweeps the sync on every note; channel 4 fades the vibrato in, and K2 sets how
    # much (its level in the FUNC's mix).
    a(B("SYNC LEAD", "LEAD", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "FUNC", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "SYN-", "TIMBRE": 0.4, "COLOR": 0.4, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.7, "RESO": 0.2, "DRIVE": 0.2, "ENV AMT": 0.0, "KEY TRK": 0.6, "VEL AMT": 0.2},
         "MODA": FN(RISE_1=0.2, FALL_1=0.6, CURVE_1=0.5, CYCLE_1="OFF", RISE_4=0.75, FALL_4=0.9, CYCLE_4="OFF", TRIG_1="NOTE",
                    TRIG_4="NOTE", LVL_1=0.0, LVL_4=1.0, OUT_1="CH 1", OUT_2="SUM"),
         "FX1": DLY("1/8D", 0.3, 0.15, "PING"), "FX2": REV("PLATE", 0.5, 0.15)},
        AM(ATTACK=0.0, DECAY=0.4, SUSTAIN=0.9, RELEASE=0.25, MODE="LEGATO", GLIDE=0.1, **{"VEL AMT": 0.5, "SPREAD": 0.4}),
        [[("SRC.TIMBRE", 0.15, 0.6), ("SRC.COLOR", 0.2, 0.7), ("SHP.CUTOFF", 0.4, 0.8), ("MOD A.FALL 1", 0.4, 0.7)],
         [("MOD A.LVL 4", 0.0, 1.0), ("LFO.RATE 1", 0.55, 0.8)],
         [("FX 1.MIX", 0.0, 0.5), ("FX 2.MIX", 0.0, 0.6)],
         [("SHP.DRIVE", 0.1, 0.9), ("SHP.RESO", 0.1, 0.7)]],
        [["A OUT 1", "SRC.TIMBRE", 0.6, "M2"], ["LFO 1", "PITCH", 0.014, "A OUT 2"]],
        LF(("SINE", 0.7), ("DRIFT", 0.3)),
        [None, None, ("SHP.CUTOFF", 0.12), ("SRC.COLOR", 0.1)], voices=1))

    # 26 ----------------------------------------------------------------------------------------------- FORMANT LEAD
    # A mono vocal lead: Braids' vowel model, a slow Tides wandering the vowel, K1 sliding through the vowels.
    a(B("FORMANT LEAD", "LEAD", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "TIDES", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "VOWL", "TIMBRE": 0.4, "COLOR": 0.4, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.7, "RESO": 0.15, "DRIVE": 0.1, "ENV AMT": 0.0, "KEY TRK": 0.6, "VEL AMT": 0.2},
         "MODA": TI(0.2, "MID", 0.5), "FX1": DLY("1/8D", 0.3, 0.15, "PING"), "FX2": REV("HALL", 0.6, 0.2)},
        AM(ATTACK=0.03, DECAY=0.4, SUSTAIN=0.9, RELEASE=0.3, MODE="LEGATO", GLIDE=0.1, **{"VEL AMT": 0.5, "SPREAD": 0.4}),
        [[("SRC.COLOR", 0.1, 0.9), ("SRC.TIMBRE", 0.2, 0.7), ("SHP.CUTOFF", 0.4, 0.85), ("AMP.ATTACK", 0.03, 0.25), ("FX 1.MIX", 0.05, 0.3)],
         [("MOD A.FREQ", 0.2, 0.8)],
         [("FX 2.MIX", 0.0, 0.6), ("FX 1.FEEDBACK", 0.15, 0.6)],
         [("SHP.DRIVE", 0.1, 0.9), ("SHP.RESO", 0.1, 0.7)]],
        [["A TIDES", "SRC.COLOR", 0.8, "M2"], ["LFO 1", "PITCH", 0.012, "M2"]],
        LF(("SINE", 0.7), ("DRIFT", 0.3)),
        [None, None, ("SRC.TIMBRE", 0.1), ("SHP.CUTOFF", 0.1)], voices=1))

    # 27 ------------------------------------------------------------------------------------------- SLOW STRINGS
    # Strings that bloom on every chord: a Peaks envelope opens the filter; a slow Tides moves the timbre, and the Peaks
    # envelope also nudges that Tides' speed (one modulator changing another's rate).
    a(B("SLOW STRINGS", "PAD", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "PEAKS", "MODB": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.4, "COLOR": 0.4, "DRIFT": 0.15, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.4, "RESO": 0.15, "DRIVE": 0.1, "ENV AMT": 0.0, "KEY TRK": 0.5, "VEL AMT": 0.1},
         "MODA": {"FUNC": "ENV", "K1": 0.6, "K2": 0.6, "GATE": "NOTE", "SUSTAIN": 1.0, "INVERT": "OFF"},
         "MODB": TI(0.25, "MID", 0.5), "FX1": CHO(0.5, "CHORUS", 0.25, 0.5, 0.9), "FX2": REV("HALL", 0.8, 0.3)},
        AM(ATTACK=0.4, DECAY=0.5, SUSTAIN=0.9, RELEASE=0.6, **{"VEL AMT": 0.3, "SPREAD": 0.7}),
        [[("SRC.COLOR", 0.15, 0.7), ("SHP.CUTOFF", 0.3, 0.6), ("AMP.ATTACK", 0.1, 0.6), ("MOD A.K1", 0.3, 0.8)],
         [("MOD B.FREQ", 0.15, 0.5)],
         [("FX 2.MIX", 0.1, 0.5), ("FX 1.MIX", 0.2, 0.7)],
         [("SHP.RESO", 0.05, 0.4), ("SHP.DRIVE", 0.05, 0.5)]],
        [["A PEAKS", "SHP.CUTOFF", 0.6, "M2"], ["B TIDES", "SRC.TIMBRE", 0.6, "M2"], ["A PEAKS", "MOD B.FREQ", 0.15]],
        LF(("SINE", 0.25), ("DRIFT", 0.3)),
        [("SRC.COLOR", 0.1), None, ("SHP.CUTOFF", 0.1)], voices=6))

    # 28 ------------------------------------------------------------------------------------------------ VOWEL PAD
    # A choir-like pad: two slow looping FUNC channels at unrelated speeds wander colour and timbre; channel 1's output
    # also speeds channel 4 up and down.
    a(B("VOWEL PAD", "PAD", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "FUNC", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "VFOF", "TIMBRE": 0.4, "COLOR": 0.4, "DRIFT": 0.15, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.6, "RESO": 0.1, "DRIVE": 0.1, "ENV AMT": 0.0, "KEY TRK": 0.5, "VEL AMT": 0.1},
         "MODA": FN(RISE_1=0.78, FALL_1=0.78, CYCLE_1="ON", RISE_4=0.72, FALL_4=0.78, CYCLE_4="ON", TRIG_1="OFF", TRIG_4="OFF",
                    LVL_1=0.0, LVL_4=0.0, OUT_1="CH 1", OUT_2="CH 4"),
         "FX1": CHO(0.5, "CHORUS", 0.2, 0.5, 0.9), "FX2": REV("HALL", 0.85, 0.3)},
        AM(ATTACK=0.45, DECAY=0.5, SUSTAIN=0.9, RELEASE=0.65, **{"VEL AMT": 0.3, "SPREAD": 0.7}),
        [[("SRC.COLOR", 0.1, 0.9), ("SRC.TIMBRE", 0.1, 0.8), ("SHP.CUTOFF", 0.1, 0.8), ("AMP.ATTACK", 0.15, 0.7)],
         [("MOD A.RISE 4", 0.5, 0.95)],
         [("FX 2.MIX", 0.1, 0.5), ("FX 2.SIZE", 0.4, 0.9)],
         [("SHP.DRIVE", 0.05, 0.5), ("SHP.RESO", 0.05, 0.4)]],
        [["A OUT 1", "SRC.COLOR", 0.6, "M2"], ["A OUT 2", "SHP.CUTOFF", 0.7, "M2"], ["A OUT 1", "MOD A.FALL 4", 0.1]],
        LF(("SINE", 0.25), ("DRIFT", 0.3)),
        [("FX 1.DEPTH", 0.1), None, ("SRC.TIMBRE", 0.1)], voices=6))

    # 29 --------------------------------------------------------------------------------------------------- TIDE PAD
    # A breathing pad: a Tides AR slope rises while the keys are held and falls on release, opening the filter and the
    # reverb's shimmer; the chorus behind it blurs the saw's edge. (2.48: this was a light Clouds; the shimmer is the reverb's now.)
    a(B("TIDE PAD", "PAD", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "TIDES", "FX1": "REVERB", "FX2": "CHORUS"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.35, "COLOR": 0.4, "DRIFT": 0.15, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.45, "RESO": 0.1, "DRIVE": 0.1, "ENV AMT": 0.0, "KEY TRK": 0.5, "VEL AMT": 0.1},
         "MODA": {"MODE": "AR", "FREQ": 0.25, "SHAPE": 0.5, "SLOPE": 0.0, "SMOOTH": 0.3, "RANGE": "MID", "GATE": "NOTE", "INVERT": "OFF"},
         "FX1": REV("HALL", 0.8, 0.3, SHIMMER=0.3),
         "FX2": {"RATE": 0.25, "DEPTH": 0.4, "WIDTH": 0.8, "MIX": 0.3, "MODE": "CHORUS", "FEEDBACK": 0.0, "DELAY": 0.0}},
        AM(ATTACK=0.35, DECAY=0.5, SUSTAIN=0.9, RELEASE=0.6, **{"VEL AMT": 0.3, "SPREAD": 0.7}),
        [[("SRC.COLOR", 0.15, 0.7), ("SHP.CUTOFF", 0.3, 0.6), ("MOD A.FREQ", 0.1, 0.4), ("FX 1.SHIMMER", 0.2, 0.5)],
         [("MOD A.SHAPE", 0.2, 0.8)],
         [("FX 1.MIX", 0.0, 0.7), ("FX 1.SIZE", 0.3, 0.95)],
         [("SHP.RESO", 0.05, 0.7), ("SHP.DRIVE", 0.05, 0.9)]],
        [["A TIDE+", "SHP.CUTOFF", 0.35, "M2"], ["A TIDE+", "FX 1.SHIMMER", 0.3, "M2"], ["A TIDES", "SRC.COLOR", 0.2, "M2"]],
        LF(("SINE", 0.3), ("DRIFT", 0.35), ("SINE", 0.4)),
        [("SHP.CUTOFF", 0.15), ("SRC.COLOR", 0.1), ("FX 2.DEPTH", 0.1), None, ("SRC.TIMBRE", 0.1)], voices=4))

    # 30 --------------------------------------------------------------------------------------------------- GRID CHOP
    # Held chords chopped by a Grids pattern: each hit opens the filter and the timbre. A Peaks LFO on the second modulator
    # drifts the pattern's X, so the rhythm slowly rearranges itself.
    a(B("GRID CHOP", "FX", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "GRIDS", "MODB": "PEAKS", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.35, "COLOR": 0.4, "DRIFT": 0.1, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP24", "CUTOFF": 0.2, "RESO": 0.3, "DRIVE": 0.3, "ENV AMT": 0.0, "KEY TRK": 0.6, "VEL AMT": 0.1},
         "MODA": {"X": 0.4, "Y": 0.6, "FILL": 0.55, "OUT": "KIT", "CHAOS": 0.1, "DECAY": 0.0},
         "MODB": {"FUNC": "LFO", "K1": 0.2, "K2": 0.0, "GATE": "NOTE", "SUSTAIN": 0.0, "INVERT": "OFF"},
         "FX1": DLY("1/8D", 0.35, 0.25, "PING"), "FX2": REV("PLATE", 0.5, 0.2)},
        AM(ATTACK=0.0, DECAY=0.4, SUSTAIN=1.0, RELEASE=0.3, **{"VEL AMT": 0.3, "SPREAD": 0.6}),
        [[("MOD A.FILL", 0.3, 0.9), ("SHP.CUTOFF", 0.15, 0.4), ("MOD A.Y", 0.2, 0.9), ("SRC.COLOR", 0.2, 0.7)],
         [("MOD B.K1", 0.1, 0.5)],
         [("FX 1.MIX", 0.1, 0.45), ("FX 2.MIX", 0.1, 0.4)],
         [("SHP.DRIVE", 0.15, 0.7), ("SHP.RESO", 0.2, 0.55)]],
        [["A GRID", "SHP.CUTOFF", 0.45, "M2"], ["A ACCENT", "SRC.TIMBRE", 0.3, "M2"], ["B PEAKS", "MOD A.X", 0.3]],
        LF(("SINE", 0.15), ("DRIFT", 0.2)),
        [("SRC.COLOR", 0.05), None, ("SHP.CUTOFF", 0.03)], voices=5))

    # 31 -------------------------------------------------------------------------------------------------- FUNC PULSE
    # A pulse instrument made of two FUNC channels: channel 1 fires on the eighths, its end of cycle triggers channel 4, and
    # the two shapes ping the filter and the timbre on held chords. A Grids accent on the second modulator adds a rest.
    a(B("FUNC PULSE", "FX", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "FUNC", "MODB": "GRIDS", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.3, "COLOR": 0.5, "DRIFT": 0.1, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP24", "CUTOFF": 0.25, "RESO": 0.3, "DRIVE": 0.3, "ENV AMT": 0.0, "KEY TRK": 0.6, "VEL AMT": 0.1},
         "MODA": FN(RISE_1=0.1, FALL_1=0.5, CURVE_1=0.5, CYCLE_1="OFF", RISE_4=0.2, FALL_4=0.5, CURVE_4=0.4, CYCLE_4="OFF",
                    TRIG_1="1/8", TRIG_4="OTHER EOC", LVL_1=0.0, LVL_4=0.0, OUT_1="CH 1", OUT_2="CH 4"),
         "MODB": {"X": 0.5, "Y": 0.4, "FILL": 0.5, "OUT": "SD", "CHAOS": 0.0, "DECAY": 0.0},
         "FX1": DLY("1/8D", 0.35, 0.25, "PING"), "FX2": REV("HALL", 0.7, 0.2)},
        AM(ATTACK=0.0, DECAY=0.4, SUSTAIN=1.0, RELEASE=0.3, **{"VEL AMT": 0.3, "SPREAD": 0.6}),
        [[("MOD A.FALL 1", 0.35, 0.7), ("SHP.CUTOFF", 0.15, 0.4), ("SRC.COLOR", 0.2, 0.7), ("MOD A.FALL 4", 0.3, 0.7)],
         [("MOD B.FILL", 0.2, 0.9)],
         [("FX 1.MIX", 0.1, 0.45), ("FX 2.MIX", 0.1, 0.4)],
         [("SHP.DRIVE", 0.15, 0.7), ("SHP.RESO", 0.2, 0.55)]],
        [["A OUT 1", "SHP.CUTOFF", 0.45, "M2"], ["A OUT 2", "SRC.TIMBRE", 0.4, "M2"], ["B ACCENT", "MOD A.FALL 1", 0.2]],
        LF(("SINE", 0.15), ("DRIFT", 0.2)),
        [("SRC.COLOR", 0.05), None, ("SHP.CUTOFF", 0.03)], voices=5))

    # 32 ------------------------------------------------------------------------------------------ MODEL DIAL
    # My own idea: one dial that changes the instrument itself. K1 steps Braids through four of its models in turn
    # (choir, vowel, formant-wave and harmonic organ), all of which keep the played pitch, while the timbre, filter and
    # echo move with it; a Tides AR swell and an LFO keep each one breathing.
    a(B("MODEL DIAL", "FX", {"SRC": "BRAIDS", "SHP": "FILTER", "MODA": "TIDES", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"MODEL": "VOSM", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.05, "LEVEL": 1.0},
         "SHP": {"TYPE": "LP", "CUTOFF": 0.7, "RESO": 0.1, "DRIVE": 0.1, "ENV AMT": 0.0, "KEY TRK": 0.6, "VEL AMT": 0.2},
         "MODA": {"MODE": "LOOP", "FREQ": 0.3, "SHAPE": 0.5, "SLOPE": 0.0, "SMOOTH": 0.3, "RANGE": "MID", "GATE": "NOTE", "INVERT": "OFF"},
         "FX1": DLY("1/8D", 0.3, 0.2, "PING"), "FX2": REV("HALL", 0.7, 0.25)},
        AM(ATTACK=0.1, DECAY=0.5, SUSTAIN=0.8, RELEASE=0.4, **{"VEL AMT": 0.5, "SPREAD": 0.5}),
        [[("SRC.MODEL", 21, 24), ("SRC.TIMBRE", 0.3, 0.7), ("SHP.CUTOFF", 0.5, 0.85), ("FX 1.MIX", 0.05, 0.3), ("AMP.ATTACK", 0.0, 0.3)],
         [("MOD A.FREQ", 0.2, 0.5)],
         [("FX 2.MIX", 0.0, 0.7), ("FX 1.FEEDBACK", 0.15, 0.6)],
         [("SHP.DRIVE", 0.05, 0.8), ("SRC.COLOR", 0.2, 0.8)]],
        [["A TIDE+", "SRC.TIMBRE", 0.7, "M2"], ["A TIDES", "SHP.CUTOFF", 0.7, "M2"]],
        LF(("SINE", 0.5), ("DRIFT", 0.3)),
        [("SRC.COLOR", 0.25), None, ("SHP.CUTOFF", 0.3)], voices=4))
    return L


if __name__ == "__main__":
    print(len(batch2()))
