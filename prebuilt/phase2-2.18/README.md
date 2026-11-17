# FM1 Phase 2 2.18 — install files (pre-release)

**Not yet run on an FM-1.** Built and checked on a computer only. The last build reported as run on an FM-1 is update 9.

## What changed since 2.17

Presets that use the Braids oscillator played two octaves below the note pressed (A above middle C came out at 110 Hz, not 440 Hz). That covered most of the 125 presets, the older ones included. Braids is tuned for 96 kHz; run at 48 kHz it sounds an octave low, and the firmware had been shifting it a further octave down instead of up. 2.18 shifts it up, so presets now play at the note pressed. The other sound sources (Plaits, Elements, Rings and the grain player) were already at the right pitch and are unchanged.

Raising the pitch makes those presets brighter and changes their level somewhat; the presets have not yet been retuned for that.

## Install from any computer

Easiest: the web installer at https://joshcowley.github.io/FM1-Phase-2/install/ (Chrome or Edge).

## Install on a Mac

1. Download `fm1ph2-2.18.fwsc` and `install_fm1ph2_2.18.sh` into the same folder.
2. Plug the FM-1 straight into the Mac (no hub) and quit anything else using MIDI.
3. In Terminal, in that folder: `sh install_fm1ph2_2.18.sh`

The script checks the package against SHA-256 `998325de5dde696373391d5eaa946418c5b4e68a872d722a90d0da9f86d66510` (also in `fm1ph2-2.18.fwsc.sha256`) before installing, fetches fm1-base's installer (c8e0599), and keeps saved user presets.

## Known issues

- Engine check 7d fails: with Clouds' GRANULR mode on the second core, the output measures about 8–10% quieter than on one core, against an allowance of 8%. It was already failing now and then before this change (up to 8.6%); with the higher pitch it fails every run. Not yet explained.
- GLASS BELLS and MODEL DIAL are out of tune; six instruments' K2–K4 changes measured small; FROZEN PAD is silent if K4 is high before a note is played.

## Licence

The package contains Mutable Instruments' MIT-licensed code; its notice is `LICENSE-MUTABLE-MIT.txt` (in this folder and at the top of the repository). The firmware as a whole is GPL-3.0; its source is this repository. Release build 2.18 (identity FM-1_79021800) with fm1-base c8e0599 and pichenettes/eurorack f4cfa76.
