# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 update 24: the factory bank of 45 (plus INIT at 0), built for the knob layer (update 19).

Every preset is built from `spec(...)`: a preset of the older banks taken as it is (kept), changed (rebuilt) or a new one,
then given four knobs that each drive 4 to 8 targets of their own (the knob layer, engine/rack.cc: "mtgt"), each with a depth, a
window of the knob's travel and a curve. Where the older preset's four macros were matrix rows (M1 to M4 to a destination) they
are turned into knob targets (the same sound: a row's amount x the knob = a target's depth x a linear curve over the whole travel),
which gives the rows back; the rows that stay are the preset's own modulation (Tides, Peaks, LFOs ...: "via M2" rows).

The grammar of a knob (K1 to K4 each go their own way, chosen per preset):
  the gentle part of the travel (the lower half to two thirds) is the musical part: the sound's own colour, speed, space and
  texture moving a little; near the top the targets that start late (a window from 0.6 to 0.9, slow-start or step curves)
  take it to glitch, wash or stutter: Braids' bit and sample-rate crush, filter drive and audio-rate FM, the LFOs' rates and
  depths, Tides' speed, delay feedback, reverb size and shimmer, chorus feedback, a random pitch scatter, Clouds.

Target text: "DEST depth start end curve ; ..." (curve L linear, SL slow-start, F fast-start, T step, S smooth S); DEST as the matrix names
a destination, "ROW n" a matrix row's depth (13 to 18: the LFOs' own targets), "ROW @n" the n-th row this preset adds."""
import copy
import json
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

CV = {"L": "LIN", "SL": "SLOW", "F": "FAST", "T": "STEP", "S": "S"}
SLOTK = {"SRC": "SRC", "SHP": "SHP", "MOD A": "MODA", "MOD B": "MODB", "FX 1": "FX1", "FX 2": "FX2"}
_D = [None]


def defs():
    if _D[0] is None:
        import fm1_presets as F
        _D[0] = (F, F.defs())
    return _D[0]


def labels(mod):
    F, D = defs()
    return [p["l"] for p in F.flat(D["MODULES"][mod])]


def has(q, dest):
    """does this destination exist in the preset's modules (as fm1_presets.dst_ix would find it)"""
    F, D = defs()
    if dest in ("PITCH", "PAN") or dest.startswith("ROW"):
        return True
    if dest.startswith("LFO."):
        return dest[4:] in [p["l"] for p in F.flat(D["LFOS"])]
    if dest.startswith("AMP."):
        return dest[4:] in [p["l"] for p in F.flat(D["AMP"])]
    sl, _, par = dest.partition(".")
    key = SLOTK.get(sl)
    mod = q["slots"].get(key) if key else None
    return bool(mod) and par in labels(mod)


def parse(txt):
    out = []
    for part in txt.split(";"):
        part = part.strip()
        if not part:
            continue
        dest, d, s, e, c = part.rsplit(None, 4)
        out.append((dest, float(d), float(s), float(e), CV[c]))
    return out


def convert(q):
    """the four macros' matrix rows (a source M1..M4 and no via) as knob targets (the same sound); returns the targets and the rows left"""
    tg, keep = [], []
    for r in q["matrix"]:
        if r[0] in ("M1", "M2", "M3", "M4") and len(r) == 3:
            tg.append((int(r[0][1]), r[1], float(r[2]), 0.0, 1.0, "LIN"))
        else:
            keep.append(list(r))
    return tg, keep


def spec(src, name=None, cat=None, level=None, move="subtle", macros=None, edit=None, rows=(), lfo_tgt=None, k=(), voices=None,
         summary="", drop=(), pool=None, kind="", note="", slots=None, win=None, lfo_set=None):
    """one preset of the bank taken from an older one. k: four (label, sentence, "targets") for K1..K4; drop: converted targets to leave out (knob, dest)"""
    q = copy.deepcopy(next(p for p in pool if p["name"] == src))
    if name:
        q["name"] = name
    if cat:
        q["cat"] = cat
    for sect, kv in (edit or {}).items():
        key = {"AMP": "amp", "LFO": "lfo"}.get(sect)
        tgt = q.setdefault(key, {}) if key else q["p"].setdefault(sect, {})
        tgt.update(kv)
    if slots:
        q["slots"].update(slots)
    if voices:
        q["fm1_voices"] = voices
    if not q.get("fm1_voices"):
        q["fm1_voices"] = 1 if q["amp"].get("MODE") in ("MONO", "LEGATO") else 6
    if level is not None:
        q["amp"]["LEVEL"] = level
    tg, q["matrix"] = convert(q)
    tg = [t for t in tg if (t[0], t[1]) not in set(drop)]
    return _finish(q, tg, src, rows, lfo_tgt, lfo_set, macros, k, move, summary, kind, note, win)


