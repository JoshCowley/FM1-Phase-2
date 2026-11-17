# FM1 Phase 2 2.20 — install files (pre-release)

**Not yet run on an FM-1.** Built and checked on a computer only.

## What changed since 2.18

- The knob layer: each knob (K1-K4) can drive up to 8 parameters of its own, apart from the 12 modulation rows, each with a depth, a window within the knob's travel and a curve, so one knob can move gently at first and push to glitch or wash near the top. The factory presets don't use it yet; the preset revision (45 presets) comes next. The home page names a knob's layer targets.
- User save slots are now 15 (were 64). Slots 1-15 keep what they held; anything saved in 16-64 is no longer reachable.
- The factory presets sound the same as in 2.18 (checked sample for sample on a computer).

## Install

Web installer: https://joshcowley.github.io/FM1-Phase-2/install/ (Chrome or Edge). On a Mac: put `fm1ph2-2.20.fwsc` and `install_fm1ph2_2.20.sh` in one folder and run `sh install_fm1ph2_2.20.sh`. The script checks SHA-256 `8326f46cf932cf80e937eb3179cafa4c38d5c94edf759cc8649123b51f953a1c` first and keeps saved user presets.

## Known issues

- Engine check 7d (Clouds GRANULR, two cores vs one: 8-10% quieter, allowance 8%) fails; not yet explained.
- The firmware image shrank by 14,648 B with the knob layer; not yet explained (renders are unchanged).
- GLASS BELLS and MODEL DIAL are out of tune; FROZEN PAD is silent if K4 is high before a note.

## Licence

Contains Mutable Instruments' MIT-licensed code (`LICENSE-MUTABLE-MIT.txt`). The firmware as a whole is GPL-3.0; its source is this repository. Release build 2.20 (identity FM-1_79021900) with fm1-base c8e0599 and pichenettes/eurorack f4cfa76.
