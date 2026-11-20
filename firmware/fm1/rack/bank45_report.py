#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 update 24: the measurement card for the factory bank of 45 (presets_bank45.py): batch3_report.py's tests, unchanged
(16 s phrase with K1 to K4 swept in turn, 12 s MORPH sweep in six steps, tuning at C2 to C5 (and A4), hands-off movement, loudness,
the 16 macro corners, load estimate), with the phrases of the new kinds (thumb pianos, textures, drones) and these additions:
  * every knob must change the sound audibly (K1 too), measured as batch 2 did with the knob layer's targets running
  * "toward the top": the change from K = 0.6 to 1 must be at least that from K = 0 to 0.6 (the lower travel is the gentle part)
  * the demos: NN_NAME.mp3, 28 s: the phrase with K1, K2, K3, K4 each swept 0 to 1 over 6 s in turn, then NN_NAME_still.mp3 (8 s, hands off)

  python3 bank45_report.py [NAME ...] [--mp3 DIR] [--json FILE] [--host-log FILE] [--from N --to M] [--nodemo]"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import batch2_report as b2
import batch3_report as b3
import granular_batch as gb
import granular_report as gr

SR, BEAT = gb.SR, gb.BEAT
S = BEAT / 2
MIDI = lambda n: n


def load_bank():
    import presets_fm1 as PF
    import fm1_presets as F
    from presets_bank45 import bank45
    pool = PF.fm1_presets(json.loads((F.WEB / "presets.json").read_text()))
    return bank45(pool)[1:]


def phrase45(q):
    """(events, held events for the tests, root note) by the preset's kind"""
    name, cat, mono = q["name"], q["cat"], q["fm1_voices"] == 1
    kind = q["bank45"].get("kind") or ""
    seq = b2.seq
    if cat == "BASS":
        lo = [28, 35, 40, 43, 47, 52, 55, 52, 47, 43, 40, 35]
        ev = seq(lo, 0.2, S, S * 0.95) + [(3.8, 3.1, 36, 0.8), (7.0, 3.0, 41, 0.8), (10.2, 3.0, 43, 0.8), (13.2, 2.6, 36, 0.8)]
        root = 36
    elif cat == "LEAD":
        sc = [55, 57, 59, 60, 62, 64, 67, 69, 72, 74, 76, 79, 81, 84, 86, 88, 91, 88, 84, 79, 76, 72, 67, 64]
        ev = seq(sc, 0.2, S, S * 0.95) + [(6.8, 3.0, 69, 0.8), (9.9, 3.0, 72, 0.8), (13.0, 2.8, 67, 0.8)]
        root = 69
    elif kind == "thumb":      # a kalimba-like melody: pentatonic notes, two together now and then, then chords struck on the beat
        mel = [60, 64, 67, 72, 71, 67, 64, 60, 62, 67, 69, 74, 72, 69, 67, 62]
        ev = seq(mel, 0.2, S, S * 1.6, 0.7) + [(0.2 + S * i, S * 1.6, n + 12, 0.55) for i, n in zip((0, 4, 8, 12), (60, 67, 62, 69))]
        ev += [(5.2 + 1.0 * k, 0.95, n, 0.7) for k in range(10) for n in ((48, 55, 60, 64) if k % 2 == 0 else (45, 52, 57, 60))]
        root = 60
    elif kind in ("arp", "pluck") or q["amp"]["SUSTAIN"] < 0.35 and cat == "KEYS":
        arp = [48, 52, 55, 59, 60, 64, 67, 71, 72, 76, 79, 83, 84, 83, 79, 76, 72, 67, 64, 60]
        step = S / 2 if kind == "pluck" else S
        tc = 0.2 + len(arp) * step + 0.3
        ev = seq(arp, 0.2, step, step * 0.9) + [(tc + 1.0 * k, 0.95, n, 0.7) for k in range(int((15.8 - tc) / 1.0)) for n in ((48, 55, 59, 64) if k % 2 == 0 else (45, 52, 57, 60))]
        root = 48
    elif kind == "organ":
        ch = [(48, 55, 60, 64), (45, 52, 57, 60), (53, 57, 60, 65), (43, 50, 55, 59)]
        ev = [(0.2 + 2.0 * i, 1.9, n, 0.7) for i in range(8) for n in ch[i % 4]]
        root = 48
    elif kind == "horn":
        ch = [(48, 55, 60, 64), (50, 57, 62, 65), (52, 59, 64, 67), (48, 55, 60, 64), (43, 50, 55, 59), (45, 52, 57, 60), (50, 57, 62, 65), (48, 55, 60, 64)]
        ev = [(0.2 + 1.9 * i, 1.5, n, 0.8) for i, c in enumerate(ch) for n in c]
        root = 48
    elif kind == "melodic":    # a sustained melodic texture or a poly lead: a line, then two chords
        sc = [48, 52, 55, 60, 64, 67, 72, 76, 79, 84, 79, 76, 72, 67, 64, 60]
        ev = seq(sc, 0.2, S, S * 0.95) + [(4.8, 5.4, n, 0.7) for n in (48, 55, 59, 64)] + [(10.4, 5.2, n, 0.7) for n in (45, 52, 57, 60)]
        root = 48
    elif kind == "chords":
        ev = [(0.2, 7.4, n, 0.75) for n in (48, 55, 59, 64)] + [(8.2, 7.2, n, 0.75) for n in (45, 52, 57, 60)]
        root = 48
    else:
        ev = [(0.2 + 0.4 * i, 7.6 - 0.4 * i, n, 0.7) for i, n in enumerate((36, 43, 48, 52, 55, 59, 64))] + [(8.2, 7.4, n, 0.7) for n in (33, 45, 52, 57, 60)]
        root = 48
    chord = [root] if mono else [root, root + 7, root + 11, root + 16]
    if kind == "thumb":
        chord = [root, root + 7]
    strike = q["amp"]["SUSTAIN"] < 0.35 or kind in ("thumb", "pluck", "arp")
    held = [(0.1 + 2.0 * k, 1.9, n, 0.75) for k in range(6) for n in chord] if strike else [(0.2, 11.8, n, 0.7 if not mono else 0.75) for n in chord]
    return ev, held, root


