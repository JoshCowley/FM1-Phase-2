#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""Loudness fit for the factory bank of 45 (presets_bank45.py): renders each preset's phrase at its default knobs (bank45_report.phrase45),
takes the RMS (0.5 to 15.5 s, as the batch reports) and scales AMP LEVEL to 0.0975 (the median RMS of batches 2 and 3), three passes (LEVEL is a squared gain),
writes presets_bank45_levels.json (LEVEL to 2 places, at most 1.0). Needs build/host (build_host.sh) and runs fm1_presets.py,
make_presets.py and build_host.sh itself between passes.   python3 fit_levels.py [NAME ...]"""
import json
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
TARGET = 0.0975
MAXLEVEL = {}      # a cap lower than 1.0 where the loudest corner of the knobs would otherwise peak above 0.5
LV = HERE / "presets_bank45_levels.json"


def rebuild():
    for c in (["python3", "fm1_presets.py"], ["python3", "make_presets.py"], ["sh", "build_host.sh"]):
        subprocess.run(c, cwd=HERE, check=True, capture_output=True)


def main():
    names = sys.argv[1:]
    import bank45_report as R
    import granular_batch as gb
    levels = json.loads(LV.read_text()) if LV.exists() else {}
    for it in range(3):
        rebuild()
        P = R.load_bank()
        with tempfile.TemporaryDirectory() as t:
            t = Path(t)
            for q in P:
                if names and q["name"] not in names:
                    continue
                ev, held, root = R.phrase45(q)
                p = t / "l.wav"
                gb.render(q["name"], gb.script(ev, q["macros"], sweeps=False), p, t)
                rms = gb.rms(gb.load(p), 0.5, 15.5)
                cur = q["amp"]["LEVEL"]
                new = min(MAXLEVEL.get(q["name"], 1.0), round(cur * (TARGET / max(rms, 1e-4)) ** 0.5, 2))      # AMP LEVEL is a squared gain: RMS goes with LEVEL x LEVEL
                levels[q["name"]] = new
                print(f"pass {it + 1} {q['name']:14} rms {rms:.4f} level {cur:.2f} -> {new:.2f}" + ("  (capped)" if new >= 1.0 else ""), flush=True)
        LV.write_text(json.dumps(levels, indent=0, sort_keys=True))
    rebuild()


if __name__ == "__main__":
    main()
