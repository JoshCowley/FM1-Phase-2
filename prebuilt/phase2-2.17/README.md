# FM1 Phase 2 2.17 — install files (pre-release)

**Not yet run on an FM-1.** Built and checked on a computer only (engine checks, preset check, 37 simulator scripts). The last build run on an FM-1 is update 9.

## Install from any computer

Easiest: the web installer at https://joshcowley.github.io/FM1-Phase-2/install/ (Chrome or Edge).

## Install on a Mac

1. Download `fm1ph2-2.17.fwsc` and `install_fm1ph2_2.17.sh` into the same folder.
2. Plug the FM-1 straight into the Mac (no hub) and quit anything else using MIDI.
3. In Terminal, in that folder: `sh install_fm1ph2_2.17.sh`

The script checks the package against SHA-256 `866b17f50b4dfa172b6a2da15fdd04cd7a83c79363679fcdff1907ba217d920c` (also in `fm1ph2-2.17.fwsc.sha256`) before installing, fetches fm1-base's installer (c8e0599), and keeps saved user presets.

## What's in it

- Every page's picture in the option C family; HOME is the spectral ribbon.
- Presets 1–16: granular (TONE, MOTION, SPACE, TEXTURE). Presets 17–32: modulation-led instruments, and 33–48: classic synth presets with mostly subtle modulated movement (both K1 MORPH, then MOTION, SPACE, TEXTURE). 125 presets in all.
- Clouds runs on the second core, at 32 kHz (to revisit before release).

## Known issues

GLASS BELLS (−18 cents at C2) and MODEL DIAL (−28 cents at C4) are out of tune; six instruments' K2–K4 changes measured small; FROZEN PAD is silent if K4 is high before a note is played; an intermittent host test failure is not yet explained.

## Licence

The package contains Mutable Instruments' MIT-licensed code; its notice is `LICENSE-MUTABLE-MIT.txt` (in this folder and at the top of the repository). The firmware as a whole is GPL-3.0; its source is this repository. Release build 2.17 (identity FM-1_79021700) of commit f5d5040 with fm1-base c8e0599 and pichenettes/eurorack f4cfa76.