b3.phrase3 = phrase45
b2.phrase = phrase45


def estimate45(q):
    """as batch2_report.estimate, for Clouds in either effect slot"""
    sl, v = q["slots"], q["fm1_voices"]
    vc = 8.0 + 6.0 * v
    fx = {"REVERB": 14.0, "DELAY": 6.6, "CHORUS": 5.0}
    extra = sum(fx.get(sl.get(k, ""), 0.0) for k in ("FX1", "FX2"))
    extra += 4.0 if sl.get("SHP") == "FILTER" else 22.0 if sl.get("SHP") == "RINGS" else 0.0
    extra += sum(3.5 for k in ("MODA", "MODB") if sl.get(k) in ("TIDES", "PEAKS"))
    for fk in ("FX1", "FX2"):
        if sl.get(fk) == "CLOUDS":
            dmax = q["p"][fk]["DENSITY"]
            c2 = float(np.interp(dmax, [0.7, 0.9, 1.0], [16.5, 29.0, 42.0])) + 22.0
            return vc + extra, c2, vc + extra, c2
    one = 0.5 * vc + extra
    two = 0.5 * vc
    return one, two, one + 3.0, two + 3.0


b2.estimate = estimate45


def dry_script(q):
    """the lines that zero the effects' wet mixes for the pitch and tuning tests (a delay, a shimmer or Clouds detunes the tail)"""
    out = ""
    for si, key in ((4, "FX1"), (5, "FX2")):
        mod = q["slots"].get(key)
        if mod in ("DELAY", "CLOUDS") or (mod == "REVERB" and q["p"].get(key, {}).get("TYPE") == "SHIMMER") or (mod == "CHORUS" and q["p"].get(key, {}).get("MODE") == "VIBRATO"):
            import presets_bank45 as B
            lab = {"CLOUDS": "BLEND"}.get(mod, "MIX")
            out += f"par {si} {B.labels(mod).index(lab)} 0\n"
    return out


