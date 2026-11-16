#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 update 15: the measurement card for the second batch of presets (presets_batch2.py).

  python3 batch2_report.py [NAME ...] [--mp3 DIR] [--json FILE] [--host-log FILE]

Extends granular_report.py (same render path, same macro, corner and hands-off tests) for instruments:
  phrase: a scale or arpeggio across three octaves, then a held chord (a single held note for the mono presets), 16 s,
          K1 0-4 s, K2 4-8 s, K3 8-12 s, K4 12-16 s (the demo);
  MORPH: a held chord (one note for the mono presets) for 12 s with K1 swept 0 to 1 over the whole 12 s: six 2 s steps
         (the middle 1 s of each): RMS, peak, spectral centroid and the root's pitch error in cents;
  tuning: one note each at C2, C3, C4 and C5 with every macro at its default: the fundamental's error in cents
          (harmonic-sum search, +-60 cents, 0.2 Hz bins);
  hands-off: 8 s of the held chord, nothing moved: 1 s RMS span and centroid ratio (granular_report.hands_off's rule);
  loudness against the batch median and batch 1's median (RMS of the phrase at the defaults);
  estimated load per core (an estimate from the owner's measurements, nothing here ran on an FM-1)."""
import json
import sys
import tempfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import granular_batch as gb
import granular_report as gr
gr.estimate = lambda q: (0.0, 0.0, 0.0)      # granular_report's estimate is for Clouds presets; this tool has its own
from presets_batch2 import batch2

SR = gb.SR
BEAT = gb.BEAT
STRIKE = [False]
BATCH1_MEDIAN_RMS = 0.092       # batch 1's median RMS at the defaults (presets-batch1.md)


def seq(notes, t0, step, dur, vel=0.75):
    return [(t0 + i * step, dur, n, vel) for i, n in enumerate(notes)]


def phrase(q):
    """(events, held events for the hands-off and MORPH tests, the one note for mono presets)"""
    name, mono = q["name"], q["fm1_voices"] == 1
    cat = q["cat"]
    S = BEAT / 2
    if cat == "BASS":
        lo = [28, 35, 40, 43, 47, 52, 55, 52, 47, 43, 40, 35]
        ev = seq(lo, 0.2, S * 1.0, S * 0.95) + [(3.8, 3.1, 36, 0.8), (7.0, 3.0, 41, 0.8), (10.2, 3.0, 43, 0.8), (13.2, 2.6, 36, 0.8)]
        root = 36
    elif cat == "LEAD":
        sc = [55, 57, 59, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86, 88, 91, 88, 84, 79, 76, 72, 67, 64]
        ev = seq(sc, 0.2, S * 1.0, S * 0.95) + [(6.8, 3.0, 69, 0.8), (9.9, 3.0, 72, 0.8), (13.0, 2.8, 67, 0.8)]
        root = 69
    elif name in ("SOFT MALLET", "SYMPATH HARP", "GLASS BELLS", "TINE EP", "TAPE KEYS", "TONEWHEEL"):
        arp = [48, 52, 55, 59, 60, 64, 67, 71, 72, 76, 79, 83, 84, 83, 79, 76, 72, 67, 64, 60]
        ev = seq(arp, 0.2, S, S * 0.9) + [(5.8 + 1.5 * k, 1.4, n, 0.7) for k in range(7) for n in ((48, 55, 59, 64) if k % 2 == 0 else (45, 52, 57, 60))]
        root = 48
    elif name in ("GRID CHOP", "FUNC PULSE"):
        ev = [(0.2, 7.4, n, 0.75) for n in (48, 55, 59, 64)] + [(8.2, 7.2, n, 0.75) for n in (45, 52, 57, 60)]
        root = 48
    elif name == "DIAL CHOIR":
        sc = [48, 52, 55, 60, 64, 67, 72, 76, 79, 84, 79, 76, 72, 67, 64, 60]
        ev = seq(sc, 0.2, S * 1.0, S * 0.95) + [(4.8, 5.4, n, 0.7) for n in (48, 55, 59, 64)] + [(10.4, 5.2, n, 0.7) for n in (45, 52, 57, 60)]
        root = 48
    else:                                      # strings and pads: the chord builds one note at a time, then a second chord
        ev = [(0.2 + 0.4 * i, 7.6 - 0.4 * i, n, 0.7) for i, n in enumerate((36, 43, 48, 52, 55, 59, 64))] + [(8.2, 7.4, n, 0.7) for n in (33, 45, 52, 57, 60)]
        root = 48
    chord = [root] if mono else [root, root + 7, root + 11, root + 16]
    strike = q["amp"]["SUSTAIN"] < 0.35 or name in ("GLASS BELLS", "TINE EP")     # these die away: the test strikes the chord afresh every 2 s
    if strike:
        held = [(0.1 + 2.0 * k, 1.9, n, 0.75) for k in range(6) for n in chord]
    else:
        held = [(0.2, 11.8, n, 0.7 if not mono else 0.75) for n in chord]
    return ev, held, root


def pitch_cents(x, t0, t1, f0, span=60):
    """error (cents) of the fundamental near f0: the best of a +-span cents search of the summed spectrum at h x f, h = 1..4"""
    m = x.mean(axis=1)[int(t0 * SR):int(t1 * SR)]
    n = 1 << 18
    s = np.abs(np.fft.rfft(m * np.hanning(len(m)), n))
    df = SR / n
    best, bc = -1.0, 0.0
    for c in np.arange(-span, span + 0.5, 1.0):
        f = f0 * 2 ** (c / 1200.0)
        sc = 0.0
        for h in (1, 2, 3, 4):
            k = int(round(h * f / df))
            sc += s[max(0, k - 2):k + 3].max() / h
        if sc > best:
            best, bc = sc, c
    return float(bc)


def root_cents(x, t0, t1, f0):
    """the root's own peak: parabolic peak of the spectrum within +-50 cents of f0"""
    m = x.mean(axis=1)[int(t0 * SR):int(t1 * SR)]
    n = 1 << 18
    s = np.abs(np.fft.rfft(m * np.hanning(len(m)), n))
    df = SR / n
    lo, hi = int(f0 * 2 ** (-50 / 1200) / df), int(f0 * 2 ** (50 / 1200) / df)
    k = lo + int(np.argmax(s[lo:hi]))
    a, b, c = np.log(s[k - 1] + 1e-12), np.log(s[k] + 1e-12), np.log(s[k + 1] + 1e-12)
    d = 0.5 * (a - c) / (a - 2 * b + c + 1e-12)
    f = (k + d) * df
    return float(1200 * np.log2(f / f0))


def pitch_best(x, t0, t1, f0):
    """the fundamental's error in cents: its own spectral peak (+-50 cents) when that is a real peak (at least a quarter of the
    spectrum's highest), else the harmonic-sum search (a tone whose fundamental is weak)"""
    m = x.mean(axis=1)[int(t0 * SR):int(t1 * SR)]
    n = 1 << 18
    s = np.abs(np.fft.rfft(m * np.hanning(len(m)), n))
    df = SR / n
    lo, hi = int(f0 * 2 ** (-50 / 1200) / df), int(f0 * 2 ** (50 / 1200) / df)
    if s[lo:hi].max() >= 0.25 * s.max():
        return root_cents(x, t0, t1, f0)
    return pitch_cents(x, t0, t1, f0)


def hz(note):
    return 440.0 * 2 ** ((note - 69) / 12.0)


def morph_test(q, held, root, tmp):
    name, md = q["name"], q["macros"]
    txt = "secs 12\n" + "\n".join(f"on {t:.2f} {d:.2f} {n} {v:.2f}" for t, d, n, v in held) + "\n"
    txt += "".join(f"set {k + 1} 0 {md[k]}\n" for k in (1, 2, 3)) + f"set 1 0 0\nramp 1 0 12 0 1\n"
    p = tmp / "morph.wav"
    gb.render(name, txt, p, tmp)
    x = gb.load(p)
    steps = []
    for i in range(6):
        a, b = (2 * i + 0.3, 2 * i + 1.3) if STRIKE[0] else (2 * i + 0.5, 2 * i + 1.5)
        seg = x[int(a * SR):int(b * SR)]
        steps.append({"rms": gb.rms(x, a, b), "peak": float(np.abs(seg).max()), "cent": gb.centroid(x, a, b),
                      "cents_err": pitch_best(x, a, b, hz(root)), "k1": (a + b) / 2 / 12})
    return steps, p


def tuning_test(q, tmp):
    name = q["name"]
    out = {}
    for note in (36, 48, 60, 72):
        txt = f"secs 3\non 0.2 2.4 {note} 0.75\n" + "".join(f"set {k + 1} 0 {v}\n" for k, v in enumerate(q["macros"]))
        p = tmp / "tune.wav"
        gb.render(name, txt, p, tmp)
        x = gb.load(p)
        out[note] = pitch_best(x, 0.35, 1.35, hz(note)) if STRIKE[0] else pitch_best(x, 0.8, 2.2, hz(note))
    return out


def hands_off2(q, held, tmp):
    name, md = q["name"], q["macros"]
    txt = "secs 9\n" + "\n".join(f"on {t:.2f} {min(d, 8.8):.2f} {n} {v:.2f}" for t, d, n, v in held) + "\n"
    txt += "".join(f"set {k + 1} 0 {v}\n" for k, v in enumerate(md))
    p = tmp / "off2.wav"
    gb.render(name, txt, p, tmp)
    x = gb.load(p)
    ts = [0.3 + 2.0 * k for k in range(4)] if STRIKE[0] else list(np.arange(1.0, 8.01, 0.5))     # struck: the same 1 s after each of four strikes
    r = [gr.db(gb.rms(x, t, t + 1.0)) for t in ts]
    c = [gb.centroid(x, t, t + 1.0) for t in ts]
    return {"rms_pp_dB": float(max(r) - min(r)), "cent_ratio": float(max(c) / max(min(c), 1)), "cent_min": float(min(c)), "cent_max": float(max(c))}


def estimate(q):
    """(core one % at defaults, core two % at defaults, core one worst case, core two worst case): the owner's figures, an estimate.
    Without Clouds both cores share the voices (six voices cost core one about 22: half of 8 + 6 per voice); with Clouds the
    voices stay on core one (8 + 6 per voice) and core two carries Clouds (16.5 at DENSITY 0.7, 29 at 0.9, 42 at 1.0, plus 22 resampling)."""
    sl, v = q["slots"], q["fm1_voices"]
    vc = 8.0 + 6.0 * v
    fx = {"REVERB": 14.0, "DELAY": 6.6, "CHORUS": 5.0}
    extra = sum(fx.get(sl.get(k, ""), 0.0) for k in ("FX1", "FX2"))
    extra += 4.0 if sl.get("SHP") == "FILTER" else 22.0 if sl.get("SHP") == "RINGS" else 0.0
    extra += sum(3.5 for k in ("MODA", "MODB") if sl.get(k) in ("TIDES", "PEAKS"))
    if sl.get("FX1") == "CLOUDS":
        dmax = q["p"]["FX1"]["DENSITY"]
        c2 = float(np.interp(dmax, [0.7, 0.9, 1.0], [16.5, 29.0, 42.0])) + 22.0
        return vc + extra, c2, vc + extra, c2
    one = 0.5 * vc + extra
    two = 0.5 * vc
    return one, two, one + 3.0, two + 3.0           # the worst case adds 3 points for the routes at full depth (my allowance)


def measure(q, num, tmp, mp3):
    name = q["name"]
    ev, held, root = phrase(q)
    STRIKE[0] = held[0][1] < 3.0
    kind = "b2_" + name
    gb.PHRASE[kind] = ev
    gr.KINDS[name] = kind
    gb.NUMBER[name] = num
    r = gr.measure(q, num, tmp, None)
    # the demo (the phrase sweep) as MP3 at the bank position, and the MORPH render
    steps, mp = morph_test(q, held, root, tmp)
    r["morph"] = steps
    r["tuning"] = tuning_test(q, tmp)
    r["off"] = hands_off2(q, held, tmp)
    r["est2"] = estimate(q)
    r["root"] = root
    if mp3:
        import subprocess
        out = Path(mp3)
        out.mkdir(parents=True, exist_ok=True)
        base = f"{num:02d}_{name.replace(' ', '_')}"
        sp = tmp / "demo.wav"
        gb.render(name, gb.script(ev, q["macros"]), sp, tmp)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(sp), "-b:a", "192k", str(out / f"{base}.mp3")], check=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(mp), "-b:a", "192k", str(out / f"{base}_morph.mp3")], check=True)
    return r


