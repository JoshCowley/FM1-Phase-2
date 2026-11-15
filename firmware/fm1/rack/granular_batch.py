#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 update 11: measures the granular batch (presets_granular.py) through the firmware's own sound path.

  python3 granular_batch.py [NAME ...] [--mp3 DIR] [--json FILE]

For each preset: a 16 s demo (render_preset: rc_load, rc_render, 44.1 kHz) of a phrase that suits it, K1 swept 0 to 1 in
0-4 s, K2 in 4-8 s, K3 in 8-12 s, K4 in 12-16 s, the other macros at the preset's own settings; the same phrase with no
macro moved (the control); and a held chord with every macro still (the movement figure). Measures (a computer's
numbers, nothing heard): peak, RMS per 4 s segment, the spectral centroid at the start and end of each sweep (0.5 s
windows) and the ratio, the same ratio over the control (so the phrase's own change is not credited to the knob), the mean
change of the log spectrum (dB) between the same two windows, and the movement of the still chord (std of the 250 ms RMS
in dB, and std/mean of the centroid, over 8 s)."""
import json
import subprocess
import sys
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
RENDER = HERE / "build" / "host" / "render_preset"
SR = 44100
BEAT = 60.0 / 112.0           # the factory tempo: 112 BPM

CH_A = [48, 55, 59, 64]       # Em9-ish, held
CH_B = [45, 52, 57, 60]       # Am7-ish
CH_C = [43, 50, 55, 62]


def chords(a=CH_A, b=CH_B, rel=15.6, vel=0.7, split=7.8):
    ev = [(0.2, split - 0.2, n, vel) for n in a] + [(split + 0.4, rel - split - 0.4, n, vel) for n in b]
    return ev


def stabs(notes, grid, pat, bars=8, vel=0.8, dur=0.12):
    """pat: sixteenth-note steps of a bar that play (all notes of the chord); grid: a sixteenth in seconds"""
    ev = []
    for bar in range(bars):
        for st in pat:
            t = (bar * 16 + st) * grid
            if t < 15.8:
                ev += [(t, dur, n, vel * (1.0 if st % 4 == 0 else 0.7)) for n in notes]
    return ev


S16 = BEAT / 4
PHRASE = {
    "pad": chords(),
    "freeze": [(0.2, 7.6, n, 0.7) for n in CH_A] + [(8.4, 7.0, n, 0.7) for n in CH_B],      # released at 15.4: the freeze (K4 past 0.67, about 14.7 s) holds the chord
    "stab": stabs([48, 55, 59], S16, [0, 3, 6, 8, 10, 12, 14]) + stabs([64], S16, [2, 7, 11, 15], vel=0.6),
    "pluck": [(t, 0.15, n, 0.8) for t, n in [(0.2 + i * BEAT / 2, [60, 64, 67, 71, 72, 71, 67, 64][i % 8]) for i in range(60)]],
    "bell": [(0.2 + i * BEAT * 1.5, 0.2, n, 0.8) for i, n in enumerate([72, 67, 64, 71, 74, 69, 65, 72, 76, 71] * 2)],
    "bass": [(0.2, 7.4, 36, 0.8), (0.2, 7.4, 43, 0.7), (8.4, 7.0, 34, 0.8), (8.4, 7.0, 41, 0.7)],
    "low": [(0.2, 7.4, 36, 0.8), (0.2, 7.4, 48, 0.6), (0.2, 7.4, 55, 0.6), (8.4, 7.0, 34, 0.8), (8.4, 7.0, 46, 0.6), (8.4, 7.0, 53, 0.6)],
    "swell": [(0.2, 3.4, n, 0.7) for n in CH_A] + [(4.2, 3.4, n, 0.7) for n in CH_B] + [(8.2, 3.4, n, 0.7) for n in CH_A] + [(12.2, 3.4, n, 0.7) for n in CH_B],
    "single": [(0.2, 7.4, 60, 0.8), (8.4, 7.0, 55, 0.8)],
    "notes": [(0.2, 3.4, 57, 0.7), (4.0, 3.6, 60, 0.7), (8.4, 3.4, 64, 0.7), (12.2, 3.4, 62, 0.7)],
}
KIND = {}      # preset name -> a PHRASE key (default "pad")


def script(ev, md, sweeps=True, secs=16, voices=None, fix=False):
    out = [f"secs {secs}"]
    if voices:
        out.append(f"voices {voices}")
    out += [f"on {t:.3f} {d:.3f} {n} {v:.2f}" for t, d, n, v in ev]
    if fix:
        out += [f"set {k + 1} 0 {md[k]}" for k in range(4)]
    if sweeps:
        for k in range(4):
            t0, t1 = 4.0 * k, 4.0 * k + 4.0
            out += [f"set {k + 1} 0 {md[k]}", f"ramp {k + 1} {t0:.2f} {t1:.2f} 0 1", f"set {k + 1} {t1 + 0.02:.2f} {md[k]}"]
    return "\n".join(out) + "\n"


def render(name, text, path, tmp):
    sp = tmp / "s.txt"
    sp.write_text(text)
    r = subprocess.run([str(RENDER), name.replace(" ", "_"), str(sp), str(path)], capture_output=True, text=True, check=True)
    return r.stdout.strip()


def load(path):
    from scipy.io import wavfile
    sr, x = wavfile.read(path)
    return x.astype(np.float64)


def spec(x, t0, t1, n=4096):
    """mean magnitude spectrum of the window [t0, t1) (mono: the two channels' mean), Hann, hops of n/2"""
    m = x.mean(axis=1)[int(t0 * SR):int(t1 * SR)]
    w = np.hanning(n)
    fr = [np.abs(np.fft.rfft(m[i:i + n] * w)) for i in range(0, max(1, len(m) - n), n // 2)]
    return np.mean(fr, axis=0) if fr else np.zeros(n // 2 + 1)


def centroid(x, t0, t1):
    s = spec(x, t0, t1)
    f = np.fft.rfftfreq(4096, 1.0 / SR)
    return float((s * f).sum() / max(s.sum(), 1e-12))


def logdiff(x, a0, a1, b0, b1):
    sa, sb = spec(x, a0, a1), spec(x, b0, b1)
    f = np.fft.rfftfreq(4096, 1.0 / SR)
    m = (f > 60) & (f < 12000)
    la, lb = 20 * np.log10(sa[m] + 1e-7), 20 * np.log10(sb[m] + 1e-7)
    return float(np.mean(np.abs(la - lb)))


def feats(x, t0, t1):
    """centroid (Hz), RMS (dB), AM depth (std of 20 ms RMS in dB), flatness (dB, 100 Hz-8 kHz), side/mid (dB)"""
    s = x[int(t0 * SR):int(t1 * SR)]
    m = s.mean(axis=1)
    sd = 0.5 * (s[:, 0] - s[:, 1])
    f = int(0.02 * SR)
    fr = [20 * np.log10(np.sqrt(np.mean(m[i:i + f] ** 2)) + 1e-6) for i in range(0, len(m) - f, f // 2)]
    sp = spec(x, t0, t1)
    fq = np.fft.rfftfreq(4096, 1.0 / SR)
    b = sp[(fq > 100) & (fq < 8000)] ** 2 + 1e-12
    return {"cent": centroid(x, t0, t1), "rms_dB": 20 * np.log10(np.sqrt(np.mean(0.5 * (s[:, 0] ** 2 + s[:, 1] ** 2))) + 1e-6),
            "am": float(np.std(fr)), "flat": float(10 * np.log10(np.exp(np.mean(np.log(b))) / np.mean(b))),
            "side": float(10 * np.log10((np.mean(sd ** 2) + 1e-12) / (np.mean(m ** 2) + 1e-12)))}


def motion(x, t0, t1):
    """how much the sound moves over [t0, t1): std of the 100 ms RMS (dB) plus 20 x the centroid's std/mean over the same frames"""
    h = int(0.1 * SR)
    m = x[int(t0 * SR):int(t1 * SR)]
    db = [10 * np.log10(np.mean(0.5 * (m[i:i + h, 0] ** 2 + m[i:i + h, 1] ** 2)) + 1e-12) for i in range(0, len(m) - h, h)]
    cs = [centroid(x, t0 + i * 0.1, t0 + i * 0.1 + 0.1) for i in range(len(db))]
    return float(np.std(db) + 20 * np.std(cs) / (np.mean(cs) + 1e-9))


def rms(x, t0, t1):
    s = x[int(t0 * SR):int(t1 * SR)]
    return float(np.sqrt(np.mean(0.5 * (s[:, 0] ** 2 + s[:, 1] ** 2))))


def bands(x, t0, t1):
    """1/3-octave band levels (dB) of [t0, t1), 100 Hz to 10 kHz"""
    sp = spec(x, t0, t1, 8192) ** 2
    f = np.fft.rfftfreq(8192, 1.0 / SR)
    edges = 100 * 2 ** (np.arange(0, 20) / 3.0)
    return np.array([10 * np.log10(sp[(f >= lo) & (f < hi)].sum() + 1e-12) for lo, hi in zip(edges[:-1], edges[1:]) if hi < 10000])


def envelope(x, t0, t1):
    h = int(0.1 * SR)
    m = x[int(t0 * SR):int(t1 * SR)]
    return np.array([10 * np.log10(np.mean(0.5 * (m[i:i + h, 0] ** 2 + m[i:i + h, 1] ** 2)) + 1e-12) for i in range(0, len(m) - h, h)])


def ab(name, md, k, ev, tmp, t0=2.0, t1=10.0):
    """macro k at 0 and at 1 (the others at the preset's own settings) over the same phrase: what the knob does at its ends"""
    out = []
    for v in (0.0, 1.0):
        m = list(md)
        m[k] = v
        p = tmp / f"ab{k}_{int(v)}.wav"
        render(name, script(ev, m, sweeps=False, secs=int(t1) + 1, fix=True), p, tmp)
        out.append(load(p))
    a, b = out
    fa, fb = feats(a, t0, t1), feats(b, t0, t1)
    ea, eb = envelope(a, t0, t1), envelope(b, t0, t1)
    ma, mb = motion(a, t0, t0 + 4), motion(b, t0, t0 + 4)
    corr = float(np.corrcoef(ea, eb)[0, 1]) if np.std(ea) > 1e-6 and np.std(eb) > 1e-6 else 1.0
    r = {"band": float(np.mean(np.abs(bands(a, t0, t1) - bands(b, t0, t1)))), "level": fb["rms_dB"] - fa["rms_dB"],
         "cent": fb["cent"] / fa["cent"], "c0": fa["cent"], "c1": fb["cent"], "am": (fb["am"] + 0.3) / (fa["am"] + 0.3),
         "side": fb["side"] - fa["side"], "flat": fb["flat"] - fa["flat"], "envcorr": corr, "motion": (mb + 0.5) / (ma + 0.5),
         "rms0": fa["rms_dB"], "rms1": fb["rms_dB"]}
    hits = []
    if r["band"] >= 3.0: hits.append("balance")
    if abs(r["level"]) >= 3.0: hits.append("level")
    if r["am"] > 1.4 or r["am"] < 0.7: hits.append("grain")
    if abs(r["side"]) >= 3.0: hits.append("width")
    if r["envcorr"] <= 0.7: hits.append("rhythm")
    if k == 1 and (r["motion"] > 1.5 or r["motion"] < 0.67): hits.append("motion")
    if min(fa["rms_dB"], fb["rms_dB"]) < -50: hits.append("SILENT")
    if abs(r["level"]) > 9: hits.append("LEVEL JUMP")
    r["hits"] = hits
    return r


def corners(name, md, ev, tmp):
    """all 16 corners of the four macros (each at 0 or 1), 14 s of the phrase: the loudest peak, the quietest 2 s RMS after the
    first second, and whether the last 4 s peak runs away from the first 4 s (feedback)"""
    worst, quiet, grow, wc, qat = 0.0, 9.0, 0.0, None, None
    for c in range(16):
        m = [float((c >> i) & 1) for i in range(4)]
        p = tmp / "corner.wav"
        render(name, script(ev, m, sweeps=False, secs=14, fix=True), p, tmp)
        x = load(p)
        pk = float(np.abs(x).max())
        if pk > worst:
            worst, wc = pk, m
        qq = min(rms(x, t, t + 2) for t in range(1, 13, 2))
        if qq < quiet:
            quiet, qat = qq, m
        a, b = float(np.abs(x[int(1 * SR):int(5 * SR)]).max()), float(np.abs(x[int(10 * SR):int(14 * SR)]).max())
        grow = max(grow, b / max(a, 1e-6))
    return {"peak": worst, "at": wc, "quiet": quiet, "qat": qat, "grow": grow}


def measure(name, md, kind, tmp, mp3=None):
    ev = PHRASE[kind]
    res = {"name": name, "kind": kind}
    w1, w2, w3 = tmp / "sweep.wav", tmp / "ctl.wav", tmp / "still.wav"
    res["render"] = render(name, script(ev, md), w1, tmp)
    render(name, script(ev, md, sweeps=False), w2, tmp)
    held = [(0.2, 9.8, n, 0.7) for n in CH_A] if kind in ("pad", "freeze", "low", "bass", "single") else ev
    render(name, script(held, md, sweeps=False, secs=10), w3, tmp)
    x, c, s = load(w1), load(w2), load(w3)
    res["ab"] = [ab(name, md, k, ev, tmp) for k in range(4)]
    res["corners"] = corners(name, md, ev, tmp)
    res["peak"] = float(np.abs(x).max())
    res["rms4"] = [rms(x, 4 * i, 4 * i + 4) for i in range(4)]
    res["macros"] = []
    for k in range(4):
        t0, t1 = 4.0 * k, 4.0 * k + 4.0
        a0, a1, b0, b1 = t0 + 0.05, t0 + 0.75, t1 - 0.75, t1 - 0.05
        ca, cb = centroid(x, a0, a1) + 1e-9, centroid(x, b0, b1)
        ka, kb = centroid(c, a0, a1) + 1e-9, centroid(c, b0, b1)
        fa, fb, ga, gb = feats(x, a0, a1), feats(x, b0, b1), feats(c, a0, a1), feats(c, b0, b1)
        mo0, mo1, mc0, mc1 = motion(x, t0, t0 + 1.5), motion(x, t1 - 1.5, t1), motion(c, t0, t0 + 1.5), motion(c, t1 - 1.5, t1)
        res["macros"].append({"mo": (mo1 + 0.5) / (mo0 + 0.5) / ((mc1 + 0.5) / (mc0 + 0.5)), "f0": fa, "f1": fb, "g0": ga, "g1": gb, "c0": ca, "c1": cb, "ratio": cb / ca, "ctl": kb / ka, "dB": logdiff(x, a0, a1, b0, b1),
                              "dBctl": logdiff(c, a0, a1, b0, b1),
                              "rms0": rms(x, a0, a1), "rms1": rms(x, b0, b1)})
    hop = int(0.25 * SR)
    seg = s[int(1.5 * SR):int(9.5 * SR)]
    db = [20 * np.log10(np.sqrt(np.mean(0.5 * (seg[i:i + hop, 0] ** 2 + seg[i:i + hop, 1] ** 2))) + 1e-9) for i in range(0, len(seg) - hop, hop)]
    cs = [centroid(s, 1.5 + i * 0.25, 1.5 + i * 0.25 + 0.25) for i in range(32)]
    res["move_dB"] = float(np.std(db))
    res["move_cent"] = float(np.std(cs) / np.mean(cs))
    res["still_rms"] = rms(s, 1.5, 9.5)
    if mp3:
        out = Path(mp3)
        out.mkdir(parents=True, exist_ok=True)
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(w1), "-b:a", "192k", str(out / f"{mp3_name(name)}.mp3")], check=True)
    return res


NUMBER = {}


def mp3_name(name):
    return f"{NUMBER.get(name, 0):02d}_{name.replace(' ', '_')}"


def verdict(d):
    """the knob's own change (the control's change over the same seconds taken out), against thresholds that are mine"""
    f0, f1, g0, g1 = d["f0"], d["f1"], d["g0"], d["g1"]
    ch = {"centroid x": (f1["cent"] / f0["cent"]) / (g1["cent"] / max(g0["cent"], 1e-9)),
          "level dB": (f1["rms_dB"] - f0["rms_dB"]) - (g1["rms_dB"] - g0["rms_dB"]),
          "grain AM x": (f1["am"] + 0.3) / (f0["am"] + 0.3) / ((g1["am"] + 0.3) / (g0["am"] + 0.3)),
          "noisiness dB": (f1["flat"] - f0["flat"]) - (g1["flat"] - g0["flat"]),
          "width dB": (f1["side"] - f0["side"]) - (g1["side"] - g0["side"]),
          "spectrum dB": d["dB"] - d["dBctl"], "motion x": d["mo"]}
    hits = [k for k, v in ch.items() if (k.endswith(" x") and (v > 1.25 or v < 0.8)) or (k.endswith("dB") and (abs(v) >= (3.0 if k != "noisiness dB" else 2.0)))]
    if abs(ch["level dB"]) > 8:
        hits.append("LEVEL JUMP")
    return ch, hits


def show(r):
    m = r["macros"]
    print(f"{r['name']:12} peak {r['peak']:.3f} rms4 " + " ".join(f"{v:.3f}" for v in r["rms4"]) + f"  move {r['move_dB']:.1f} dB {r['move_cent']:.2f}")
    c = r["corners"]
    print(f"   corners (16): peak {c['peak']:.3f} at K={c['at']}  quietest 2 s RMS {c['quiet']:.3f}  last/first 4 s peak x{c['grow']:.2f}")
    for k, d in enumerate(m):
        A = r["ab"][k]
        print(f"   K{k + 1} " + ("OK  " if A["hits"] else "WEAK") + f" ends: balance {A['band']:.1f} dB level {A['level']:+.1f} grain x{A['am']:.2f} width {A['side']:+.1f} rhythm r={A['envcorr']:.2f} motion x{A['motion']:.2f} centroid {A['c0']:.0f}->{A['c1']:.0f}  " + ",".join(A["hits"]))
        ch, hits = verdict(d)
        if False: print(f"   K{k + 1} " + ("OK  " if hits else "WEAK") + " " + " ".join(f"{n.split()[0]} {v:+.1f}" if n.endswith("dB") else f"{n.split()[0]} x{v:.2f}" for n, v in ch.items()) + "  " + ",".join(h.split()[0] for h in hits))
        if False: print(f"   K{k + 1} cent {d['c0']:6.0f} -> {d['c1']:6.0f} x{d['ratio']:.2f} (ctl x{d['ctl']:.2f})  spec {d['dB']:4.1f} dB (ctl {d['dBctl']:.1f})  rms {d['rms0']:.3f} -> {d['rms1']:.3f}")


def main():
    args = sys.argv[1:]
    mp3 = args[args.index("--mp3") + 1] if "--mp3" in args else None
    js = args[args.index("--json") + 1] if "--json" in args else None
    names = [a for i, a in enumerate(args) if not a.startswith("--") and (i == 0 or args[i - 1] not in ("--mp3", "--json"))]
    sys.path.insert(0, str(HERE))
    from presets_granular import granular, KINDS
    KIND.update(KINDS)
    P = granular()
    for i, q in enumerate(P):
        NUMBER[q["name"]] = i + 1
    import tempfile
    out = []
    with tempfile.TemporaryDirectory() as t:
        for q in P:
            if names and q["name"] not in names:
                continue
            r = measure(q["name"], q["macros"], KIND.get(q["name"], "pad"), Path(t), mp3)
            show(r)
            out.append(r)
    if js:
        Path(js).write_text(json.dumps(out, indent=1))


if __name__ == "__main__":
    main()
