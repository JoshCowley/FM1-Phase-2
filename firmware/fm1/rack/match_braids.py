#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 (14 Nov 2026): chooses, for every factory preset that played Plaits, the Braids model, TIMBRE and COLOR
that sound closest to it, and writes presets_fm1_braids.json, which presets_fm1.py applies (to_braids).

Each preset is played twice through the engine on this computer (rack.cc built as the FM-1's, Plaits still in, as a
library: build/match/librack.so): as it was, on Plaits, and with every Braids candidate in its place, the rest of the
preset (shaper, AMP, FUNC, LFOs, matrix, effects) as presets_fm1.py carries it over. A phrase that suits the preset's
category (as audition.c's: chords for keys, pads and textures, single notes for basses and leads). The distance:
  - spectral shape: third-octave bands, 63 Hz-12.5 kHz, each frame's bands relative to its total, averaged (dB)
  - spectral centroid over time (octaves, x 6: an octave counts as 6 dB)
  - loudness envelope over time (dB below its own peak, floor -50; x 0.3)
  - tonality: spectral flatness, 200 Hz-5 kHz, over time (x 20: 0 a pure tone, 1 white noise), so a noise model can't
    stand in for a pitched sound because the reverb blurs both alike
Loudness itself is left to level_fm1.py. Two rules on top of the distance: a preset played as an instrument (keys,
basses, leads, pads: CH BS LD PD) keeps a pitched model (no noise or drum models: UNPITCHED), and FX SPEAK keeps a
voice (ALLOW). The FX presets may take anything. Two passes: every model at four TIMBRE/COLOR corners, then the best four
models on a 5 x 5 grid.

  EURORACK=/path/to/eurorack python3 match_braids.py [NAME ...]     (default: every Plaits preset)
"""
import ctypes
import json
import os
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
ENG = HERE.parent.parent.parent / "engine"
WEB = HERE.parent.parent.parent / "web"
TESTS = HERE.parent.parent / "tests"
OUT = HERE / "presets_fm1_braids.json"
LIB = HERE / "build" / "match" / "librack.so"
SR = 48000
BLOCK = 480
SECS = 2.4

sys.path.insert(0, str(HERE))
import fm1_presets as F          # noqa: E402
from presets_fm1 import fm1_presets, to_braids   # noqa: E402

BRAIDS_MODELS = ["CSAW", "/\\-_", "//-_", "FOLD", "UUUU", "SUB-", "SUB/", "SYN-", "SYN/", "//x3", "-_x3", "/\\x3",
                 "SIx3", "RING", "////", "//uu", "TOY*", "ZLPF", "ZPKF", "ZBPF", "ZHPF", "VOSM", "VOWL", "VFOF", "HARM",
                 "FM", "FBFM", "WTFM", "PLUK", "BOWD", "BLOW", "FLUT", "BELL", "DRUM", "KICK", "CYMB", "SNAR", "WTBL",
                 "WMAP", "WLIN", "WTx4", "NOIS", "TWLN", "CLKN", "CLOU", "PRTC", "QPSK"]


UNPITCHED = {"CYMB", "SNAR", "KICK", "NOIS", "TWLN", "CLKN", "QPSK", "PRTC", "CLOU"}
ALLOW = {"FX SPEAK": ["VOWL", "VFOF", "VOSM"]}


def candidates(name):
    if name in ALLOW:
        return ALLOW[name]
    if name[:2] in ("CH", "BS", "LD", "PD") or name == "INIT":
        return [m for m in BRAIDS_MODELS if m not in UNPITCHED]
    return BRAIDS_MODELS


def build_lib():
    eur = Path(os.environ.get("EURORACK", HERE.parent.parent.parent.parent / "eurorack"))
    od = LIB.parent / "o"
    od.mkdir(parents=True, exist_ok=True)
    cxx = (f"c++ -O2 -fPIC -ffp-contract=off -fno-exceptions -fno-rtti -std=gnu++11 -w -DTEST -DRACK_FM1 -DRACK_LITE "
           f"-DRACK_ARENA_KB=304 -DRACK_RINGS16 -I{ENG / 'shim'} -I{eur}")
    objs = []
    for src in (ENG / "sources.txt").read_text().split():
        if src.startswith(("elements/", "warps/", "clouds/", "marbles/")):
            continue
        o = od / (src.replace("/", "_")[:-3] + ".o")
        if not o.exists():
            subprocess.run(f"{cxx} -c {eur / src} -o {o}", shell=True, check=True)
        objs.append(str(o))
    for name in ("grids_nodes.cc", "rack.cc"):
        o = od / ("x_" + name[:-3] + ".o")
        subprocess.run(f"{cxx} -c {ENG / name} -o {o}", shell=True, check=True)
        objs.append(str(o))
    subprocess.run(f"c++ -shared -o {LIB} {' '.join(objs)} -lm", shell=True, check=True)


class Engine:
    def __init__(self):
        L = ctypes.CDLL(str(LIB))
        f, i = ctypes.c_float, ctypes.c_int
        sig = {"set_module": [i, i], "set_param": [i, i, f], "set_meta": [i, i, f, f, f],
               "set_route": [i, i, i, i, f, i], "set_macro": [i, f], "set_master": [f], "set_master_fx": [f, f, f, f],
               "set_legato": [i], "set_vintage": [f], "set_ext_slew": [i, f], "set_tune": [i, f], "set_hw": [i],
               "set_voices": [i], "note_on": [f, f], "note_off": [f], "render": [i]}
        for n, a in sig.items():
            getattr(L, n).argtypes = a
        L.out_l.restype = ctypes.POINTER(ctypes.c_float)
        L.out_r.restype = ctypes.POINTER(ctypes.c_float)
        self.L = L
        try:
            self.rng = ctypes.c_uint32.in_dll(L, "_ZN6stmlib6Random10rng_state_E")
        except ValueError:
            self.rng = None

    def play(self, pc, events):
        L = self.L
        if self.rng is not None:
            self.rng.value = 0x21
        L.init()
        for c in pc["calls"]:
            getattr(L, c[0])(*c[1:])
        cap = pc.get("fm1", {}).get("fm1_voices", 6)
        L.set_voices(1 if pc["mode"] in (1, 2) else min(6, pc["voices"], cap))
        L.fm1_fit()
        n = int(SECS * SR) // BLOCK
        out = np.zeros(n * BLOCK, dtype=np.float32)
        ev = sorted([(t, 1, note, vel) for t, d, note, vel in events] +
                    [(t + d, 0, note, 0) for t, d, note, vel in events])
        k = 0
        for b in range(n):
            t = b * BLOCK / SR
            while k < len(ev) and ev[k][0] <= t:
                _, on, note, vel = ev[k]
                L.note_on(note, vel) if on else L.note_off(note)
                k += 1
            L.render(BLOCK)
            l = np.ctypeslib.as_array(L.out_l(), (BLOCK,))
            r = np.ctypeslib.as_array(L.out_r(), (BLOCK,))
            out[b * BLOCK:(b + 1) * BLOCK] = l + r
        return out


def phrase(name):
    cat = name[:2]
    if cat == "BS":
        return [(0.0, 0.5, 36, 0.8), (0.6, 0.5, 43, 0.8), (1.2, 0.8, 36, 0.9)]
    if cat == "LD":
        return [(0.0, 0.6, 60, 0.8), (0.7, 0.9, 67, 0.8)]
    if cat == "CH":
        return [(0.0, 1.0, n, 0.75) for n in (48, 52, 55, 59)] + [(1.1, 0.7, 67, 0.8)]
    return [(0.0, 2.0, n, 0.7) for n in (48, 55, 59, 64)]


NFFT, HOP = 2048, 512
_win = np.hanning(NFFT).astype(np.float32)
_freqs = np.fft.rfftfreq(NFFT, 1.0 / SR)
_edges = 63.0 * 2 ** (np.arange(0, 24 + 1) / 3.0)           # third octaves, 63 Hz .. 12.7 kHz
_band = np.digitize(_freqs, _edges) - 1


def features(x):
    frames = np.lib.stride_tricks.sliding_window_view(x, NFFT)[::HOP] * _win
    p = np.abs(np.fft.rfft(frames, axis=1)) ** 2 + 1e-12
    bands = np.stack([p[:, _band == b].sum(axis=1) for b in range(24)], axis=1)
    tot = bands.sum(axis=1)
    tdb = 10 * np.log10(tot)
    act = tdb > tdb.max() - 40
    shape = (10 * np.log10(bands[act]) - tdb[act, None]).mean(axis=0)
    cent = np.log2((p[:, 1:] * _freqs[1:]).sum(axis=1) / p[:, 1:].sum(axis=1) + 1.0)
    env = np.maximum(tdb - tdb.max(), -50.0)
    mid = p[:, (_freqs >= 200) & (_freqs <= 5000)]
    flat = np.exp(np.log(mid).mean(axis=1)) / mid.mean(axis=1)
    return shape, cent, env, act, flat


def distance(a, b):
    sa, ca, ea, aa, fa = a
    sb, cb, eb, ab, fb = b
    both = aa & ab
    c = np.abs(ca[both] - cb[both]).mean() * 6 if both.any() else 12.0
    t = np.abs(fa[both] - fb[both]).mean() * 20 if both.any() else 10.0
    return float(np.abs(sa - sb).mean() + c + 0.3 * np.abs(ea - eb).mean() + t)


def main():
    if not LIB.exists() or "--rebuild" in sys.argv:
        build_lib()
    names = [a for a in sys.argv[1:] if not a.startswith("--")]
    page = json.loads((WEB / "presets.json").read_text())
    D = F.defs()
    tail = F.tail_of(json.loads((TESTS / "presets-calls.json").read_text()))
    orig = {q["name"]: q for q in fm1_presets(page, braids={}) if q["slots"].get("SRC") == "PLAITS"}
    todo = names or list(orig)
    E = Engine()
    res = json.loads(OUT.read_text()) if OUT.exists() else {}

    def calls(q):
        c = F.calls_for(q, D, tail)
        c["fm1"] = {k: q[k] for k in ("fm1_voices", "osc") if k in q}
        return c

    def try_one(q, ev, ref, model, t, c):
        cand = to_braids(json.loads(json.dumps(q)), {"model": model, "timbre": t, "color": c})
        x = E.play(calls(cand), ev)
        if not np.isfinite(x).all() or np.abs(x).max() < 1e-5:
            return 1e9
        return distance(ref, features(x))

    for name in todo:
        q = orig[name]
        ev = phrase(name)
        ref = features(E.play(calls(q), ev))
        first = []
        for m in candidates(name):
            d = min(try_one(q, ev, ref, m, t, c) for t in (0.25, 0.75) for c in (0.25, 0.75))
            first.append((d, m))
        first.sort()
        best = (1e9, None, 0, 0)
        for _, m in first[:4]:
            for t in np.linspace(0.1, 0.9, 5):
                for c in np.linspace(0.1, 0.9, 5):
                    d = try_one(q, ev, ref, m, float(t), float(c))
                    if d < best[0]:
                        best = (d, m, round(float(t), 2), round(float(c), 2))
        d, m, t, c = best
        plaits = q["p"]["SRC"].get("MODEL", "?")
        res[name] = {"model": m, "timbre": t, "color": c, "from": plaits, "distance": round(d, 2)}
        print(f"{name:16} {plaits:9} -> {m:5} T {t:.2f} C {c:.2f}  distance {d:5.2f}  "
              f"(next: {', '.join(f'{mm} {dd:.1f}' for dd, mm in first[1:4])})", flush=True)
        OUT.write_text(json.dumps(res, indent=1, sort_keys=True) + "\n")


if __name__ == "__main__":
    sys.exit(main())