def verdicts(r, med):
    m = r["morph"]
    rms = [s["rms"] for s in m]
    db = [20 * np.log10(max(x, 1e-9)) for x in rms]
    cents = [s["cent"] for s in m]
    r["morph_ok_level"] = min(rms) >= 0.01 and max(s["peak"] for s in m) <= 0.9 and (max(db) - min(db)) <= 12.0
    # at least three clearly different characters: greedy grouping of the centroids, a new group when 1.25x apart (or more)
    groups = []
    for c in cents:
        for g in groups:
            if max(c, g, 1.0) / max(min(c, g), 1.0) < 1.25:
                break
        else:
            groups.append(c)
    r["morph_chars"] = len(groups)
    r["morph_pitch_max"] = max(abs(s["cents_err"]) for s in m)
    r["tune_max"] = max(abs(v) for v in r["tuning"].values())
    r["move_ok"] = r["off"]["rms_pp_dB"] >= 2.0 or r["off"]["cent_ratio"] >= 1.2
    r["loud_b2"] = gr.db(r["ctl_rms"]) - med
    r["loud_b1"] = gr.db(r["ctl_rms"]) - gr.db(BATCH1_MEDIAN_RMS)
    c = r["corners"]
    r["safe"] = c["peak"] < 0.9 and c["quiet"] >= 0.004 and not (c["grow"] > 2.5 and c["peak"] > 0.5)
    r["checks"] = {
        "peak 0.2-0.9": 0.2 < r["peak"] < 0.9, "RMS per 4 s >= 0.01": min(r["rms4"]) >= 0.01,
        "MORPH level": r["morph_ok_level"], "MORPH >= 3 characters": r["morph_chars"] >= 3, "MORPH pitch <= 15 c": r["morph_pitch_max"] <= 15,
        "tuning <= 10 c": r["tune_max"] <= 10, "hands-off movement": r["move_ok"], "loudness +-4 dB (batch)": abs(r["loud_b2"]) <= 4,
        "loudness +-4 dB (batch 1)": abs(r["loud_b1"]) <= 4, "K2-K4 audible": all(e["pass"] for e in r["ev"][1:]),
        "corners safe": r["safe"], "4+ voices fit or mono by design": (not r["host"]) or r["host"]["voices"] >= 4 or r["fm1_voices"] == 1,
    }
    r["pass"] = all(r["checks"].values())
    return r


