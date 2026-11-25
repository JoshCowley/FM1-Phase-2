#!/bin/sh
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# Update 42: CLOUDS' RENDER, the exact comparisons rack_host cannot make (two runs in one process differ): each render is a process of its own
# (render_preset, TIDE PAD with Clouds' parameters set, a held chord, 6 s), compared as WAV files.
#   EURORACK=/path/to/eurorack sh build_host.sh && sh clouds_render_check.sh
# NORMAL (mask 15) against HIGH (0): GRANULR, LOOPDLY under -60 dB rms (the spectral mode, a different but equal glitch pattern, is held to its level);
# the idle diffuser and idle feedback path (4, 8): sample for sample; the page's RENDER 0, 1, 2 (LIGHT, HIGH, NORMAL) against masks 3887, 0, 15: sample for sample.
set -e
cd "$(dirname "$0")"
R=build/host/render_preset; T=$(mktemp -d)
cat > $T/g.txt <<'EOS'
secs 6
on 0.1 5.5 48 0.8
on 0.1 5.5 52 0.8
on 0.1 5.5 55 0.8
on 0.1 5.5 59 0.8
par 4 0 0.3
par 4 1 0.5
par 4 3 0.9
par 4 4 0.4
par 4 5 1.0
par 4 7 0.0
par 4 8 0.3
EOS
{ cat $T/g.txt; echo "par 4 9 1"; echo "par 4 7 0.3"; } > $T/l.txt
{ cat $T/g.txt; echo "par 4 9 2"; } > $T/s.txt
for sc in g l s; do for m in 0 4 8 15 3887; do RP_CLQ=$m $R TIDE_PAD $T/$sc.txt $T/${sc}_$m.wav > /dev/null; done; done
for p in 0 1 2; do { cat $T/g.txt; echo "par 4 25 $p"; } > $T/gp$p.txt; $R TIDE_PAD $T/gp$p.txt $T/gp_$p.wav > /dev/null; done
python3 -I - "$T" <<'EOP'
import sys, struct, math
import numpy as np
T = sys.argv[1]
def rd(p):
    b = open(p, 'rb').read(); i = b.find(b'data'); n = struct.unpack('<I', b[i+4:i+8])[0]
    return np.frombuffer(b[i+8:i+8+n], dtype='<f4')
def db(a, b):
    d = np.sqrt(np.mean((a - b) ** 2)); r = np.sqrt(np.mean(a ** 2)); return -240.0 if d == 0 else 20 * math.log10(d / r)
bad = 0
def say(s, ok):
    global bad; print("6i " + s + ("" if ok else "   FAIL")); bad += not ok
for sc, nm in (("g", "GRANULR"), ("l", "LOOPDLY"), ("s", "SPECTRL")):
    h, n = rd(f"{T}/{sc}_0.wav"), rd(f"{T}/{sc}_15.wav")
    if sc == "s":
        lv = 20 * math.log10(np.sqrt(np.mean(n ** 2)) / np.sqrt(np.mean(h ** 2))); say(f"{nm}: NORMAL against HIGH: level {lv:+.2f} dB (limit 0.3; the spectral mode's glitch pattern differs)", abs(lv) < 0.3)
    else:
        d = db(h, n); say(f"{nm}: NORMAL against HIGH: {d:.1f} dB rms (limit -60)", d < -60)
for m, nm in ((4, "idle diffuser"), (8, "idle feedback path")):
    d = db(rd(f"{T}/g_0.wav"), rd(f"{T}/g_{m}.wav")); say(f"GRANULR with the {nm} skipped: {d:.0f} dB against running it (want -240, sample for sample)", d <= -240)
for p, m in ((0, 3887), (1, 0), (2, 15)):
    d = db(rd(f"{T}/g_{m}.wav"), rd(f"{T}/gp_{p}.wav")); say(f"RENDER {('LIGHT', 'HIGH', 'NORMAL')[p]} (page value {p}) against mask {m}: {d:.0f} dB (want -240)", d <= -240)
sys.exit(1 if bad else 0)
EOP
rc=$?; rm -rf $T; exit $rc
