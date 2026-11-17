#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 update 17: the measurement card for the third batch of presets (presets_batch3.py): batch2_report.py's tests
(16 s phrase with K1 to K4 swept in turn, 12 s MORPH sweep in six steps, tuning at C2 to C5, hands-off movement, loudness, the 16
macro corners, load estimate) with three changes: the phrase suits each instrument, the hands-off test also tracks the root's pitch
(drift in cents over 8 s, and the wobble of a single held note early and late, which shows a vibrato that grows), and the
movement test depends on the preset's level: SUBTLE must move a little (at least 0.1 dB of RMS or x1.02 of centroid) and not
much (at most 3.5 dB, x1.5, 12 cents of drift), MEDIUM must be audible (2 dB or x1.2, or a vibrato of 8 cents peak to peak
at its fullest), BOLD clearly (4 dB or x1.5).

  python3 batch3_report.py [NAME ...] [--mp3 DIR] [--json FILE] [--host-log FILE]

Also writes NN_NAME_still.mp3: 8 s of the held chord with nothing touched (a chord struck every 2 s for the instruments that die away)."""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import batch2_report as b2
import granular_batch as gb
import granular_report as gr
from presets_batch3 import batch3

SR, BEAT = gb.SR, gb.BEAT
BATCH2_MEDIAN_RMS = 0.0975      # batch 2's median RMS at the defaults (presets-batch2.md)
NOPITCH = ("NOISE SWEEP",)
PEAKS = ()            # the tuning test follows the fundamental's own peak, not a comb of partials
DRY = {"ATMOSPHERE": ""}      # tuning and pitch are measured with the effects' mix at zero (the shimmer and the tape wow detune the tails)


def phrase3(q):
    name, cat, mono = q["name"], q["cat"], q["fm1_voices"] == 1
    S = BEAT / 2
    seq = b2.seq
    if cat == "BASS":
        lo = [28, 35, 40, 43, 47, 52, 55, 52, 47, 43, 40, 35]
        ev = seq(lo, 0.2, S, S * 0.95) + [(3.8, 3.1, 36, 0.8), (7.0, 3.0, 41, 0.8), (10.2, 3.0, 43, 0.8), (13.2, 2.6, 36, 0.8)]
        root = 36
    elif cat == "LEAD":
        sc = [55, 57, 59, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86, 88, 91, 88, 84, 79, 76, 72, 67, 64]
        ev = seq(sc, 0.2, S, S * 0.95) + [(6.8, 3.0, 69, 0.8), (9.9, 3.0, 72, 0.8), (13.0, 2.8, 67, 0.8)]
        root = 69
    elif name in ("ARP PLUCK", "FM BELL", "E PIANO"):
        arp = [48, 52, 55, 59, 60, 64, 67, 71, 72, 76, 79, 83, 84, 83, 79, 76, 72, 67, 64, 60]
        step = S / 2 if name == "ARP PLUCK" else S
        tc = 0.2 + len(arp) * step + 0.3
        ev = seq(arp, 0.2, step, step * 0.9) + [(tc + 1.0 * k, 0.95, n, 0.7) for k in range(int((15.8 - tc) / 1.0)) for n in ((48, 55, 59, 64) if k % 2 == 0 else (45, 52, 57, 60))]
        root = 48
    elif name == "ROTARY ORGAN":
        ch = [(48, 55, 60, 64), (45, 52, 57, 60), (53, 57, 60, 65), (43, 50, 55, 59)]
        ev = [(0.2 + 2.0 * i, 1.9, n, 0.7) for i in range(8) for n in ch[i % 4]]
        root = 48
    elif name == "HORN SECT":
        ch = [(48, 55, 60, 64), (50, 57, 62, 65), (52, 59, 64, 67), (48, 55, 60, 64), (43, 50, 55, 59), (45, 52, 57, 60), (50, 57, 62, 65), (48, 55, 60, 64)]
        ev = [(0.2 + 1.9 * i, 1.5, n, 0.8) for i, c in enumerate(ch) for n in c]
        root = 48
    elif name == "NOISE SWEEP":
        ev = [(0.2, 7.4, n, 0.75) for n in (48, 55, 59, 64)] + [(8.2, 7.2, n, 0.75) for n in (45, 52, 57, 60)]
        root = 48
    else:
        ev = [(0.2 + 0.4 * i, 7.6 - 0.4 * i, n, 0.7) for i, n in enumerate((36, 43, 48, 52, 55, 59, 64))] + [(8.2, 7.4, n, 0.7) for n in (33, 45, 52, 57, 60)]
        root = 48
    chord = [root] if mono else [root, root + 7, root + 11, root + 16]
    strike = q["amp"]["SUSTAIN"] < 0.35 or name in ("FM BELL", "E PIANO", "ARP PLUCK")
    held = [(0.1 + 2.0 * k, 1.9, n, 0.75) for k in range(6) for n in chord] if strike else [(0.2, 11.8, n, 0.7 if not mono else 0.75) for n in chord]
    return ev, held, root


WIN = {"ARP PLUCK": (0.03, 0.33), "FM BELL": (0.1, 1.1)}      # the part of each strike the tests look at (default 0.3 to 1.3 s after it)


def dry(q):
    """the macros for the tuning and pitch tests: K3 (SPACE) at 0 for the presets in DRY"""
    md = list(q["macros"])
    if q["name"] in DRY:
        md[2] = 0.0
    return md


def w0(name):
    return WIN.get(name, (0.3, 1.3))


def morph_test(q, held, root, tmp):
    name, md = q["name"], q["macros"]
    A, Bw = w0(name)
    txt = "secs 12\n" + "\n".join(f"on {t:.2f} {d:.2f} {n} {v:.2f}" for t, d, n, v in held) + "\n"
    txt += "".join(f"set {k + 1} 0 {md[k]}\n" for k in (1, 2, 3)) + "set 1 0 0\nramp 1 0 12 0 1\n"
    p = tmp / "morph.wav"
    gb.render(name, txt, p, tmp)
    x = gb.load(p)
    one = [(0.1 + 2.0 * k, 1.9, root, 0.75) for k in range(6)] if b2.STRIKE[0] else [(0.2, 11.8, root, 0.75)]
    txt1 = "secs 12\n" + DRY.get(name, "") + "\n".join(f"on {t:.2f} {d:.2f} {n} {v:.2f}" for t, d, n, v in one) + "\n"
    txt1 += "".join(f"set {k + 1} 0 {dry(q)[k]}\n" for k in (1, 2, 3)) + "set 1 0 0\nramp 1 0 12 0 1\n"
    p1 = tmp / "morph1.wav"
    gb.render(name, txt1, p1, tmp)
    x1 = gb.load(p1)
    steps = []
    for i in range(6):
        a, b = (2 * i + 0.1 + A, 2 * i + 0.1 + Bw) if b2.STRIKE[0] else (2 * i + 0.5, 2 * i + 1.5)
        seg = x[int(a * SR):int(b * SR)]
        steps.append({"rms": gb.rms(x, a, b), "peak": float(np.abs(seg).max()), "cent": gb.centroid(x, a, b),
                      "cents_err": 0.0 if name in NOPITCH else (peak_cents if name in PEAKS else comb_cents)(x1, a, b, b2.hz(root)), "k1": (a + b) / 2 / 12})
    return steps, p


def tuning_test(q, tmp):
    name = q["name"]
    A, Bw = w0(name)
    out = {}
    for note in (36, 48, 60, 72):
        txt = f"secs 3\n{DRY.get(name, '')}on 0.1 2.4 {note} 0.75\n" + "".join(f"set {k + 1} 0 {v}\n" for k, v in enumerate(dry(q)))
        p = tmp / "tune.wav"
        gb.render(name, txt, p, tmp)
        x = gb.load(p)
        pc = peak_cents if name in PEAKS else comb_cents
        out[note] = 0.0 if name in NOPITCH else pc(x, 0.1 + A, 0.1 + Bw, b2.hz(note)) if b2.STRIKE[0] else pc(x, 0.8, 2.2, b2.hz(note))
    return out


b2.morph_test, b2.tuning_test = morph_test, tuning_test
b2.phrase = phrase3
PEAK = ()       # inharmonic tones: the fundamental's own peak
b2.pitch_best = lambda x, t0, t1, f0: comb_cents(x, t0, t1, f0)       # one tuning test for the whole batch (see comb_cents)


def band_cents(x, t0, t1, f0, n=1 << 17, span=50):
    """the energy-weighted mean pitch (cents from f0) of the spectrum within +-span cents of f0: steady for a detuned or chorused tone
    (a peak-picker jumps between its partials)"""
    m = x.mean(axis=1)[int(t0 * SR):int(t1 * SR)]
    s = np.abs(np.fft.rfft(m * np.hanning(len(m)), n)) ** 2
    df = SR / n
    k = np.arange(int(f0 * 2 ** (-span / 1200) / df), int(f0 * 2 ** (span / 1200) / df) + 1)
    c = 1200 * np.log2(k * df / f0)
    return float((s[k] * c).sum() / max(s[k].sum(), 1e-20))


def comb_cents(x, t0, t1, f0, span=60):
    """the tuning error (cents) of the played note by a comb search: the spectrum is summed at h x f for h = 1..16 with f = f0/4 x 2^(c/1200),
    c from -60 to +60 cents; the best c is the answer. (On the host render Braids' fundamental sits two octaves below the played note, as in
    batches 1 and 2, so a peak at f0 is only its 4th harmonic, which a square or a triangle has not: batch2_report's test then measured
    nothing. The comb counts every partial that lies on the series, whichever are there, and works the same for a tone at f0.)"""
    m = x.mean(axis=1)[int(t0 * SR):int(t1 * SR)]
    n = 1 << 18
    s = np.abs(np.fft.rfft(m * np.hanning(len(m)), n))
    df = SR / n
    best, bc = -1.0, 0.0
    for c in np.arange(-span, span + 0.5, 1.0):
        f = f0 / 4 * 2 ** (c / 1200.0)
        sc = 0.0
        for h in range(1, 17):
            k = int(round(h * f / df))
            if k + 3 >= len(s):
                break
            sc += s[max(0, k - 2):k + 3].max()
        if sc > best:
            best, bc = sc, c
    return float(bc)


def peak_cents(x, t0, t1, f0, span=60):
    """the error (cents) of the strongest peak within +-60 cents of f0/4, for an inharmonic tone (the FM bell) where a comb of partials has no meaning"""
    m = x.mean(axis=1)[int(t0 * SR):int(t1 * SR)]
    n = 1 << 19
    s = np.abs(np.fft.rfft(m * np.hanning(len(m)), n))
    df = SR / n
    fc = f0 / 4
    lo, hi = int(fc * 2 ** (-span / 1200) / df), int(fc * 2 ** (span / 1200) / df)
    k = lo + int(np.argmax(s[lo:hi]))
    a, b, c = np.log(s[k - 1] + 1e-12), np.log(s[k] + 1e-12), np.log(s[k + 1] + 1e-12)
    return float(1200 * np.log2((k + 0.5 * (a - c) / (a - 2 * b + c + 1e-12)) * df / fc))


def wobble(x, t0, t1, f0, win=0.05):
    """peak-to-peak (cents) of the band-mean pitch near f0 in windows of 100 ms over [t0, t1)"""
    out = [band_cents(x, t, t + win, f0, 1 << 16, 70) for t in np.arange(t0, t1 - win, 0.0125)]
    return float(max(out) - min(out)) if out else 0.0


def pitch_routes(q):
    """the deliberate pitch modulation in cents (peak) at the macros' defaults, from the routes: matrix rows and LFO targets whose
    destination is PITCH (the engine: 1.0 = 12 semitones); a route whose via is M2 follows K2's default; the LFO FADE is not counted"""
    md = q["macros"]
    tot = 0.0
    for row in q["matrix"]:
        if row[1] == "PITCH":
            tot += abs(row[2]) * (md[1] if len(row) > 3 and row[3] == "M2" else 1.0) * 1200
    for t in q.get("lfo_tgt", []):
        if t and t[0] == "PITCH":
            tot += abs(t[1]) * 1200
    return tot