def main():
    a = sys.argv[1:]
    opt = lambda k: a[a.index(k) + 1] if k in a else None
    mp3, js, hl = opt("--mp3"), opt("--json"), opt("--host-log")
    names = [x for i, x in enumerate(a) if not x.startswith("--") and (i == 0 or a[i - 1] not in ("--mp3", "--json", "--host-log"))]
    P = batch2()
    host = gr.host_lines(hl) if hl else {}
    out = []
    with tempfile.TemporaryDirectory() as t:
        for i, q in enumerate(P):
            if names and q["name"] not in names:
                continue
            r = measure(q, 17 + i, Path(t), mp3)
            r["host"] = host.get(q["name"])
            r["fm1_voices"] = q["fm1_voices"]
            r["cat"] = q["cat"]
            out.append(r)
            print(f"{r['num']} {q['name']:12} pk {r['peak']:.2f} rms {r['ctl_rms']:.3f}", flush=True)
    med = float(np.median([gr.db(r["ctl_rms"]) for r in out]))
    for r in out:
        verdicts(r, med)
        m = r["morph"]
        bad = [k for k, v in r["checks"].items() if not v]
        print(f"{r['num']:2} {r['name']:12} pk {r['peak']:.2f} rms {r['ctl_rms']:.3f} ld {r['loud_b2']:+.1f}/{r['loud_b1']:+.1f} "
              f"MORPH rms {min(s['rms'] for s in m):.3f}-{max(s['rms'] for s in m):.3f} cent " + "/".join(f"{s['cent']:.0f}" for s in m)
              + f" ({r['morph_chars']}) pitch {r['morph_pitch_max']:.0f}c tune {r['tune_max']:.0f}c " + str({k: round(v) for k, v in r['tuning'].items()})
              + f" off {r['off']['rms_pp_dB']:.1f}dB x{r['off']['cent_ratio']:.2f} "
              + "K" + "".join("+" if e["pass"] else "-" for e in r["ev"]) + f" est {r['est2'][0]:.0f}/{r['est2'][1]:.0f} | "
              + ("PASS" if r["pass"] else "FAIL " + "; ".join(bad)))
    if js:
        Path(js).write_text(json.dumps(out, indent=1, default=float))


if __name__ == "__main__":
    main()