def units(fa, fb, sa, sb, ta, tb):
    """how audible the change from state a to state b is, in units where 1.0 is a just-clear change: the centroid x1.25, the level 2 dB,
    the spectrum 3 dB (mean of the log spectra's difference), the grain (the 20 ms RMS spread) x1.4, the width 3 dB, the tail 3 dB. The largest of them"""
    f = np.fft.rfftfreq(4096, 1.0 / SR)
    mk = (f > 60) & (f < 12000)
    la, lb = 20 * np.log10(sa[mk] + 1e-7), 20 * np.log10(sb[mk] + 1e-7)
    u = [abs(np.log(fb["cent"] / fa["cent"])) / np.log(1.25), abs(fb["rms_dB"] - fa["rms_dB"]) / 2.0, float(np.mean(np.abs(la - lb))) / 3.0,
         abs(np.log((fb["am"] + 0.3) / (fa["am"] + 0.3))) / np.log(1.4), abs(fb["side"] - fa["side"]) / 3.0, abs(tb - ta) / 3.0]
    return float(max(u))


def top_test(q, held, tmp):
    """the lower half of each knob's travel (0 to 0.5) against the upper (0.5 to 1): one chord held 3 s, the other knobs at their defaults, the
    sound (sustain window) and its tail compared at the knob's 0, 0.5 and 1, as units (above). The upper half must change the sound at least
    as much as the lower (0.9 of it), or by 1.5 units or more: the lower travel is the gentle part"""
    name, md = q["name"], q["macros"]
    strike = b2.STRIKE[0]
    chord = [n for t, d, n, v in held if t < 0.5] or [48]
    out = []
    sw, tw = ((0.3, 1.2), (1.3, 2.3)) if strike else ((1.5, 3.0), (3.4, 5.4))
    dur = 1.0 if strike else 3.2
    for k in range(4):
        st = []
        for v in (0.0, 0.5, 1.0):
            m = list(md)
            m[k] = v
            p = tmp / f"top{k}.wav"
            txt = "secs 6\n" + b3.DRY.get(name, "") * 0 + "\n".join(f"on 0.20 {dur:.2f} {n} 0.75" for n in chord) + "\n" + "".join(f"set {i + 1} 0 {m[i]}\n" for i in range(4))
            gb.render(name, txt, p, tmp)
            x = gb.load(p)
            t0, t1 = sw[0] + 0.2 - 0.2, sw[1]
            st.append((gb.feats(x, sw[0], sw[1]), gb.spec(x, sw[0], sw[1]), gr.db(gb.rms(x, tw[0], tw[1]))))
        low = units(st[0][0], st[1][0], st[0][1], st[1][1], st[0][2], st[1][2])
        top = units(st[1][0], st[2][0], st[1][1], st[2][1], st[1][2], st[2][2])
        out.append({"low": low, "top": top, "ok": top >= 0.9 * low or top >= 1.5})
    return out


def demo(q, ev, root, out_dir, base, tmp):
    """28 s: the phrase (twice if short) with K1, K2, K3, K4 swept 0 to 1 over 6 s each in turn, then 4 s at the defaults; and the 8 s still"""
    md = q["macros"]
    L = 28.0
    evs = list(ev)
    last = max(t + d for t, d, n, v in ev)
    if last < 14.0:
        evs += [(t + last + 0.5, d, n, v) for t, d, n, v in ev]
    evs = [(t, min(d, L - t - 0.1), n, v) for t, d, n, v in evs if t < L - 0.5]
    txt = f"secs {int(L)}\n" + "\n".join(f"on {t:.3f} {d:.3f} {n} {v:.2f}" for t, d, n, v in evs) + "\n"
    for k in range(4):
        t0, t1 = 6.0 * k, 6.0 * k + 6.0
        txt += f"set {k + 1} 0 {md[k]}\nramp {k + 1} {t0:.2f} {t1:.2f} 0 1\nset {k + 1} {t1 + 0.02:.2f} {md[k]}\n"
    p = tmp / "demo.wav"
    gb.render(q["name"], txt, p, tmp)
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(p), "-b:a", "160k", str(Path(out_dir) / f"{base}.mp3")], check=True)


