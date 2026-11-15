# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 update 11: the first batch of granular presets, made for this unit (Braids into Clouds, the four macros,
LFOs, Tides and Peaks). The page has none of them: presets_fm1.py puts them first in the bank (after INIT).

One grammar for every preset (all of it a design choice, to be judged by ear):
  K1 TONE     the colour of the source, and Clouds' own TONE on the wet
  K2 MOTION   how much and how fast it moves: LFO rate, and the depth of LFO / Tides routes (the route's "via" is M2)
  K3 SPACE    the wash: Clouds BLEND and SPREAD, FEEDBACK where it is safe, the reverb or delay
  K4 TEXTURE  the grain's character: density, size, pitch scatter, spray, freeze-like states, glitch
A macro is written as (destination, value at 0, value at 1) in the parameter's own units (an option's name for a list):
the matrix amount follows (value at 1 - value at 0) / span, and the base is the value at 0. Each macro starts at "md".
DENSITY is never parked in Clouds' silent band (0.47 to 0.53) unless grains are fired by a trigger (GRN SYNC).
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
            if p["l"] == lab:
                return ("AMP" if sl == "AMP" else "LFO" if sl == "LFO" else SLOTN[sl]), lab, p
    raise SystemExit(f"presets_granular: no parameter {dest}")


def _raw(p, v):
    if p["t"] == "list":
        return float(p["opts"].index(v)) if isinstance(v, str) else float(v)
    return float(v)


def _span(p):
    t = p["t"]
    return float(len(p["opts"]) - 1) if t == "list" else 2.0 if t == "bip" else float(p["max"] - p["min"]) if t == "int" else 1.0


BASE_TGT = [("FX 1.TONE", 0.3), ("FX 1.BLEND", 0.1), ("FX 1.POSITION", 0.08), ("FX 1.SIZE", 0.08), ("SRC.TIMBRE", 0.12), None]


def G(name, slots, p, amp, macros, mods=(), lfo=None, lfo_tgt=None, voices=4, md=(0.5, 0.5, 0.5, 0.5)):
    """One preset: slots (module per slot), p (the designed values of every other parameter), amp, macros (four lists
    of (destination, value at 0, value at 1)), mods (more matrix rows: [source, destination, amount (, via)] with the
    amount in the matrix's own terms), lfo (the LFOs' own settings), lfo_tgt (up to six direct LFO routes)"""
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
            amt = round((b - a) / _span(d), 4)
            rows.append([f"M{i + 1}", dest, amt])
    rows += [list(m) for m in mods]
    if len(rows) > 12:
        raise SystemExit(f"{name}: {len(rows)} matrix rows (12 at most)")
    if len(name) > 12 or name != name.upper():
        raise SystemExit(f"{name}: a name is 12 characters or fewer")
    q = F.S.P(name, slots, p, amp, (), rows, None, None, voices)
    q["matrix"] = rows
    q["macros"] = list(md)
    if lfo:
        q["lfo"] = lfo
    tgt = list(BASE_TGT)                    # every preset moves a little on its own (LFO 1 colour and wash, LFO 2 the read point, LFO 3 the source)
    for j, t in enumerate(lfo_tgt or []):
        if t is not None:
            tgt[j] = t
    q["lfo_tgt"] = tgt
    q["bank"] = 1
    q["cat"] = "FX"
    q["fm1_voices"] = voices
    return q


# ----------------------------------------------------------------------------------------------------------------------
def REV(typ="HALL", size=0.8, mix=0.3, damp=0.5, **kw):
    return {"TYPE": typ, "SIZE": size, "DAMP": damp, "DIFFUSE": 0.75, "MIX": mix, "PRE": 0.1, "LO CUT": 0.25, **kw}