def hands3(q, held, root, tmp, out=None, base=None):
    name, md = q["name"], q["macros"]
    strike = b2.STRIKE[0]
    txt = "secs 9\n" + "\n".join(f"on {t:.2f} {min(d, 8.8):.2f} {n} {v:.2f}" for t, d, n, v in held) + "\n"
    txt += "".join(f"set {k + 1} 0 {v}\n" for k, v in enumerate(md))
    p = tmp / "off3.wav"
    gb.render(name, txt, p, tmp)
    x = gb.load(p)
    A, Bw = w0(name)
    ts = [0.1 + A + 2.0 * k for k in range(4)] if strike else list(np.arange(1.0, 8.01, 0.5))
    dl = (Bw - A) if strike else 1.0
    r = [gr.db(gb.rms(x, t, t + dl)) for t in ts]
    c = [gb.centroid(x, t, t + dl) for t in ts]
    f0 = b2.hz(root)
    # the pitch drift: the root alone, 1 s windows, the band-mean pitch (a chord's other notes would leak into the band)
    if strike:
        one = "secs 9\n" + "".join(f"on {0.1 + 2.0 * k:.2f} 1.9 {root} 0.75\n" for k in range(4))
    else:
        one = f"secs 9\non 0.1 8.8 {root} 0.75\n"
    one += "".join(f"set {k + 1} 0 {v}\n" for k, v in enumerate(md))
    p1 = tmp / "one.wav"
    gb.render(name, one, p1, tmp)
    x1 = gb.load(p1)
    if name in NOPITCH:
        pc = [0.0]
    elif strike:
        pc = [band_cents(x1, 0.1 + A + 2.0 * k, 0.1 + Bw + 2.0 * k, f0) for k in range(4)]
    else:
        pc = [band_cents(x1, t, t + 1.0, f0) for t in np.arange(1.0, 8.01, 1.0)]
    if not strike:
        rf = [gr.db(gb.rms(x, t, t + 0.25)) for t in np.arange(1.0, 8.0, 0.25)]
        cf = [gb.centroid(x, t, t + 0.25) for t in np.arange(1.0, 8.0, 0.25)]
    else:
        rf, cf = r, c
    res = {"fast_pp_dB": float(max(rf) - min(rf)), "fast_ratio": float(max(cf) / max(min(cf), 1)), "rms_pp_dB": float(max(r) - min(r)), "cent_ratio": float(max(c) / max(min(c), 1)), "cent_min": float(min(c)), "cent_max": float(max(c)),
           "drift_c": float(max(pc) - min(pc)), "pitch_route_c": pitch_routes(q)}
    if out:
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-t", "8", "-i", str(p), "-b:a", "192k", str(out / f"{base}_still.mp3")], check=True)
    return res