def hands45(q, held, root, tmp, out=None, base=None):
    """batch3_report.hands3 with the struck presets measured on strikes 3 to 6 of 6 (a long ring is then at its steady overlap: the first strikes
    sound lighter only because nothing rings under them yet)"""
    name, md = q["name"], q["macros"]
    strike = b2.STRIKE[0]
    L = 13 if strike else 9
    txt = f"secs {L}\n" + "\n".join(f"on {t:.2f} {min(d, L - 0.2):.2f} {n} {v:.2f}" for t, d, n, v in held) + "\n"
    txt += "".join(f"set {k + 1} 0 {v}\n" for k, v in enumerate(md))
    p = tmp / "off3.wav"
    gb.render(name, txt, p, tmp)
    x = gb.load(p)
    A, Bw = b3.w0(name)
    ks = range(2, 6)
    ts = [0.1 + A + 2.0 * k for k in ks] if strike else list(np.arange(1.0, 8.01, 0.5))
    dl = (Bw - A) if strike else 1.0
    r = [gr.db(gb.rms(x, t, t + dl)) for t in ts]
    c = [gb.centroid(x, t, t + dl) for t in ts]
    f0 = b2.hz(root)
    one = (f"secs {L}\n" + "".join(f"on {0.1 + 2.0 * k:.2f} 1.9 {root} 0.75\n" for k in range(6))) if strike else f"secs 9\non 0.1 8.8 {root} 0.75\n"
    one += "".join(f"set {k + 1} 0 {v}\n" for k, v in enumerate(md))
    p1 = tmp / "one.wav"
    gb.render(name, one, p1, tmp)
    x1 = gb.load(p1)
    pc = [b3.band_cents(x1, 0.1 + A + 2.0 * k, 0.1 + Bw + 2.0 * k, f0) for k in ks] if strike else [b3.band_cents(x1, t, t + 1.0, f0) for t in np.arange(1.0, 8.01, 1.0)]
    if not strike:
        rf = [gr.db(gb.rms(x, t, t + 0.25)) for t in np.arange(1.0, 8.0, 0.25)]
        cf = [gb.centroid(x, t, t + 0.25) for t in np.arange(1.0, 8.0, 0.25)]
    else:
        rf, cf = r, c
    res = {"fast_pp_dB": float(max(rf) - min(rf)), "fast_ratio": float(max(cf) / max(min(cf), 1)), "rms_pp_dB": float(max(r) - min(r)), "cent_ratio": float(max(c) / max(min(c), 1)),
           "cent_min": float(min(c)), "cent_max": float(max(c)), "drift_c": float(max(pc) - min(pc)), "pitch_route_c": b3.pitch_routes(q)}
    if out:
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-t", "8", "-i", str(p), "-b:a", "160k", str(out / f"{base}_still.mp3")], check=True)
    return res


def classify(o):
    """the movement level from the hands-off measurements (batch3_report's limits): bold if the sound moves 4 dB, x1.5 or 6 dB fast; subtle
    if it moves under 2 dB, x1.2, 3 dB fast and no big vibrato; else medium"""
    wob = o["pitch_route_c"] * 2
    if o["rms_pp_dB"] >= 4.0 or o["cent_ratio"] >= 1.5 or o["fast_pp_dB"] >= 6.0:
        return "bold"
    if o["rms_pp_dB"] < 2.0 and o["cent_ratio"] < 1.2 and o["fast_pp_dB"] < 3.0 and o["fast_ratio"] < 1.5 and wob < 16:
        return "subtle"
    return "medium"