def CL(**kw):
    """Clouds' designed values (GRANULR, 16 ST); anything a macro owns is overwritten by its value at 0"""
    d = {"POSITION": 0.3, "SIZE": 0.6, "PITCH": 0.0, "DENSITY": 0.7, "TEXTURE": 0.7, "BLEND": 0.6, "SPREAD": 0.7,
         "FEEDBACK": 0.1, "REVERB": 0.0, "MODE": "GRANULR", "FREEZE": "OFF", "QUALITY": "16 ST", "IN GAIN": 0.0,
         "NOTE TRG": "OFF", "PITCH TRK": "OFF", "GRN SYNC": "OFF", "SPRAY": 0.0, "PITCH RND": 0.0, "PITCH QNT": "FREE",
         "SIZE RND": 0.0, "SCAN": 0.0, "TONE": 0.0}
    d.update(kw)
    return d


PADAMP = {"ATTACK": 0.3, "DECAY": 0.5, "SUSTAIN": 0.9, "RELEASE": 0.6, "VEL AMT": 0.3, "SPREAD": 0.6, "LEVEL": 0.5}
SLOWLFO = {"SHAPE 1": "SINE", "RATE 1": 0.2, "SYNC 1": "FREE", "SHAPE 2": "DRIFT", "RATE 2": 0.2, "SYNC 2": "FREE",
           "SHAPE 3": "TRI", "RATE 3": 0.1, "SYNC 3": "FREE"}


def TI(f=0.25, rng="MID", shape=0.5):
    return {"MODE": "LOOP", "FREQ": f, "SHAPE": shape, "SLOPE": 0.0, "SMOOTH": 0.5, "RANGE": rng}


def DLY(t="1/8D", fb=0.3, mix=0.2, mode="PING", **kw):
    return {"TIME": t, "FEEDBACK": fb, "TONE": 0.5, "MIX": mix, "MODE": mode, "LO CUT": 0.3, **kw}


