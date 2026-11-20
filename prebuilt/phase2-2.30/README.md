# FM1 Phase 2 2.30 — install files (pre-release)

**Not yet run on an FM-1.** Built and checked on a computer only.

## What changed since 2.29

- Drum track filled out. LANE SOUND (pick each lane's Braids model from a list with its processor cost; tone, pitch, decay) and LANE MIX (level, pan, send to FX). Four kits: 909 ISH, 808 ISH, LO-FI, SPARSE. LANE FEEL (each lane's own length, timing, shift and choke), DRUM GEN (Euclid and other generators fill a lane, then edit by hand; no undo). Patterns A-H switch at the bar line and are saved. DRUM MIDI (channel and note per lane; channel 10 notes coming in play the lanes) and DRUM KIT (pattern, kit, swing, and IF BUSY: CLOUDS eases when the processor is full). STYLE starts and stops the drums.

## In 2.29

- Drum track, first playable: a DRUM page on the 7th tap of SEQ. Six lanes (kick, snare, closed hat, open hat, clap, perc), 16 steps; hold SEQ and the white keys are the lane's steps; mute per lane; drums run with PLAY and follow the tempo or MIDI clock; each lane sends its General MIDI drum note on channel 10. Four trimmed Braids drum voices on the second core. The pattern starts empty: hold SEL and turn ALGO left to load the 909 ISH beat. Not yet saved (comes with patterns A-H).

## In 2.28

- JAM mode: double-tap FX on any page (or the third FX page). The lower keys keep playing notes; the upper 12 keys each fire a move: GRAIN DELAY, STUTTER 1/8, 1/16, 1/32, REVERSE SCRUB, TAPE STOP, WASH, FREEZE PAD, GLITCH, FILTER DIVE, FILTER RISE, CRUSH. Each holds or latches, has an amount and follows velocity; moves stack; the stutters, GLITCH and FILTER RISE follow the tempo. Moves sit on top of the sound and never change the preset; leaving JAM or changing preset ends them. Moves use the effects the preset has: most presets have no CLOUDS, so the grain moves there are swells. JAM settings aren't kept after power-off.

## In 2.27

- CLOUDS grain controls in the manner of the GR-1: two new first pages. GRAIN 1: POSITION, WINDOW (how wide a region grains are drawn from), SPEED (a playhead that moves through the buffer, reverse through stopped to forward, up to 2x; with FREEZE it scrubs the frozen sound) and JITTER (random grain starts, per grain). GRAIN 2: SIZE, DENSITY, RND PAN (was SPREAD) and FREEZE. All are modulation and knob targets and saved in your sounds. The buffer holds 0.77 to 6.14 s depending on QUALITY. At their defaults the presets sound as in 2.26.

## In 2.26

- The filter is always in the sound path. Hold ENV and turn K1 on any page to sweep it (+/-5 octaves around the preset's own cutoff; the screen shows FILTER SWEEP). Also on the FILTER page, and as MIDI CC 74 (64 = centre). Matrix rows and the knobs can still move the cutoff. The sweep resets on loading a factory preset and is kept in your saved sounds.
- ENV now turns its page when you let go of it (and not at all if you turned K1 while holding it). The filter's choice moved off ENV's second page.

## In 2.25

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

Web installer: https://joshcowley.github.io/FM1-Phase-2/install/ (Chrome or Edge). On a Mac: put `fm1ph2-2.30.fwsc` and `install_fm1ph2_2.30.sh` in one folder and run `sh install_fm1ph2_2.30.sh`. The script checks SHA-256 `9178719526966fec9dcb191f809084ea36fe502f6942aaccd2c005dc943ea708` first and keeps saved user presets.

## Known issues

- Engine check 7d (Clouds GRANULR, two cores vs one: 8-10% quieter, allowance 8%) fails; not yet explained.
- The firmware image shrank by 14,648 B with the knob layer; not yet explained (renders are unchanged).
- GLASS BELLS and MODEL DIAL are out of tune; FROZEN PAD is silent if K4 is high before a note.

## Licence

Contains Mutable Instruments' MIT-licensed code (`LICENSE-MUTABLE-MIT.txt`). The firmware as a whole is GPL-3.0; its source is this repository. Release build 2.30 (identity FM-1_79021900) with fm1-base c8e0599 and pichenettes/eurorack f4cfa76.