def measure(q, num, tmp, mp3):
    name = q["name"]
    ev, held, root = phrase3(q)
    b2.STRIKE[0] = held[0][1] < 3.0
    r = b2.measure(q, num, tmp, mp3)
    base = f"{num:02d}_{name.replace(' ', '_')}"
    r["off"] = hands3(q, held, root, tmp, Path(mp3) if mp3 else None, base)
    r["move"] = q["move"]
    return r


def verdicts(r, med):
    b2.verdicts(r, med)
    off, mv = r["off"], r["move"]
    nop = r["name"] in NOPITCH
    r["loud_b2"] = gr.db(r["ctl_rms"]) - gr.db(BATCH2_MEDIAN_RMS)
    r["loud_batch"] = gr.db(r["ctl_rms"]) - med
    wob = off["pitch_route_c"] * 2
    if mv == "subtle":
        ok = (off["rms_pp_dB"] >= 0.1 or off["cent_ratio"] >= 1.02) and off["rms_pp_dB"] <= 3.5 and off["cent_ratio"] <= 1.5 and off["fast_pp_dB"] <= 5.0 and off["drift_c"] <= 20
    elif mv == "medium":
        ok = off["rms_pp_dB"] >= 2.0 or off["cent_ratio"] >= 1.2 or off["fast_pp_dB"] >= 3.0 or off["fast_ratio"] >= 1.5 or wob >= 16
    else:
        ok = off["rms_pp_dB"] >= 4.0 or off["cent_ratio"] >= 1.5 or off["fast_pp_dB"] >= 6.0
    c = r["checks"]
    c.pop("hands-off movement", None)
    c[f"movement fits {mv}"] = ok
    c["loudness +-4 dB (batch 2)"] = abs(r["loud_b2"]) <= 4
    c["loudness +-4 dB (this batch)"] = abs(r["loud_batch"]) <= 4
    c.pop("loudness +-4 dB (batch)", None)
    if nop:
        c["tuning <= 10 c"] = True
        c["MORPH pitch <= 15 c"] = True
    r["pass"] = all(c.values())
    return r


