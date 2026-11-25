#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 update 12: the measurement card for each of the 16 granular presets (presets_granular.py).

  python3 granular_report.py [NAME ...] [--mp3 DIR] [--json FILE] [--host-log FILE]

Uses granular_batch.py's render and analysis (render_preset, the firmware's own sound path at 44.1 kHz). Per preset:
  16 s sweep render (K1 0-4 s, K2 4-8 s, K3 8-12 s, K4 12-16 s, the others at the preset's defaults): peak, RMS per 4 s;
  per macro: centroid and RMS at the start and end of its sweep (0.7 s windows), each also divided by the same windows of
  a control render (no macro moved) so the phrase's own change is not credited to the knob, and the same two figures from a
  clean A/B (the macro at 0 and at 1 over the same 2-10 s of the phrase); the macro passes if any of those centroid or RMS
  ratios is 1.25 or more (or 0.8 or less), or the A/B shows the rhythm or the grain texture change (granular_batch's tests);
  hands-off: 8 s with nothing moving (a held chord, or the phrase for the rhythmic ones): the peak-to-peak of the 1 s RMS (dB)
  and the max/min of the 1 s centroid (window hop 0.5 s); movement passes at 2 dB or 1.2x;
  loudness: RMS of the control render against the batch median (dB; pass within 4 dB);
  the 16 macro corners: loudest peak, quietest 2 s, growth of the peak (feedback).
  Estimated load per core (an estimate from the owner's FM-1 measurements; nothing here ran on an FM-1)."""
import json
import re
import sys
import tempfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import granular_batch as gb
KINDS = {}   # (2.48: the granular batch is gone; this file keeps the helpers bank45_report.py uses)

SR = gb.SR
RHYTHMIC = ("stab", "pluck", "bell", "notes")


def db(x):
    return 20 * np.log10(max(x, 1e-9))


def hands_off(name, md, kind, tmp):
    held = [(0.2, 9.8, n, 0.7) for n in gb.CH_A]
    ev = gb.PHRASE[kind] if kind in RHYTHMIC else held
    p = tmp / "off.wav"
    gb.render(name, gb.script(ev, md, sweeps=False, secs=9), p, tmp)
    s = gb.load(p)
    r = [db(gb.rms(s, t, t + 1.0)) for t in np.arange(1.0, 8.01, 0.5)]
    c = [gb.centroid(s, t, t + 1.0) for t in np.arange(1.0, 8.01, 0.5)]
    return {"rms_pp_dB": float(max(r) - min(r)), "cent_ratio": float(max(c) / max(min(c), 1)), "cent_min": float(min(c)), "cent_max": float(max(c)),
            "std250_dB": 0.0}


def estimate(q):
    """(core one %, core two % at the defaults, core two % worst case over the macros): the owner's measured figures"""
    sl = q["slots"]
    v = q["fm1_voices"]
    one = 8.0 * v
    one += {"REVERB": 14.0, "DELAY": 6.6, "CHORUS": 5.0}.get(sl.get("FX2", ""), 0.0)
    one += 3.5 if sl.get("MODA") in ("TIDES", "PEAKS") else 0.0
    cl = q["p"]["FX1"]

    def cost(d):
        mode = cl["MODE"]
        if mode == "LOOPDLY":
            c = 20.0
        elif mode == "SPECTRL":
            c = 12.0
        else:
            c = float(np.interp(d, [0.7, 0.9, 1.0], [16.5, 29.0, 42.0])) if d >= 0.7 else 16.5
        return c + 22.0 + (12.5 if cl["QUALITY"].startswith("8") else 0.0)

    d0 = cl["DENSITY"]
    dmax = d0
    for r in q["matrix"]:
        if r[1] == "FX 1.DENSITY" and r[0].startswith("M"):
            dmax = max(dmax, d0 + max(r[2], 0.0))
    dd = d0
    for r in q["matrix"]:
        if r[1] == "FX 1.DENSITY" and r[0].startswith("M"):
            dd = d0 + r[2] * q["macros"][int(r[0][1]) - 1]
    return one, cost(dd), cost(dmax)


def measure(q, num, tmp, mp3):
    name, md = q["name"], q["macros"]
    kind = KINDS.get(name, "pad")
    r = gb.measure(name, md, kind, tmp, mp3)
    r["num"] = num
    r["off"] = hands_off(name, md, kind, tmp)
    # control render's RMS (defaults, nothing moved)
    p = tmp / "ctl2.wav"
    gb.render(name, gb.script(gb.PHRASE[kind], md, sweeps=False), p, tmp)
    r["ctl_rms"] = gb.rms(gb.load(p), 0.5, 15.5)
    r["est"] = estimate(q)
    ev = []
    for k in range(4):
        m, A = r["macros"][k], r["ab"][k]
        sc = (m["c1"] / m["c0"]) / max(m["ctl"], 1e-9)       # sweep, control taken out
        sr = 10 ** (((m["f1"]["rms_dB"] - m["f0"]["rms_dB"]) - (m["g1"]["rms_dB"] - m["g0"]["rms_dB"])) / 20)
        ar = 10 ** (A["level"] / 20)
        big = lambda x: x >= 1.25 or x <= 0.8
        why = [n for n, ok in (("sweep centroid", big(sc)), ("sweep RMS", big(sr)), ("A/B centroid", big(A["cent"])), ("A/B RMS", big(ar)),
                               ("rhythm", A["envcorr"] <= 0.7), ("grain", big(A["am"])),
                               ("motion", k == 1 and (A["motion"] >= 1.5 or A["motion"] <= 0.67))) if ok]
        ev.append({"sc": sc, "sr": sr, "ab_cent": A["cent"], "ab_rms": ar, "why": why, "pass": bool(why)})
    r["ev"] = ev
    return r


def host_lines(path):
    d = {}
    for ln in Path(path).read_text().splitlines():
        m = re.match(r"3 \S+\s+(.+?)\s+peak ([\d.]+)\s+voices (\d+)/(\d+)\s+arena\s+(\d+) KB", ln)
        if m:
            d[m.group(1).strip()] = {"voices": int(m.group(3)), "of": int(m.group(4)), "arena": int(m.group(5))}
    return d


def main():
    sys.exit("granular_report.py is a helper library now (the granular batch went with Clouds in 2.48): run bank45_report.py")


if __name__ == "__main__":
    main()