def newp(name, cat, slots, p, amp, k, lfo=None, lfo_tgt=None, rows=(), macros=(0.5, 0.5, 0.5, 0.5), voices=6, level=0.5, move="subtle",
         summary="", kind="", note="", win=None, osc=None):
    """a new preset (no older one behind it): the modules' parameters as presets_batch2.B takes them"""
    from presets_batch2 import B, AM, LF
    a = dict(amp)
    a.setdefault("LEVEL", level)
    q = B(name, cat, slots, p, AM(**a), [[], [], [], []], [], lfo or LF(), list(lfo_tgt or []), voices=voices, osc=osc)
    q["matrix"] = []
    return _finish(q, [], "new", rows, None, None, list(macros), k, move, summary, kind, note, win)


def _finish(q, tg, src, rows, lfo_tgt, lfo_set, macros, k, move, summary, kind, note, win):
    for r in rows:
        q["matrix"].append(list(r))
    if len(q["matrix"]) > 12:
        raise SystemExit(f"{q['name']}: {len(q['matrix'])} matrix rows")
    if lfo_tgt is not None:
        q["lfo_tgt"] = list(lfo_tgt) + [None] * (6 - len(lfo_tgt))
    if lfo_set:
        t = list(q.get("lfo_tgt") or [None] * 6)
        t += [None] * (6 - len(t))
        for i, v in lfo_set.items():
            t[i] = v
        q["lfo_tgt"] = t
    if macros:
        q["macros"] = list(macros)
    nrows0 = len(q["matrix"]) - len(rows)
    out = {1: [], 2: [], 3: [], 4: []}
    for kk, dest, dep, s, e, c in tg:
        out[kk].append((dest, dep, s, e, c))
    labs, sens = [], []
    for i in range(4):
        lab, sen, txt = k[i]
        labs.append(lab)
        sens.append(sen)
        for dest, dep, s, e, c in parse(txt):
            if dest.startswith("ROW @"):
                dest = f"ROW {nrows0 + int(dest[5:])}"
            if not has(q, dest):
                raise SystemExit(f"{q['name']}: K{i + 1} has no destination {dest}")
            out[i + 1].append((dest, dep, s, e, c))
    q["mtgt"] = []
    trimmed = []
    for kk in (1, 2, 3, 4):
        lst = out[kk]
        n_extra = len(parse(k[kk - 1][2]))
        conv = lst[:len(lst) - n_extra]
        extra = lst[len(lst) - n_extra:]
        while len(conv) + len(extra) > 8:
            j = min(range(len(conv)), key=lambda i: abs(conv[i][1]))
            trimmed.append((kk, conv.pop(j)))
        for dest, dep, s, e, c in conv + extra:
            q["mtgt"].append([kk, dest, round(max(-1.27, min(1.27, dep)), 2), s, e, c])
    q["bank"] = 1
    q["bank45"] = {"labels": labs, "knobs": sens, "summary": summary, "move": move, "src": src, "kind": kind, "note": note,
                   "trimmed": [(a, b[0]) for a, b in trimmed], "win": win}
    q["move"] = move
    return q


def all_checks(L):
    for q in L:
        n = [sum(1 for t in q["mtgt"] if t[0] == k) for k in (1, 2, 3, 4)]
        if any(not 4 <= x <= 8 for x in n):
            raise SystemExit(f"{q['name']}: knob targets {n} (4 to 8 each)")
        if len(q["name"]) > 16:
            raise SystemExit(f"{q['name']}: name too long")


def bank45(pool):
    """INIT first, then the 45 (the pool is the full older list, from presets_fm1.fm1_presets)"""
    import os
    out = [next(p for p in pool if p["name"] == "INIT")]
    if os.environ.get("B8LAB"):         # a scratch bank for sound experiments: the file defines L (a list of presets, built with spec/B)
        g = {"pool": pool}
        exec(open(os.environ["B8LAB"]).read(), g)
        return out + g["L"]
    from bank45_r1 import round1
    rounds = int(os.environ.get("B8ROUNDS", "3"))      # the build is in three rounds of 15: B8ROUNDS=1 builds positions 1 to 15 only (and so on)
    out += round1(pool)
    if rounds >= 2:
        from bank45_r2 import round2
        out += round2(pool)
    if rounds >= 3:
        from bank45_r3 import round3
        out += round3(pool)
    lv = HERE / "presets_bank45_levels.json"
    if lv.exists():                     # the loudness fit (fit_levels.py): AMP LEVEL per preset, measured
        L = json.loads(lv.read_text())
        for q in out[1:]:
            if q["name"] in L:
                q["amp"]["LEVEL"] = L[q["name"]]
    all_checks(out[1:])
    names = [q["name"] for q in out]
    if len(set(names)) != len(names):
        raise SystemExit("bank45: a name twice")
    return out