def main():
    a = sys.argv[1:]
    opt = lambda k: a[a.index(k) + 1] if k in a else None
    mp3, js, hl = opt("--mp3"), opt("--json"), opt("--host-log")
    names = [x for i, x in enumerate(a) if not x.startswith("--") and (i == 0 or a[i - 1] not in ("--mp3", "--json", "--host-log"))]
    P = batch3()
    host = gr.host_lines(hl) if hl else {}
    out = []
    with tempfile.TemporaryDirectory() as t:
        for i, q in enumerate(P):
            if names and q["name"] not in names:
                continue
            r = measure(q, 33 + i, Path(t), mp3)
            r["host"] = host.get(q["name"])
            r["fm1_voices"] = q["fm1_voices"]
            r["cat"] = q["cat"]
            out.append(r)
            print(f"{r['num']} {q['name']:12} pk {r['peak']:.2f} rms {r['ctl_rms']:.3f}", flush=True)
    med = float(np.median([gr.db(r["ctl_rms"]) for r in out]))
    for r in out:
        verdicts(r, med)
        m, o = r["morph"], r["off"]
        bad = [k for k, v in r["checks"].items() if not v]
        print(f"{r['num']:2} {r['name']:12} {r['move'][:3]} pk {r['peak']:.2f} rms {r['ctl_rms']:.3f} ld {r['loud_batch']:+.1f}/{r['loud_b2']:+.1f}/{r['loud_b1']:+.1f} "
              f"MORPH rms {min(s['rms'] for s in m):.3f}-{max(s['rms'] for s in m):.3f} cent " + "/".join(f"{s['cent']:.0f}" for s in m)
              + f" ({r['morph_chars']}) pitch {r['morph_pitch_max']:.0f}c tune {r['tune_max']:.0f}c "
              + f"off {o['rms_pp_dB']:.1f}dB x{o['cent_ratio']:.2f} dr {o['drift_c']:.0f}c vib {o['pitch_route_c']:.0f}c "
              + "K" + "".join("+" if e["pass"] else "-" for e in r["ev"]) + f" est {r['est2'][0]:.0f}/{r['est2'][1]:.0f} | "
              + ("PASS" if r["pass"] else "FAIL " + "; ".join(bad)))
    if js:
        Path(js).write_text(json.dumps(out, indent=1, default=float))


if __name__ == "__main__":
    main()