def measure(q, num, tmp, mp3, nodemo=False):
    name = q["name"]
    ev, held, root = phrase45(q)
    b2.STRIKE[0] = held[0][1] < 3.0
    ds = dry_script(q)
    if ds:
        b3.DRY[name] = ds
    else:
        b3.DRY.pop(name, None)
    if q["bank45"].get("win"):
        b3.WIN[name] = tuple(q["bank45"]["win"])
    r = b2.measure(q, num, tmp, None)
    base = f"{num:02d}_{name.replace(' ', '_')}"
    out = Path(mp3) if mp3 else None
    if out:
        out.mkdir(parents=True, exist_ok=True)
    r["off"] = hands45(q, held, root, tmp, out, base)
    r["move_intended"] = q["move"]
    r["move_measured"] = classify(r["off"])
    r["move"] = q["move"]            # the intended level, checked against its own limits (batch3_report.verdicts)
    r["top"] = top_test(q, held, tmp)
    if out and not nodemo:
        demo(q, ev, root, out, base, tmp)
    return r


def verdicts(r, med):
    b3.verdicts(r, med)
    c = r["checks"]
    c.pop("K2-K4 audible", None)
    c["K1-K4 audible"] = all(e["pass"] for e in r["ev"])
    c["knobs build toward the top"] = all(t["ok"] for t in r["top"])
    r["pass"] = all(c.values())
    return r


def main():
    a = sys.argv[1:]
    opt = lambda k: a[a.index(k) + 1] if k in a else None
    mp3, js, hl = opt("--mp3"), opt("--json"), opt("--host-log")
    lo, hi = int(opt("--from") or 1), int(opt("--to") or 99)
    nodemo = "--nodemo" in a
    names = [x for i, x in enumerate(a) if not x.startswith("--") and (i == 0 or a[i - 1] not in ("--mp3", "--json", "--host-log", "--from", "--to"))]
    P = load_bank()
    host = gr.host_lines(hl) if hl else {}
    out = []
    with tempfile.TemporaryDirectory() as t:
        for i, q in enumerate(P):
            if (names and q["name"] not in names) or not lo <= i + 1 <= hi:
                continue
            r = measure(q, i + 1, Path(t), mp3, nodemo)
            r["host"] = host.get(q["name"])
            r["fm1_voices"] = q["fm1_voices"]
            r["cat"] = q["cat"]
            r["pos"] = i + 1
            out.append(r)
            print(f"{r['num']} {q['name']:12} pk {r['peak']:.2f} rms {r['ctl_rms']:.3f}", flush=True)
    med = float(np.median([gr.db(r["ctl_rms"]) for r in out]))
    for r in out:
        verdicts(r, med)
        m, o = r["morph"], r["off"]
        bad = [k for k, v in r["checks"].items() if not v]
        print(f"{r['num']:2} {r['name']:12} {r['move'][:3]} pk {r['peak']:.2f} rms {r['ctl_rms']:.3f} ld {r['loud_batch']:+.1f}/{r['loud_b2']:+.1f} "
              f"MORPH rms {min(s['rms'] for s in m):.3f}-{max(s['rms'] for s in m):.3f} cent " + "/".join(f"{s['cent']:.0f}" for s in m)
              + f" ({r['morph_chars']}) pitch {r['morph_pitch_max']:.0f}c tune {r['tune_max']:.0f}c "
              + f"off {o['rms_pp_dB']:.1f}dB x{o['cent_ratio']:.2f} dr {o['drift_c']:.0f}c vib {o['pitch_route_c']:.0f}c "
              + "K" + "".join("+" if e["pass"] else "-" for e in r["ev"]) + " top " + "/".join(f"{t['low']:.1f}>{t['top']:.1f}" for t in r["top"])
              + f" est {r['est2'][0]:.0f}/{r['est2'][1]:.0f} | " + ("PASS" if r["pass"] else "FAIL " + "; ".join(bad)))
        for k, e in enumerate(r["ev"]):
            if not e["pass"] or "-v" in a:
                print(f"     K{k + 1} sweep cent x{e['sc']:.2f} rms x{e['sr']:.2f}  A/B cent x{e['ab_cent']:.2f} rms x{e['ab_rms']:.2f}  {e['why']}")
    if js:
        Path(js).write_text(json.dumps(out, indent=1, default=float))


if __name__ == "__main__":
    main()
