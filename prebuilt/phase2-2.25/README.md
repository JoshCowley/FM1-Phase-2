# FM1 Phase 2 2.25 — install files (pre-release)

**Not yet run on an FM-1.** Built and checked on a computer only.

## What changed since 2.24

- A new factory bank of 45 presets plus INIT (was 124): 9 pads and drones, 9 keys, bells and plucks, 6 thumb pianos, 6 leads, 7 basses, 8 playable textures. The granular presets and the thin, noisy and FX-style sounds are gone. Every knob drives several parameters through the knob layer, from gentle movement low on the knob to glitch or wash near the top.
- Saved user sounds and the last preset played are moved to the same preset in the new bank, or to INIT if it was removed.

## In 2.24

- Fix: since 2.22 a mono preset could keep a note on after its key went up, if keys were held when a preset loaded (the held-key list wasn't cleared on load). Now cleared on every preset load; the check that plays every preset 10 ways passes again.

## In 2.23

- Sequencer: deleting a note on the NOTES page moves the cursor on to the next step (wrapping at the pattern's end), as entering a note does; after a lasso delete it goes to the step after the lasso.
- Sequencer pages: more contrast. On-steps bright, empty steps a visible grey, the cursor a white frame, the playhead ember orange with black edges, and the other marks recoloured.

## In 2.22

- Mono presets remember the keys held: the newest key plays, and letting go of it goes back to the key held before it (gliding there if GLIDE is up). Letting go of a key underneath leaves the note playing alone.

## In 2.21

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

Web installer: https://joshcowley.github.io/FM1-Phase-2/install/ (Chrome or Edge). On a Mac: put `fm1ph2-2.25.fwsc` and `install_fm1ph2_2.25.sh` in one folder and run `sh install_fm1ph2_2.25.sh`. The script checks SHA-256 `e4f77c015d8d004ed532a953c92d75601bce99459cc30d0d0ead3d59b4757a54` first and keeps saved user presets.

## Known issues

- Engine check 7d (Clouds GRANULR, two cores vs one: 8-10% quieter, allowance 8%) fails; not yet explained.
- The firmware image shrank by 14,648 B with the knob layer; not yet explained (renders are unchanged).
- GLASS BELLS and MODEL DIAL are out of tune; FROZEN PAD is silent if K4 is high before a note.

## Licence

Contains Mutable Instruments' MIT-licensed code (`LICENSE-MUTABLE-MIT.txt`). The firmware as a whole is GPL-3.0; its source is this repository. Release build 2.25 (identity FM-1_79021900) with fm1-base c8e0599 and pichenettes/eurorack f4cfa76.
