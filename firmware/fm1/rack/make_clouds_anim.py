#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""The CLOUDS picture's animation, from the firmware itself (update 7): writes a simulator script that plays a chord
in GRANULR, then FREEZE, PITCH up and down, DENSITY at its silent middle, LOOPDLY, SPECTRL and silence, taking a
screenshot every 80 ms; then (with --build) runs the host simulator on it and assembles clouds-picture-sim.mp4
(720 x 720, nearest neighbour, H.264 yuv420p) and a contact sheet of 8 key frames with its caption.

   python3 make_clouds_anim.py OUTDIR [--host BINARY]     the script and frames in OUTDIR, the video beside them"""
import os, subprocess, sys

STEP = 80
lines, marks = [], []
n = [0]
def w(ms): lines.append("wait %d" % ms)
def frames(sec, label=None):
    for _ in range(int(sec * 1000 / STEP)):
        lines.append("wait %d" % STEP)
        lines.append("shot f%04d.png" % n[0])
        n[0] += 1
    if label: marks.append((n[0] - 1, label))
def c(s): lines.append(s)

c("wait 500"); c("press HOME"); c("release HOME"); c("wait 200"); c("tap FX"); c("wait 300")
c("turn ALGO 1"); c("wait 300"); c("tap FX"); c("wait 500")
c("spin K1 15")                                   # POSITION 20 -> 50
c("spin K4 -4")                                   # DENSITY 70 -> 66
c("key w0 down 100"); c("key w2 down 100"); c("key w4 down 100"); c("key w7 down 90")
frames(3.0, "GRANULR: a chord")
c("spin K1 8"); c("spin K2 -8")                   # POSITION 66, SIZE 44
frames(2.0, "POSITION and SIZE turned")
c("spin K4 10")                                   # DENSITY 86
frames(1.5, "DENSITY up")
c("spin K4 -18"); frames(1.8, "DENSITY at 50: Clouds plays no grains")
c("spin K4 18"); frames(1.0)
c("turn SELECT 2"); c("wait 100"); c("spin K3 1")   # page 3: FREEZE on
frames(2.5, "FREEZE")
c("spin K3 -1"); frames(1.0)
c("turn SELECT -2"); c("wait 100"); c("spin K3 12")   # PITCH +12
frames(2.2, "PITCH +12: warm, rising")
c("spin K3 -24"); frames(2.4, "PITCH -12: cool, larger, sinking")
c("spin K3 12"); frames(0.8)
c("turn SELECT 4"); c("wait 100"); c("spin K1 20"); c("turn SELECT -4"); c("wait 100")   # SPRAY up (page 5)
frames(2.0, "SPRAY: a wider halo")
c("turn SELECT 2"); c("wait 100"); c("spin K2 1")   # MODE: LOOPDLY
frames(3.0, "LOOPDLY")
c("turn SELECT -2"); c("wait 100"); c("spin K2 12"); frames(2.0, "LOOPDLY: SIZE up")
c("turn SELECT 2"); c("wait 100"); c("spin K2 1")   # SPECTRL
frames(3.0, "SPECTRL (illustrative)")
c("turn SELECT -1"); c("wait 100"); c("spin K1 20")   # TEXTURE
frames(2.0, "SPECTRL: TEXTURE")
c("key w0 up"); c("key w2 up"); c("key w4 up"); c("key w7 up")
frames(3.5, "silence: still and dim")

def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, "anim.txt"), "w").write("\n".join(lines) + "\n")
    open(os.path.join(out, "marks.txt"), "w").write("\n".join("%d %s" % m for m in marks) + "\n")
    if "--host" in sys.argv:
        host = sys.argv[sys.argv.index("--host") + 1]
        subprocess.check_call([host, os.path.join(out, "anim.txt"), out], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
if __name__ == "__main__":
    main()
