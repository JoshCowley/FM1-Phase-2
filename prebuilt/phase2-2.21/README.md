# FM1 Phase 2 2.21 — install files (pre-release)

**Not yet run on an FM-1.** Built and checked on a computer only.

## What changed since 2.20

- Notes on sustained presets could hang on the FM-1. A key-up the sound engine's queue refused was dropped; it is now re-sent until taken. A watchdog also ends any note held 150 ms after nothing is counted as playing. Loading a preset or all-notes-off now clears the sustain pedal.
- ARP > HOLD no longer stays on after a restart (a saved HOLD with the arp off latched every note).
- A read-out: hold OCT- and OCT+ (off the CLOUDS pages) to see the voices sounding, gates held, keys, pedal, HOLD and how often the watchdog fired.
- Not yet confirmed on an FM-1: the hang never appeared on the computer, so whether this cures it is unknown until tried.

## In 2.20

- Two taps on ARP (the arp's FM-1 sound off) or the chords' FM-1 sound off no longer silence the keys: they play the FM-1 directly, and the arp or chords keep only their MIDI send. Three taps (SEND) don't change the FM-1's sound. Notes held while a switch changes can't hang. Checked by a new simulator script (sim_taps.txt) over all 32 on/off states of the five switches. Limit: a key already held when the arp's FM-1 sound goes off stays silent until released.

## In 2.19

- The knob layer: each knob (K1-K4) can drive up to 8 parameters of its own, apart from the 12 modulation rows, each with a depth, a window within the knob's travel and a curve, so one knob can move gently at first and push to glitch or wash near the top. The factory presets don't use it yet; the preset revision (45 presets) comes next. The home page names a knob's layer targets.
- User save slots are now 15 (were 64). Slots 1-15 keep what they held; anything saved in 16-64 is no longer reachable.
- The factory presets sound the same as in 2.18 (checked sample for sample on a computer).

## Install

Web installer: https://joshcowley.github.io/FM1-Phase-2/install/ (Chrome or Edge). On a Mac: put `fm1ph2-2.21.fwsc` and `install_fm1ph2_2.21.sh` in one folder and run `sh install_fm1ph2_2.21.sh`. The script checks SHA-256 `afc1715d07e927c1e70e4201af162be1403ded8f9d9a38efe5eb7885dbc27b97` first and keeps saved user presets.

## Known issues

- Engine check 7d (Clouds GRANULR, two cores vs one: 8-10% quieter, allowance 8%) fails; not yet explained.
- The firmware image shrank by 14,648 B with the knob layer; not yet explained (renders are unchanged).
- GLASS BELLS and MODEL DIAL are out of tune; FROZEN PAD is silent if K4 is high before a note.

## Licence

Contains Mutable Instruments' MIT-licensed code (`LICENSE-MUTABLE-MIT.txt`). The firmware as a whole is GPL-3.0; its source is this repository. Release build 2.21 (identity FM-1_79021900) with fm1-base c8e0599 and pichenettes/eurorack f4cfa76.