def granular():
    L = []
    a = L.append

    # 1 -------------------------------------------------------------------------------------------- the frozen pad
    # Classic: an "infinite" pad made by freezing a chord in Clouds' buffer and granulating it. K4 slides from a live
    # cloud to a frozen one (FREEZE engages above the middle): play the chord, push K4 up, let go of the keys.
    a(G("FROZEN PAD", {"SRC": "BRAIDS", "MODA": "TIDES", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "ZPKF", "TIMBRE": 0.4, "COLOR": 0.55, "DRIFT": 0.15, "LEVEL": 1.0},
         "MODA": TI(0.25, "MID"),
         "FX1": CL(POSITION=0.4, SIZE=0.75, DENSITY=0.7, TEXTURE=0.8, BLEND=0.6, SPREAD=0.8, FEEDBACK=0.15, **{"IN GAIN": 0.4}),
         "FX2": REV("HALL", 0.85, 0.25)},
        {**PADAMP, "ATTACK": 0.4},
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.45, 0.1)],
         [("LFO.RATE 1", 0.1, 0.7), ("MOD A.FREQ", 0.1, 0.6)],
         [("FX 1.BLEND", 0.2, 0.95), ("FX 2.MIX", 0.05, 0.6)],
         [("FX 1.DENSITY", 0.6, 0.9), ("FX 1.SPRAY", 0.05, 0.6), ("FX 1.SIZE", 0.9, 0.4), ("FX 1.FREEZE", 0, 0.6)]],
        [["LFO 1", "FX 1.TONE", 0.5, "M2"], ["A TIDES", "SRC.TIMBRE", 0.4, "M2"]],
        SLOWLFO, None))

    # 2 ------------------------------------------------------------------------------------------- the dense cloud
    # Classic: large grains at high density, smooth window, a little scatter: a buttery, smeared backdrop (a tutorial's
    # description of "clouds"); small pitch scatter detunes it into a chorus.
    a(G("DENSE CLOUD", {"SRC": "BRAIDS", "MODA": "TIDES", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "ZLPF", "TIMBRE": 0.5, "COLOR": 0.2, "DRIFT": 0.2, "LEVEL": 1.0},
         "MODA": TI(0.2, "MID"),
         "FX1": CL(POSITION=0.5, SIZE=0.9, DENSITY=0.7, TEXTURE=0.85, BLEND=0.7, SPREAD=0.9, FEEDBACK=0.2, SPRAY=0.2,
                   **{"PITCH RND": 0.03}),
         "FX2": REV("HALL", 0.8, 0.25)},
        PADAMP,
        [[("SRC.TIMBRE", 0.4, 0.85), ("FX 1.TONE", -0.3, 0.1)],
         [("LFO.RATE 1", 0.1, 0.6), ("MOD A.FREQ", 0.1, 0.6)],
         [("FX 1.BLEND", 0.4, 0.95), ("FX 2.MIX", 0.05, 0.55)],
         [("FX 1.DENSITY", 0.65, 0.8), ("FX 1.SIZE", 0.9, 0.65), ("FX 1.PITCH RND", 0.0, 0.15)]],
        [["A TIDE+", "FX 1.TEXTURE", 0.4, "M2"], ["A TIDES", "SRC.TIMBRE", 0.3, "M2"], ["LFO 2", "FX 1.SIZE", 0.3, "M2"]],
        SLOWLFO, None))

    # 3 ---------------------------------------------------------------------------------------------- the time smear
    # Classic: time-stretch. Short plucks go in, their attacks are smeared into a long cloud by big overlapping grains
    # read from a slowly moving place in the buffer.
    a(G("TIME SMEAR", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "PLUK", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.1, "LEVEL": 1.0, "AD VCA": "ON", "AD DEC": 0.8},
         "FX1": CL(POSITION=0.15, SIZE=0.8, DENSITY=0.75, TEXTURE=0.9, BLEND=0.75, SPREAD=0.8, FEEDBACK=0.3, **{"IN GAIN": 0.9}),
         "FX2": REV("HALL", 0.85, 0.2)},
        {**PADAMP, "ATTACK": 0.0, "DECAY": 0.5, "SUSTAIN": 0.5, "RELEASE": 0.5},
        [[("SRC.TIMBRE", 0.2, 0.9), ("FX 1.TONE", -0.6, 0.1)],
         [("FX 1.SCAN", 0.0, 0.6), ("LFO.RATE 1", 0.1, 0.5)],
         [("FX 1.FEEDBACK", 0.05, 0.7), ("FX 2.MIX", 0.05, 0.6)],
         [("FX 1.SIZE", 0.5, 0.9), ("FX 1.DENSITY", 0.65, 0.8), ("FX 1.POSITION", 0.05, 0.4)]],
        [["LFO 1", "FX 1.TONE", 0.4, "M2"]],
        SLOWLFO, None))

    # 4 ------------------------------------------------------------------------------------------------ the slow swell
    # Approximation of a reverse-like swell (Clouds cannot play grains backwards): a Peaks envelope with a slow attack
    # opens the cloud (blend, feedback, brightness) after every note on, so each chord blooms from its own start.
    a(G("SLOW SWELL", {"SRC": "BRAIDS", "MODA": "PEAKS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "BOWD", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.15, "LEVEL": 1.0},
         "MODA": {"FUNC": "ENV", "K1": 0.5, "K2": 0.6, "GATE": "NOTE", "SUSTAIN": 1.0},
         "FX1": CL(POSITION=0.35, SIZE=0.85, DENSITY=0.8, TEXTURE=0.9, BLEND=0.25, SPREAD=0.9, FEEDBACK=0.2,
                   **{"IN GAIN": 0.3, "PITCH QNT": "5TH+OCT"}),
         "FX2": REV("HALL", 0.9, 0.3)},
        {**PADAMP, "RELEASE": 0.7},
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.45, 0.1)],
         [("AMP.ATTACK", 0.0, 1.0), ("MOD A.K1", 0.0, 1.0)],
         [("FX 1.BLEND", 0.15, 0.9), ("FX 2.MIX", 0.1, 0.55)],
         [("FX 1.TEXTURE", 0.3, 1.0), ("FX 1.SIZE", 0.95, 0.25), ("FX 1.PITCH RND", 0.0, 0.6)]],
        [["A PEAKS", "FX 1.BLEND", 0.65], ["A PEAKS", "FX 1.FEEDBACK", 0.25], ["LFO 1", "FX 1.TONE", 0.8, "M2"]],
        {**SLOWLFO, "RATE 1": 0.4}, None))

    # 5 ---------------------------------------------------------------------------------------------------- stutter
    # Classic: grains fired on the clock (GRN SYNC) with short square-ish windows: machine-exact pulses whose timbre
    # shifts with where they read. DENSITY sits at noon on purpose: only the clock makes grains.
    a(G("STUTTER", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "DELAY"},
        {"SRC": {"MODEL": "ZLPF", "TIMBRE": 0.5, "COLOR": 0.4, "DRIFT": 0.05, "LEVEL": 1.0},
         "FX1": CL(POSITION=0.1, SIZE=0.45, DENSITY=0.5, TEXTURE=0.3, BLEND=0.7, SPREAD=0.7, FEEDBACK=0.1,
                   **{"GRN SYNC": "1/16", "IN GAIN": 1.0, "PITCH QNT": "5TH+OCT"}),
         "FX2": DLY("1/8D", 0.45, 0.35)},
        {**PADAMP, "ATTACK": 0.0, "DECAY": 0.3, "SUSTAIN": 0.8, "RELEASE": 0.45},
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.4, 0.3)],
         [("FX 1.GRN SYNC", "1/2", "1/16"), ("FX 1.POSITION", 0.05, 0.5)],
         [("FX 2.MIX", 0.05, 0.4), ("FX 2.FEEDBACK", 0.15, 0.5)],
         [("FX 1.SIZE RND", 0.0, 0.7), ("FX 1.SPRAY", 0.0, 0.5), ("FX 1.PITCH RND", 0.0, 0.3), ("FX 1.TEXTURE", 0.1, 0.9)]],
        [["LFO 1", "FX 1.POSITION", 0.2, "M2"]],
        {**SLOWLFO, "SHAPE 1": "S&H", "SYNC 1": "1/4"}, None))

    # 6 ------------------------------------------------------------------------------------------------ beat repeat
    # Classic: a beat-repeat / stutter made by freezing a looping delay on the beat: a Peaks envelope on the clock
    # drives FREEZE, so a short slice repeats and then lets go. K2 sets how much of each beat is held.
    a(G("BEAT REPEAT", {"SRC": "BRAIDS", "MODA": "PEAKS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "ZLPF", "TIMBRE": 0.5, "COLOR": 0.2, "DRIFT": 0.05, "LEVEL": 1.0},
         "MODA": {"FUNC": "ENV", "K1": 0.0, "K2": 0.5, "GATE": "1/4", "SUSTAIN": 1.0},
         "FX1": CL(POSITION=0.3, SIZE=0.25, DENSITY=0.7, TEXTURE=0.5, BLEND=0.8, SPREAD=0.7, FEEDBACK=0.15,
                   MODE="LOOPDLY", **{"IN GAIN": 0.5}),
         "FX2": REV("PLATE", 0.5, 0.15)},
        {**PADAMP, "ATTACK": 0.0, "DECAY": 0.3, "SUSTAIN": 0.7, "RELEASE": 0.2},
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.4, 0.3)],
         [("MOD A.GATE", "1/4", "1/16"), ("FX 1.SIZE", 0.15, 0.4)],
         [("FX 2.MIX", 0.05, 0.4), ("FX 1.BLEND", 0.5, 0.9)],
         [("FX 1.PITCH", -0.25, 0.25), ("FX 1.POSITION", 0.1, 0.5), ("FX 1.TEXTURE", 0.2, 0.9), ("FX 1.FEEDBACK", 0.1, 0.4)]],
        [["A PEAKS", "FX 1.FREEZE", 1.0]],
        SLOWLFO, None, md=(0.5, 0.0, 0.5, 0.5)))

    # 7 -------------------------------------------------------------------------------------------------------- geiger
    # Classic: small grains at low density (a Geiger counter) with square windows: discontinuous spikes, glitch. A
    # sample-and-hold LFO throws the read point about; the echo (FX 2) turns the clicks into a wash.
    a(G("GEIGER", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "DELAY"},
        {"SRC": {"MODEL": "FOLD", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.1, "LEVEL": 1.0},
         "FX1": CL(POSITION=0.3, SIZE=0.12, DENSITY=0.57, TEXTURE=0.1, BLEND=0.9, SPREAD=0.9, FEEDBACK=0.1,
                   **{"PITCH RND": 0.15, "PITCH QNT": "5TH+OCT"}),
         "FX2": DLY("1/8", 0.35, 0.25, TONE=0.55)},
        {**PADAMP, "ATTACK": 0.02},
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.4, 0.4)],
         [("LFO.RATE 1", 0.35, 0.8), ("FX 1.SPRAY", 0.1, 0.6)],
         [("FX 2.MIX", 0.05, 0.5), ("FX 1.BLEND", 0.5, 0.9), ("FX 1.SPREAD", 0.3, 1.0)],
         [("FX 1.DENSITY", 0.6, 0.88), ("FX 1.SIZE", 0.1, 0.25), ("FX 1.TEXTURE", 0.1, 0.5)]],
        [["LFO 1", "FX 1.POSITION", 0.35, "M2"]],
        {**SLOWLFO, "SHAPE 1": "S&H"}, None))

    # 8 ------------------------------------------------------------------------------------------------------ shimmer
    # Classic: an octave-up grain cloud fed back on itself, so each pass climbs another octave and fades into the
    # reverb (shimmer). Feedback stays at or below 0.55 so it cannot run away.
    a(G("SHIMMER", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "ZLPF", "TIMBRE": 0.5, "COLOR": 0.2, "DRIFT": 0.15, "LEVEL": 1.0},
         "FX1": CL(POSITION=0.3, SIZE=0.7, PITCH=0.5, DENSITY=0.75, TEXTURE=0.85, BLEND=0.5, SPREAD=0.9, FEEDBACK=0.3),
         "FX2": REV("SHIMMER", 0.9, 0.3, SHIMMER=0.4)},
        PADAMP,
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.4, 0.2)],
         [("LFO.RATE 1", 0.1, 0.7), ("LFO.RATE 2", 0.1, 0.6)],
         [("FX 1.BLEND", 0.35, 0.9), ("FX 1.FEEDBACK", 0.15, 0.55), ("FX 2.MIX", 0.1, 0.5)],
         [("FX 1.DENSITY", 0.66, 0.8), ("FX 1.SIZE", 0.9, 0.65), ("FX 1.PITCH RND", 0.0, 0.1)]],
        [["LFO 1", "FX 1.TONE", 0.5, "M2"], ["LFO 2", "FX 1.PITCH", 0.03, "M2"]],
        SLOWLFO, None))

    # 9 ------------------------------------------------------------------------------------------------------ sparkle
    # Classic: pitch scatter: small grains of a bell, each shifted by a random interval kept to fifths and octaves, so
    # the notes become a glitter of harmonics.
    a(G("SPARKLE", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "BELL", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.05, "LEVEL": 1.0, "AD VCA": "ON", "AD DEC": 0.9},
         "FX1": CL(POSITION=0.3, SIZE=0.4, PITCH=0.25, DENSITY=0.85, TEXTURE=0.7, BLEND=0.75, SPREAD=1.0, FEEDBACK=0.15,
                   **{"PITCH RND": 0.3, "PITCH QNT": "5TH+OCT", "IN GAIN": 1.0}),
         "FX2": REV("PLATE", 0.7, 0.25)},
        {**PADAMP, "ATTACK": 0.0, "DECAY": 0.5, "SUSTAIN": 0.5, "RELEASE": 0.7},
        [[("SRC.TIMBRE", 0.15, 0.85), ("FX 1.TONE", -0.3, 0.5)],
         [("LFO.RATE 1", 0.2, 0.7), ("FX 1.SPRAY", 0.05, 0.5)],
         [("FX 1.BLEND", 0.3, 0.8), ("FX 2.MIX", 0.1, 0.5)],
         [("FX 1.PITCH RND", 0.05, 0.5), ("FX 1.SIZE", 0.5, 0.15), ("FX 1.DENSITY", 0.72, 0.95)]],
        [["LFO 1", "FX 1.POSITION", 0.25, "M2"]],
        SLOWLFO, None))

    # 10 ---------------------------------------------------------------------------------------------- pitch echo
    # Classic: a grain delay: regular grains (density below noon) read from the newest audio, square-ish windows, the
    # feedback making repeats, and PITCH making each repeat rise or fall; the tempo delay (FX 2) adds a second voice.
    a(G("PITCH ECHO", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "DELAY"},
        {"SRC": {"MODEL": "PLUK", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.05, "LEVEL": 1.0, "AD VCA": "ON", "AD DEC": 0.6},
         "FX1": CL(POSITION=0.0, SIZE=0.5, PITCH=0.0, DENSITY=0.3, TEXTURE=0.3, BLEND=0.7, SPREAD=0.7, FEEDBACK=0.4,
                   **{"IN GAIN": 0.9}),
         "FX2": DLY("1/8D", 0.3, 0.2)},
        {**PADAMP, "ATTACK": 0.0, "DECAY": 0.45, "SUSTAIN": 0.2, "RELEASE": 0.35},
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.4, 0.3)],
         [("FX 1.DENSITY", 0.15, 0.4), ("LFO.RATE 1", 0.1, 0.5)],
         [("FX 1.FEEDBACK", 0.1, 0.65), ("FX 2.MIX", 0.05, 0.4)],
         [("FX 1.PITCH", -0.25, 0.5), ("FX 1.SIZE", 0.3, 0.7)]],
        [["LFO 1", "FX 1.POSITION", 0.1, "M2"]],
        SLOWLFO, None))

    # 11 ---------------------------------------------------------------------------------------------- micro loop
    # Classic: a micro-loop: the looping delay is frozen after a note starts, so the first slice of each note repeats
    # for as long as the key is down (a Peaks envelope on the note gate drives FREEZE; its attack is the capture time).
    a(G("MICRO LOOP", {"SRC": "BRAIDS", "MODA": "PEAKS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "FLUT", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.1, "LEVEL": 1.0},
         "MODA": {"FUNC": "ENV", "K1": 0.4, "K2": 0.5, "GATE": "NOTE", "SUSTAIN": 1.0},
         "FX1": CL(POSITION=0.3, SIZE=0.3, DENSITY=0.7, TEXTURE=0.5, BLEND=0.9, SPREAD=0.7, FEEDBACK=0.15, MODE="LOOPDLY"),
         "FX2": REV("PLATE", 0.6, 0.2)},
        {**PADAMP, "ATTACK": 0.05},
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.4, 0.3)],
         [("MOD A.K1", 0.2, 0.7), ("FX 1.POSITION", 0.1, 0.6)],
         [("FX 2.MIX", 0.1, 0.5), ("FX 1.BLEND", 0.55, 0.9)],
         [("FX 1.SIZE", 0.1, 0.7), ("FX 1.PITCH", -0.25, 0.25), ("FX 1.DENSITY", 0.55, 0.9)]],
        [["A PEAKS", "FX 1.FREEZE", 1.0], ["LFO 1", "FX 1.POSITION", 0.15, "M2"]],
        SLOWLFO, None))

    # 12 ------------------------------------------------------------------------------------------------- deep note
    # Classic: the "THX" swell (a published Clouds tip): sawtooth chords with heavy random pitch scatter in fifths and
    # octaves, converging as K4 comes down to nothing.
    a(G("DEEP NOTE", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "//x3", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.3, "LEVEL": 1.0},
         "FX1": CL(POSITION=0.3, SIZE=0.7, DENSITY=0.8, TEXTURE=0.8, BLEND=0.85, SPREAD=1.0, FEEDBACK=0.2,
                   **{"PITCH QNT": "5TH+OCT"}),
         "FX2": REV("HALL", 0.9, 0.25)},
        {**PADAMP, "ATTACK": 0.35},
        [[("SRC.TIMBRE", 0.1, 0.9), ("FX 1.TONE", -0.7, 0.1)],
         [("LFO.RATE 1", 0.1, 0.6), ("LFO.RATE 2", 0.1, 0.8)],
         [("FX 2.MIX", 0.1, 0.5), ("FX 1.SPREAD", 0.5, 1.0)],
         [("FX 1.PITCH RND", 0.0, 0.6), ("FX 1.DENSITY", 0.7, 0.92), ("FX 1.SIZE", 0.9, 0.5)]],
        [["LFO 1", "FX 1.TONE", 0.5, "M2"], ["LFO 2", "FX 1.PITCH", 0.25, "M2"], ["LFO 1", "FX 1.SPRAY", 0.3, "M2"]],
        SLOWLFO, None))

    # 13 ---------------------------------------------------------------------------------------------------- rain
    # Classic: dense noise washes: tiny grains of noise at high density with scatter in position and pitch. K4 runs
    # from a drizzle to a downpour; K2 is the gusts.
    a(G("RAIN", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "NOIS", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.0, "LEVEL": 1.0},
         "FX1": CL(POSITION=0.5, SIZE=0.15, DENSITY=0.8, TEXTURE=0.5, BLEND=0.85, SPREAD=1.0, FEEDBACK=0.1, SPRAY=0.5,
                   **{"PITCH RND": 0.15, "IN GAIN": 0.6}),
         "FX2": REV("PLATE", 0.6, 0.25)},
        {**PADAMP, "ATTACK": 0.1},
        [[("SRC.TIMBRE", 0.2, 0.8), ("FX 1.TONE", -0.5, 0.2)],
         [("LFO.RATE 1", 0.1, 0.5), ("FX 1.SPRAY", 0.2, 0.9)],
         [("FX 2.MIX", 0.05, 0.55), ("FX 1.SPREAD", 0.2, 1.0)],
         [("FX 1.DENSITY", 0.66, 0.92), ("FX 1.SIZE", 0.08, 0.25), ("FX 1.PITCH RND", 0.05, 0.3)]],
        [["LFO 1", "FX 1.TONE", 0.3, "M2"], ["LFO 2", "SRC.TIMBRE", 0.2, "M2"]],
        SLOWLFO, None))

    # 14 ---------------------------------------------------------------------------------------------- spectral fog
    # Classic: spectral blur. Clouds' spectral mode (mono) smears a chord's partials: below the middle DENSITY stops
    # bins updating (a partial freeze), SIZE warps the spectrum, TEXTURE thins or fattens it.
    a(G("SPECTRAL FOG", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "WTBL", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.2, "LEVEL": 1.0},
         "FX1": CL(POSITION=0.4, SIZE=0.5, DENSITY=0.3, TEXTURE=0.6, BLEND=0.8, SPREAD=0.7, FEEDBACK=0.1,
                   MODE="SPECTRL", QUALITY="16 MONO"),
         "FX2": REV("HALL", 0.8, 0.25)},
        PADAMP,
        [[("SRC.TIMBRE", 0.3, 0.85), ("FX 1.TONE", -0.4, 0.3)],
         [("LFO.RATE 1", 0.1, 0.5), ("FX 1.POSITION", 0.2, 0.7)],
         [("FX 1.BLEND", 0.5, 0.9), ("FX 2.MIX", 0.1, 0.5)],
         [("FX 1.DENSITY", 0.55, 0.25), ("FX 1.SIZE", 0.5, 0.9), ("FX 1.TEXTURE", 0.5, 0.9)]],
        [["LFO 1", "FX 1.SIZE", 0.2, "M2"]],
        SLOWLFO, None))

    # 15 ---------------------------------------------------------------------------------------------- tape decay
    # My own: lo-fi tape. The looping delay at 8-bit (Clouds' companded mode, which its manual likens to a cassette)
    # feeds back through a wobbling read point and the tape-mode delay, so each repeat wears a little more.
    a(G("TAPE DECAY", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "DELAY"},
        {"SRC": {"MODEL": "ZLPF", "TIMBRE": 0.5, "COLOR": 0.2, "DRIFT": 0.3, "LEVEL": 1.0, "BITS": "16"},
         "FX1": CL(POSITION=0.25, SIZE=0.9, DENSITY=0.6, TEXTURE=0.5, BLEND=0.6, SPREAD=0.7, FEEDBACK=0.35,
                   MODE="LOOPDLY", QUALITY="8 ST"),
         "FX2": DLY("1/8", 0.35, 0.25, "TAPE", WOW=0.4, TONE=0.4)},
        {**PADAMP, "ATTACK": 0.05, "RELEASE": 0.5},
        [[("SRC.TIMBRE", 0.15, 0.85), ("FX 1.TONE", -0.7, 0.0)],
         [("FX 2.WOW", 0.1, 0.9), ("LFO.RATE 2", 0.1, 0.8)],
         [("FX 1.BLEND", 0.3, 0.9), ("FX 2.MIX", 0.05, 0.5)],
         [("FX 1.FEEDBACK", 0.1, 0.6), ("SRC.BITS", "16", "6"), ("FX 1.POSITION", 0.1, 0.5)]],
        [["LFO 1", "FX 1.POSITION", 0.2, "M2"], ["LFO 2", "FX 1.SIZE", 0.1, "M2"], ["LFO 2", "FX 1.BLEND", 0.3, "M2"]],
        SLOWLFO, None))

    # 16 --------------------------------------------------------------------------------------------- grain choir
    # My own: a choir. Braids' vowel voice into Clouds; each new note fires a grain (NOTE TRG), so chord changes speak,
    # and K1 walks the vowel while K4 turns the choir from smooth to breathy and scattered.
    a(G("GRAIN CHOIR", {"SRC": "BRAIDS", "FX1": "CLOUDS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "VOWL", "TIMBRE": 0.5, "COLOR": 0.5, "DRIFT": 0.2, "LEVEL": 1.0},
         "FX1": CL(POSITION=0.4, SIZE=0.7, DENSITY=0.7, TEXTURE=0.8, BLEND=0.65, SPREAD=0.9, FEEDBACK=0.15,
                   **{"NOTE TRG": "ON"}),
         "FX2": REV("HALL", 0.85, 0.3)},
        PADAMP,
        [[("SRC.TIMBRE", 0.1, 0.9), ("FX 1.TONE", -0.5, 0.2)],
         [("LFO.RATE 1", 0.1, 0.5), ("FX 1.POSITION", 0.2, 0.6)],
         [("FX 1.BLEND", 0.4, 0.95), ("FX 2.MIX", 0.1, 0.5)],
         [("FX 1.DENSITY", 0.6, 0.92), ("FX 1.SPRAY", 0.0, 0.5), ("FX 1.SIZE", 0.9, 0.4)]],
        [["LFO 1", "FX 1.SIZE", 0.2, "M2"], ["LFO 2", "SRC.TIMBRE", 0.1, "M2"]],
        SLOWLFO, None))
    return L


KINDS = {"FROZEN PAD": "freeze", "TIME SMEAR": "pluck", "STUTTER": "stab", "BEAT REPEAT": "stab", "SPARKLE": "bell",
         "PITCH ECHO": "pluck", "SLOW SWELL": "swell", "TAPE DECAY": "notes", "MICRO LOOP": "notes", "DEEP NOTE": "low", "RAIN": "single"}      # the demo phrase (granular_batch.py), default "pad"


if __name__ == "__main__":
    print(len(granular()))
