# FM1 Rack (Lite) firmware

The user manual for this firmware: `manual/FM1-Rack-FM1-Firmware-Manual.md` (hosted as `web/manual/fm1.html`).

**Status: builds and passes fm1-base's checks; its sound side passes the computer check; it has not run on an
FM-1.** Don't install it until `fm1/bench` has run on your FM-1 and its FPU check said OK: this firmware plays the
engine in the audio interrupt, and the benchmark is what tells us that's affordable.

## Update 58 / release 2.34: tap again = next group (host-tested, NOT run on an FM-1)

- **The rule.** A page button tapped again goes to its next GROUP of pages; SELECT turns the pages inside the group. Groups: ENV AMP / FILTER / SHAPER; LFO: LFO (9 pages), GATE PATTERN (2), MOD A, MOD B, MSEQ, MATRIX; FX: FX 1, FX 2, JAM; EDIT: SOURCE, OSC; SEQ: the note pages, the drum pages. SEL, ARP and GLO are one group each (tapped again they stay). From HOME: FILTER ENV x2 (was 8), GATE PATTERN LFO x2 (12), JAM FX x3 (7), the DRUM grid SEQ x2 (7); SAVE and a preset change as before (1 each). `sim_groups.txt` proves them.
- **Kept.** The SEL / ARP double and triple taps, FX double tap (JAM), hold FX and taps, ENV + K1 sweep, hold SEQ (the step layer), OCT- + OCT+ read-outs are untouched. LFO double tap (MATRIX) conflicts with tap-again, so a tap on one of LFO's own pages waits the 0.3 s before it moves on: a quick double tap is only ever the MATRIX, a slower second tap is the next group. On the drum pages SELECT is the step or lane, so SEL held + SELECT turns the drum pages.
- **LFO opens on the LFO page.** FILTER's F ATK-F REL page is hidden unless ENV SRC is OWN. Drum pages in the order DRUM, KIT, LANE SOUND, MIX, TIMING, GENERATE, MIDI.
- **Renamed:** TEMPO + VOICES, TOUCH + LEDS, TUNING + BEND, EQ + COMP, CPU + MEMORY, CLICK + COUNT IN; BASS STYLE, HUMANISE; SPICE + CHANCE, GENERATE, DRUM GRID, KIT + PATTERNS, LANE TIMING, DRUM GENERATE. Not changed: the sound (all 46 presets render identically), saved data, LAYER MIDI CH and CHORD + ARP (still in GLOBAL).
- Not done: each group remembering its last page (SEQ does), folding AMP / LFO low-use pages.

## Update 56: pages moved, merged, and a GATE PATTERN generator (host-tested, NOT run on an FM-1)

- LAYER MIDI CH and CHORD + ARP are now GLOBAL pages 9 and 10 (same params and storage).
- RHYTHM, RHYTHM CHANCE and the Euclid ring are ARP pages 3 and 4 ("RHYTHM: WHICH STEPS", "CHANCE: HOW OFTEN"; HITS/STEPS/TURN, PATTERN/CHANCE/MUTATE). SEL has 4 pages.
- LFO gains pages 10 and 11: GATE PATTERN, two Euclid lanes (ringgen.c shared with RHYTHM, gatepat.c). GATE 1/2 are matrix sources (ext lanes 6 and 7), LFO RETRIG, PEAKS/TIDES GATE and FUNC TRIG options.
- Settings record RKL9 becomes RKLA (arp[40] becomes arp[53]); RKL9 loads and gains defaults.
- Tests: sim_gate, sim_pageorder, rack_host check 15 (`--gate`).

## Update 26: the filter is always in the chain, with a SWEEP (host-tested, NOT heard, NOT run on an FM-1)

Josh: "the filter in the signal chain that can be modulated but also controlled by a knob for filter sweeps. Have it always in the chain instead of selectable in the envelope pages."

**Before.** The filter was a module of the shaper slot (SHP, module 12, beside Rings and empty), picked with ALGORITHM on the second ENV page; 42 of the 46 factory presets had it, two had Rings and two (INIT and one more) nothing. Its parameters, the matrix rows and the knob targets into it were SHP's; there was no performance control for the cutoff.
**Now.** The filter is in every voice's chain after the shaper and before AMP (`engine/rack.cc`, the block "FILTER: always in the voice's chain"). Its parameters sit on **GLO's spare parameters 8-22** (`FLT_P0`), the player's **SWEEP on GLO 23** (`FLT_SWEEP`): the same trick as OSC on AMP's 24-31, so no new slot (a ninth slot would clash with the knob layer's `MT_ROW` 9). The browser build of the engine (no `RACK_FM1`) keeps the filter module in SHP as before. `set_module(SHP, 12)` plays as empty; `make_presets.py` (`migrate_filter`) moves the 42 presets' filter calls (set_param, set_meta, matrix rows, knob targets) to GLO 8-22 and empties their shaper. ENV's pages: AMP, FILTER (`PG_FLT`, a page of its own, `UI_FLT` from `make_ui.py`: the browser's FILTER pages plus SWEEP), then the shaper, whose picker is Rings or empty. The matrix and the knob layer list the parameters as **FLT.**
**Open is transparent.** A plain low-pass (LP or LP24, no RESO, DRIVE, SAT, AUDIO FM or GAIN) with its cutoff above 9.6 kHz fades out over 14 semitones and is skipped above 21.6 kHz (the SVF is not transparent near Nyquist: the fade is in step with the cutoff, so a sweep to the top ends in the unfiltered sound without a jump; the wet level moves 0.05 a block). The filter's default is that (cutoff at the top, nothing on), so the presets that had no filter play as before; a plain filter's cutoff counts from 78 semitones at most, so a sweep down from open closes it at once. Every other filter runs exactly as it did.
**SWEEP.** An offset in semitones added to the cutoff after the envelope, key tracking and velocity, and all the matrix and knob-layer modulation: -1 to +1 is 5 octaves each way (`FLT_SWEEP_SEMIS` 60, the cutoff's own scale), the centre 0 is the preset's own cutoff (the sound is unchanged: sample for sample). Smoothed at control rate (the engine's parameter glide, about 5 ms a block at a time). Control: **hold ENV and turn K1, from any page** (not the sequencer's; ENV's page now turns when ENV is let go, not when pressed, and not if K1 was turned). Chosen over the other candidates: a held button with a K knob is the only control that is under the hand on every page, takes no macro from the presets (K1-K4 stay the macros on HOME, and on the pages when ENV is up), and HOME has no spare control (PRESET, ALGORITHM = category, SELECT = voices, K1-K4 = macros); a dedicated FILTER knob would have had to take one of them. K1 is the nearest encoder to ENV's row of buttons, as the LFO's learn gesture (LFO held, K1 picks the source) already was. Steps of 0.02 (0.1 octave), snapped to the 0.02 grid so the centre is exactly 0; the toast shows **FILTER SWEEP +1.2 OCT** (CENTRE at 0). **MIDI CC 74** (brightness) sets it: 64 the centre, 0 the bottom, 127 the top (`RCE_SWEEP`: glides). It is also the FILTER page's 16th parameter and a destination of the matrix and the knob layer.
**Saved, reset.** SAVE keeps the filter and the SWEEP in a user slot (`upatch_t.flt[16]`, 470 bytes a slot; `pad` bit 2; the older 422 and 438 byte objects are spread out as they are read, and a slot saved before has its filter (module 12 in p[1], routes into SHP) moved to the new place on load, the SWEEP at the centre, the filter open where it had none). A factory preset load resets it (rk_init). Static RAM +368 B (the slots' larger record, 256 B, and the voices' wet level).
**Checks.** All 46 factory presets rendered with `render_preset` (a chord with two macros moving, 6 s) before and after: sample for sample the same. rack_host check 10: SWEEP down and up moves the spectral centroid (INIT, SAW LEAD, MOOG BASS, HORN SECT), an open filter stays open at +0.4 (no sample differs), a matrix LFO on the cutoff still moves it at three sweep settings. `sim_sweep.txt` drives the control (hold ENV and K1, the toast, no page change, CC 74, the FILTER page, save and load in a user slot, a factory load resets it, a power cycle keeps it).

## FM1 Phase 2: no Plaits (14 Nov 2026)

Plaits is left out of this firmware (`fm1.toml`: `-DRACK_NO_PLAITS`; `make_units.py` and `build_host.sh` skip its
sources; the module picker offers Braids as the source), for room: the image went from 579,168 to 415,116 bytes of
581,632. The 54 factory presets that played Plaits now play Braids: `match_braids.py` played each one on this
computer, Plaits against every Braids model and TIMBRE/COLOR, through the whole preset, and chose the closest in
spectrum, brightness over time, loudness envelope and tonality (instrument presets keep a pitched model; FX SPEAK a
voice). The choices are in `presets_fm1_braids.json`; `presets_fm1.py` (`to_braids`) carries the rest over (Plaits'
decay envelope as Braids' AD, macros and routes on HARMO or MORPH onto COLOR), and `level_fm1.py` levelled them
again. A user preset saved with Plaits loads on Braids. The weakest matches: PD ORGAN AIR, CH DRAWBAR (ADDITIVE has
no close Braids model), FX SPEAK, CH BRIGHT EP, LD SQUARE. The benchmark firmware (`fm1/bench`) keeps Plaits.

## The factory bank of 45 (update 24; host-tested, NOT heard, NOT run on an FM-1)

The bank is INIT and 45 presets (`NPRESETS` 46; it was 125), with 15 user slots after them: 9 pads and drones (1-9), 9 keys, bells and plucks (10-18),
6 thumb pianos (19-24), 6 leads (25-30), 7 basses (31-37) and 8 textures (38-45). `presets_bank45.py` builds it (`bank45_r1.py`, `bank45_r2.py`, `bank45_r3.py`: the
presets, written on the knob layer: each of K1 to K4 drives 4 to 8 targets, gentle at 0 and glitch or wash at 1); `fit_levels.py` fits each preset's loudness (`presets_bank45_levels.json`);
`bank45_report.py` measures all of them (tuning at C2 to C5, level, K1 sweep, hands-off movement, the 16 knob corners, load estimate) and renders the demos.
The 16 granular presets and 73 others were removed (`presets_old_names.txt` is the old bank in its old order); they stay in `presets_fm1.py` and the older batch files as sources.
**Older saves.** A user slot saved before this bank holds the old bank's preset number as its base: `OLD_TO_NEW` (presets.h, made by `make_presets.py` from
`presets_old_names.txt`, the old bank in its old order) maps it to the new number where that sound is still in the bank, else to INIT; such a slot plays as it was saved
(its knob targets cleared, every setting is in the slot; `upatch_t.pad` bit 1 marks a slot already saved on the new bank). The settings record's `rsv` bit 0 does the same
for the preset last played (a factory number is mapped; a user slot keeps its slot number). The home page's knob read-outs show each factory preset's own four names (`MNAMES`).

## What it does

The FM-1's 67 factory presets (the page's 57, fitted to the FM-1's CPU, and ten of its own), from the Lite modules
(Plaits, Braids, Rings, filter, Tides, Peaks, Grids, FUNC, delay, reverb, chorus), played from the keys (with velocity)
and from USB or TRS MIDI.

Stages 1 to 4 of the menus (sound editing; save and the library; scales, chords, the arpeggiator, tunings and GLOBAL;
the sequencer and the new frame) are in: the page's own pages, buttons and knobs (`menus.c`; the parameters come from the page's definitions through
`make_ui.py` -> `ui_defs.h`).

| Button (printed) | Page |
|---|---|
| HOME | home: the preset, scope, chain, macros. Press again: the RACK overview; again: home (and all notes off) |
| EDIT | the source (Plaits or Braids), then OSC (a second oscillator and the mixer). Hold: UNDO |
| ENV | AMP (envelopes, voice mode; its well shows the envelope on every page), then the FILTER (always in the chain), then the shaper (Rings or empty). Held with K1 turned: the filter's SWEEP, on any page |
| LFO | MOD A, MOD B, then the three LFOs (and each LFO's two direct targets), then MSEQ, then the 12-row matrix |
| FX | FX 1, then FX 2. Hold, let go, then one tap: FX 1 off / on, two taps: FX 2 (engine set_fx_bypass: skipped, buffers cleared when back on; a preset load turns both on) |
| ARP | the arpeggiator, then STYLE (style_def.py: while a chord is held, three layers, each on its own MIDI channel and, if chosen, the FM-1's sound: the chord as played, held while its keys are; a bass line in the style's rhythm; the arp as it runs; play.c st_*, lay_*), LAYER MIDI CH (+ BASS FM1), CHORD + ARP (each in two stages: FM-1 and SEND, with or without STYLE; play.c lay_local, lay_send). Two quick taps: ARP FM-1 on/off; three: ARP SEND |
| SEL | scale + chord, then the keys (QNT, harmony, zone, strum). Two quick taps: CHD FM-1 on/off; three: CHD SEND (the keys always play through the chord layer, update 36). Held with EDIT (0.5 s, LEARN_MS): LEARN on / off |
| GLO | GLOBAL: settings, playing, tuning, MIDI, master, system |
| SAVE | the SAVE page: name, slot and category; SAVE again stores it (and it comes back at power-on). OCT+ the library |
| SEQ | the sequencer: NOTES, PATTERN, SPICE, GEN, BANKS, SONG in turn. Hold (0.6 s): the step layer (the 16 white keys are steps) |
| PLAY, REC | start / stop the sequencer; REC READY (stopped) or recording on / off (playing) |

| Knob | Home | A module's page, AMP, LFO | Matrix |
|---|---|---|---|
| PRESET | presets, factory then user (load when you stop turning) | the same | the same |
| ALGORITHM | category | the module picker (press the slot's button to load, HOME to close) | - |
| SELECT | voices, 1-6 | the page within | the row |
| K1-K4 | macros 1-4 (also MIDI CC 20-23) | the page's four parameters | source, destination, amount, via |
| OCT- / OCT+, MASTER | octave, volume | | |

**Save and the library** (`save.c`, after the page's SAVE): 64 user presets, 8 to a store object (A/B sectors,
torn-write safe, in the data area at 0xC8000-0xD7FFF). A user preset keeps the factory preset it started from (for
what the menus don't reach: master EQ, tuning), the six modules and their parameters, AMP, the LFOs, the 18 routes,
the macros and the tempo, a 10-character name and a category: 422 bytes.

| SAVE page | NAME + SLOT | LIBRARY (OCT+ from NAME) |
|---|---|---|
| SELECT | the letter (past the last: the library) | back to NAME (left) |
| ALGORITHM, K1 | change the letter (A-Z 0-9 - # .) | the row (PRESET too) |
| K2 / K3 / K4 | the slot / the category / the page of 16 | the tab (ALL USR BS PD LD CH FX OTH) / the action / - |
| SAVE | store it there (renaming: keep the new name) | back to NAME |
| OCT+ / OCT- | the library / - | do the action (LOAD, COPY, RENAME, DELETE: twice) / back |
| HOME | close | close |

**The wells move** (`vis.c`, after the page's visualisers): Plaits' wave by model (a field of dots for the noise models,
bursts for the drums), Braids' three stepped waves by bit depth and rate, Rings' ripples on each note, the filter's
response curve, Peaks' and Tides' outputs scrolling, Grids' three lanes with the playhead, the delay's repeats, the
reverb's cloud, the chorus' two voices, the LFOs' shapes, AMP's envelope and the voices' levels. The well redraws about
30 times a second, the rest ten. Messages show as the page's toast, a light box over the lower part of the screen.

![Save](screen-save.png) ![Library](screen-library.png)

A module's page shows the page's cells: label, value, a bar (ticks for a list), and in the slot's colour the value
modulation has moved it to. The well shows the output (AMP's first page: its envelope). Loading a module stops the
sound for a moment (the arena is laid out again) and says how many voices fit; an edit that needs a bigger buffer (a
longer delay, STR+VERB, SHIMMER) fits it again when the knobs rest ("MEMORY FITTED"). Edits are not kept when the
preset changes unless saved (SAVE). `sim_menus.txt`, `sim_save.txt`, `sim_vis.txt` and `sim_play*.txt` drive the pages in fm1-base's host simulator
(`tools/build.py ../MODFM1/firmware/fm1/rack --host`, then `build/host/fm1ph2_host sim_menus.txt OUTDIR`).

![A module's page](screen-module.png) ![The matrix](screen-matrix.png)

**After the benchmark (3 Nov 2026):** one FM-1 core played about 2 voices (`../../results/README.md`). The
second core now renders half the voices (the screen shows 2 CORES; 1 CORE* if fm1-base gave it up), and poly
presets start with their own voices (up to 6) on two cores, 4 on one, unless SAVE remembered another number (SELECT asks for up to 6;
the firmware sheds what doesn't fit). A preset that
overloads the core even at one voice (Rings' MODAL measured 102 %) is muted until you pick another, with
**TOO HEAVY FOR FM-1** on the screen, so the screen and controls keep working.

MIDI in (on GLOBAL > MIDI CH, or all): notes (through SEL and the arp, as the keys), pitch bend (± GLOBAL > BEND),
sustain (CC 64), program change (GLOBAL > PROG CHG: factory presets, then user), CC 120/123 (all off), CC 20-23
(macros), clock (GLOBAL > CLOCK MIDI: the arp's steps and the tempo). MIDI out over USB (GLOBAL > MIDI OUT, off by
default as on the page): the notes played (chords and arp included; MIDI in isn't echoed), the clock on the internal
tempo, and with +CC the macros as CC 21-24. The voice modes play as on the page: MONO, LEGATO, and UNI 2 / UNI 4 (each key plays 2 or 4 detuned
voices). If a block of sound takes over 90 % of its time, the firmware drops a voice and shows how many it shed.

![FM1 Rack's screen, from fm1-base's host simulator](screen.png)

The screen takes the page's look (`web/src/2-screen.js`): its colours and 12 x 12 module icons (`icons.h`, converted
from the page's table). A header with the source's icon, the category in a coloured tab and the preset; a well with
the output's scope over the page's dot grid; the chain as icon tiles (orange source, teal shaper, purple modulators,
blue effects; red: asked for but no room in the arena); the macros as cells with rounded bars; a footer with CPU and
RAM bars, a light per voice lit by its envelope, the cores and voice mode (SHED n when voices were dropped, HEAVY when
muted), the octave (green while keys are held) and the output meter.

**OSC and the mixer** (the firmware's own; `engine/rack.cc` osc_mix, the OSC page in `menus.c`): a plain oscillator in
each voice, mixed with the source before the shaper, so the filter or Rings, AMP and the effects take both. WAVE (SINE,
TRI, SAW, SQUARE, PULSE, NOISE; the saw and pulses band-limited with PolyBLEP), OCTAVE (-3..+2), SEMI (±12), FINE (±1
semitone); then the mixer: PW, OSC LVL (0: off, the default) and MAIN LVL (the source, 1 the default). FINE, PW and both
levels are matrix destinations (OSC.). The engine keeps them on AMP's spare parameters 24-30; a preset that leaves OSC
alone sounds exactly as before (every factory preset checked sample for sample against the engine without OSC). SAVE
keeps OSC with the user preset; stage 2's user presets load with it off. The well: two cycles of the wave at its level,
and MAIN and OSC as faders.

**LEARN** (update 35): hold SEL and EDIT together for 0.5 s (neither then taps, and EDIT's hold is no UNDO; SEL held alone does nothing). It links the macro last turned on home (on home while learning,
SELECT or K1-K4 pick M1-M4; the macro framed in gold, HOME blinking, a gold band on the pages). On a module, AMP, LFO or
OSC page, turning a parameter's knob links the macro to it in the matrix, in the row it already has or the first free
one of the 12, and the turn sets the amount ("M2>SRC.TIMBRE +24"). Tap HOME, or hold SEL + EDIT again, to finish. Home's macro cells now show what
each macro moves (its first destination and how many more) or UNASSIGNED.

![OSC](screen-osc.png) ![Macros after LEARN](screen-learn.png)

**Stage 3: playing** (`play.c`, `vis3.c`, after the page's 5b-seq.js, 5c-tools.js and 6-engine.js). SEL, GLO and ARP
are settings, not part of a preset: they stay when the preset changes and are saved on their own about 3 seconds
after the last change (the settings record grew: stage 2's preset, octave and voices carry over).

- **SEL**: SCALE (9), ROOT, CHORD (OFF, TRIAD, 7TH, 9TH, 11TH, SUS4, 6TH, POWER: the scale's own chord on each key in
  KEY harmony, a chromatic one in FREE), VOICING (CLOSE, LEAD: the inversion nearest the last chord, OPEN, INV1, INV2,
  +OCT); then QNT (SNAP down to the scale, ALL up to it, WHITE: the scale on the white keys, C4 the root), HARMONY,
  ZONE (with chords on, the seven lowest keys F3-B3 add 6, m7, M7, 9 or force DIM, MIN, MAJ; MIN + MAJ: sus) and
  STRUM (up to 80 ms between notes). The well: two octaves lighting the chord, its name (Cmaj7, Am7/G...), the scale;
  page 2 maps the FM-1's 27 keys to the notes they play. The keys in the scale glow on the keybed; SEL stays half lit
  while chords or QNT are on.
- **ARP**: MODE (UP, DOWN, UPDN, DNUP, RANDOM, ORDER, CHORD, REPEAT, UP+8, CONV, DIVG, PINKY, THUMB, TURING), RATE
  (1/4 to 1/32 with triplets), OCTAVE (1-4), GATE; HOLD (latch), MUTATE and LOOP (the Turing register), SWING. On the
  internal tempo or MIDI clock. The well: the pattern as bars by pitch, the step playing, the register's bits.
- **GLOBAL**: TEMPO, MIDI CH, VOICES (AUTO or 1-6: the same as SELECT on home), CLOCK; VEL CURVE, ANIMATION (the wells:
  full, calm, still), LED BRIGHT (the unlit buttons' glow); TUNING (13: equal, just, Pythagorean, meantone, Werckmeister
  III, Kirnberger III, rast, bayati, slendro, pelog, 19-, 24-, 31-EDO), T ROOT, A4 HZ, BEND; PROG CHG, MIDI OUT, OUT CH,
  LOCAL; LOW / MID / HIGH EQ and COMP (the master, over every preset); SYSTEM (measured: CPU history, arena, voices,
  cores). Each page's well draws it: a metronome, the velocity curve, each degree's cents against equal temperament,
  MIDI traffic, the EQ curve, the CPU graph.
- Home shows small tabs over the scope when SEL, the arp or a tuning is on.

How: the main loop pushes keys through rack_core's queue; the audio interrupt turns them into notes (`play.c`), counts
the notes sounding (two keys may share one), runs the strums and the arp by the codec's frame and applies what falls
due at the start of each block (5.8 ms). Checked in the simulator (`sim_play.txt`, `sim_play2.txt`): C5 plays C-E-G
with TRIAD, C-Eb-G with the zone's MIN key held, a strum spaces C4-E4-G4 by 35 ms, the arp steps, the settings come
back after a reboot. Not yet heard on an FM-1.

![SEL](screen-sel.png) ![ARP](screen-arp.png) ![Tuning](screen-tuning.png)

Flash: 539,548 of 581,632 bytes (about 41 KB free), RAM (.data + .bss) 77,468 of 98,304 bytes, since 8 Nov:
- the engine and the Mutable modules back at -O2 for speed (-Os was 36 KB smaller but about 15 % slower on the FM-1,
  measured 4 Nov); the menus and the platform stay at -Oz. Building rarely played units small inside the -O2 build
  (clang's minsize attribute) was tried and saved nothing measurable, so all units are -O2;
- USB audio in and out (fm1.toml usb_audio_in / usb_audio_out, update 57): the FM-1 recordable on the computer, and the computer playing through it, beside USB-MIDI;
- the splash and panel screens in the bold font (APP_BIG_FONT FONT_B), so the large and medium fonts are left out;
- Rings' chord table const (in flash, 1.4 KB of RAM); the user presets' read buffer shares its RAM with the
  sequencer's undo copy (1.5 KB; reading presets drops the undo).

**Stage 4: the frame, sliders and the sequencer** (after the HTML mockup of 4 Nov; `frame.c`, `seq.c`, `seqpages.c`):

- **The frame**: CPU and RAM sit in a 10-row status strip at the top (voice lights, cores and voice mode, CPU and RAM
  bars with numbers, octave, output meter). The modulation strip first built at the bottom was removed again (5 Nov):
  the sliders show the modulation instead, and the cells are taller.
- **Sliders with a halo** (the dials of the first stage 4 build went back to sliders, 5 Nov): a parameter the matrix
  moves glows in its source's colour: the range it has swept lately round the track, the live value filling it, the
  set value as a tick, a glow round the thumb and the cell's edge that brightens as the modulation moves.
- **Each kind of modulation has its colour**, the same on the sliders and in the matrix: macros gold, velocity green,
  key tracking sky blue, random white, envelopes coral, MOD A / B orange, LFOs violet, MSEQ pink.
- **Home**: a taller scope (84 rows), the chain, the macros as sliders; the sequencer is no longer on home.
- **Pages loop**: SELECT goes round the pages (and the matrix's rows) in both directions.
- **Contrast** (5 Nov): brighter labels, tracks and grid, a darker well, stronger halos, so they stand out on the black.
- **Holds** wait 0.6 s (SEQ's layer), so a quick press is never taken for a hold.
- **The sequencer** (a port of the page's 5d-sequencer.js): 8 banks (A-H) of up to 64 steps, each step up to 6 notes
  with their own length and force, ratchet, nudge, slide, chance; per bank length, division, swing, gate, SPICE and
  DICE; GEN's 18 tools (EUCLID, RANDOM, ACID, ARP, BERLIN ...; ALGO left undoes); BANKS (copy, clear, double, halve;
  switch now or at the loop end); SONG, 16 rows of bank, repeats and transpose. Step entry on NOTES (play a note or chord,
  release, the cursor moves on); REC records into the steps as it plays. It plays in the audio interrupt by the codec's
  frame, on the internal tempo or MIDI clock (MIDI start / stop when CLOCK is MIDI), and sends MIDI out.
- **The sequencer's pages**: its four values as sliders in a row at the top, the piano roll (or the page's picture)
  in the rest, 152 rows tall.
- **MSEQ** (LFO, after the LFOs): four lanes of 16 values on the tempo, the matrix's sources SEQ 1-4, with shapes.
- **SAVE on a sequencer page** keeps the banks and the song (store objects in the data area at 0xB6000-0xC7FFF). The
  sound stops for a moment while the flash is written, so it is never saved automatically while it plays.
- Not yet from the page: parameter locks, the free take, capture, note repeat, FILL.

Checked in the simulator (`sim_seq.txt`): step entry with a chord, playback at the tempo with MIDI out (velocity and
gate as set), MIDI start / stop sent, a modulated slider's halo moving, every sequencer page and the layer drawn; all the other
simulator scripts pass, also under AddressSanitizer. Not yet heard on an FM-1.

**Rings, shared (8 Nov 2026)**: on the FM-1 (engine/rack.cc, RACK_FM1: RINGS_SHARED) one Rings serves every voice,
as on the hardware: each voice adds its source, after AMP's envelope and Rings' EXCITE, to one input, keeps its dry part
(1 - MIX), and each new note strums the next of Rings' RINGS_POLY (2) resonators; Rings' output (times MIX and AMP's
LEVEL) joins the voices before the effects. Its parameters are read with the voice struck last. Memory: one Rings
instead of one per voice, so Rings presets fit 6 voices (they were 1-2). The first core runs Rings and the reverb a
block behind while the second renders voices, so the second takes more of them: the first's work counts as 1.85 voices
for Rings (2.2 with STR+VERB) and 1.4 for the reverb, and each core takes half of what is left (14 Nov 2026). The models MODAL, SYMPATH and SYM QNT (55-58 % of a core for one voice,
bench 4) play as STRING; the menus offer STRING, FM VOICE and STR+VERB (rc_rings_fix maps presets and user presets).

**Factory presets (Nov 2026)**: the page's 57 (`web/presets_src.py`) and, since 14 Nov, the FM-1's own set of 67 built
from them by `presets_fm1.py`: the same names, fitted to the FM-1's CPU (fewer voices where the source is heavy, Rings'
own reverb for the REVERB module, cheaper Plaits models where one sounds the same: see its docstring), FUNC in eight
(PD ANALOG, PD EVOLVING and six of the ten new ones), LFO targets, lead vibratos at 4.5-5.5 Hz (the page's were 1 Hz),
and ten new presets. Their macros start in the middle (the designed sound at 12 o'clock), velocity moves level and
tone, and some basses use OSC as a sub (`"osc"`: make_presets.py adds it as AMP 24-30). The chain, all here:

- `fm1_presets.py` turns them into engine calls (`firmware/tests/presets-fm1-calls.json`) the way the page's preset
  loader does, from the same module definitions (make_ui.load()); `fm1_presets.py --check` shows it gives every page
  preset the calls the page recorded (`tests/presets-calls.json`, record_calls.mjs), so the page isn't needed.
- `level_fm1.py` (needs EURORACK) plays them all through `audition` and sets each LEVEL toward -19 dB on the loud part
  of its phrase (`presets_fm1_levels.json`). MODAL percussion (CH MARIMBA, CH MUSIC BOX, FX CHIMES, FX GLASS RAIN)
  stays 5-8 dB under at full LEVEL: its peaks reach the limiter first.
- `make_presets.py` (and `--full`) writes presets.h; the bench's `make_cases.py` its cases from the same file.

To change a preset: edit presets_fm1.py (or presets_src.py for both), then `python3 fm1_presets.py && python3
level_fm1.py && python3 make_presets.py && python3 make_presets.py --full`, and the bench's make_cases.py (and --full).

`build/host/swclick` (built by build_host.sh) measures the clicks of each preset change (the old way, silence at once,
against the fade) and `build/host/swclick knobs` those of every continuous knob of every preset turned a quarter of its
range (at once against gliding), each against the same moment with nothing changed. Update 35, host: preset changes worst
+30.6 dB the old way, 0.0 dB with the fade; knob turns over +6 dB: 21 of 1779 at once, 1 gliding.

`build/host/audition OUTDIR` (built by build_host.sh) plays every preset through the firmware's sound path (rc_load,
rc_render at 44.1 kHz) with a phrase for its category and writes a WAV each, with its peak and loudness.

## Update 14: HOME's spectral ribbon (simulator-tested, NOT yet seen on an FM-1)

HOME's picture is now a time-frequency picture (`vis_home.c`, on the picture kit of `vis_pic.c` and `vis_clouds.c`; only HOME's picture changed): nine
ribbons, one a band of frequency (170 Hz to 13 kHz, log spaced, low at the bottom), each the filled profile of that band's energy over the last seven seconds,
dimmer with age, in one hue ramp (teal with Clouds in the patch, amber without), a bright accent "now" edge at the right with a dot at each band's newest
level. The history moves one 3 px column every 100 ms; between steps only the dabs, the now edge's dots, the dashed cutoff edge and the freeze outline move,
so the changed-rows-only transfer sends little. Layout: status strip, the preset name, the picture, the chain's process icons (small, under the ribbons),
the four macro read-outs. The granular presets (the first 16 after INIT) show their macros as TONE, MOTION, SPACE, TEXTURE (a range check on the preset
number, no preset change); the others keep their target names.

- Real: the band energies, FREEZE, the filter's cutoff as heard, the delay's time (division at the tempo, or FREE MS), FEEDBACK and MIX, the reverb's
  amount and SIZE, the LFO and modulator values (the ribbons breathe with them, at the 10 Hz of the columns), the note events (a thin bar at the band of each
  held note, from its start to its end), the output level.
- Where the spectrum comes from: the output scope (`rc_scope`, the last 1024 samples of the output, mono, 44.1 kHz: 23 ms). 23 ms resolves 43 Hz at best,
  a Hann window about twice that, so nothing below 170 Hz is shown and the lowest two bands are somewhat blurred together. Bands 0-4 (170 Hz to 1.9 kHz):
  the 1024 samples summed 8 at a time (a crude low-pass: 13 dB or more off what folds back from 8 kHz and up), Hann window, a Goertzel at two frequencies a band.
  Bands 5-8 (1.9 to 13 kHz): the newest 256 samples, Hann window, three Goertzel frequencies a band. A band's power is its largest probe's (a single probe
  under-reads a broad band of noise and a tone off the probes), level = dB re full scale from -84 to 0 with a tilt of -11 dB (lowest band) to +2 dB (highest),
  a little convex. About 5,000 multiply-adds in the main loop once a picture frame (none in the audio interrupt), a 256 byte scratch on the stack.
  Snapshots only 23 ms long, about every 50-70 ms, averaged over each 100 ms column.
- Modelled: the grain dabs. Clouds' own grains are private. Births follow DENSITY, SIZE, TEXTURE, PITCH and the level as on the Clouds page (a third of the
  rate, 14 a second at most), each dab sits at a band drawn in proportion to the live band energies (a grain plays what the buffer holds) moved by PITCH, some an
  octave up, POSITION puts it that far back in the buffer's length, SPRAY scatters it; the trail is the SIZE and the reverb's amount and SIZE; the delay's repeats are
  the dab again one delay time later (about a dab in two), fainter by FEEDBACK. In the frozen box the dabs sit in the held stretch.
- Held: FREEZE (Clouds' own flag) puts an ice-tinted outlined box round the held stretch (the Clouds buffer's length before the moment it went on, drawn
  from there to the now edge as it goes on); inside it the ribbons are pale and settle (each new column is three parts the last to one part the sound).
  The ribbons inside are the real output, not a copy of the spectrum at the moment it went on.
- Illustrative: the hue (teal or amber: Clouds in the patch or not), the shaded stripes above the cutoff, the glint round the box, the seam of a preset change
  (the new ramp enters from the now edge in about half a second; the history keeps the sound it was).
- State: the history (9 x 72 bytes) shares the Clouds page's ribbon arrays and the dabs (48 x 16 bytes) its mote field, so HOME adds only its own few dozen
  bytes (`hm`); because they are shared, HOME's history starts empty each time you come back to it from another page.
- Check: `sim_home_c.txt` (DENSE CLOUD playing a chord, FROZEN PAD with K4 up until it freezes, an older preset, a preset change caught mid-morph, silence).
  The debug read-out (hold OCT- and OCT+) works on HOME as on the Clouds page.

## Update 12: the 16 granular presets, measured and reworked (host-tested, NOT heard, NOT run on an FM-1)

`presets_granular.py` holds FROZEN PAD to GRAIN CHOIR (Braids into Clouds, K1 TONE, K2 MOTION, K3 SPACE, K4 TEXTURE); they come first in the bank after INIT, so the bank has 93 presets (it was 77). Update 15 added 16 more (positions 17 to 32, `presets_batch2.py`, K1 MORPH), so the bank had 109. Update 17 added 16 more (positions 33 to 48, `presets_batch3.py`: the everyday sounds, K1 MORPH, measured by `batch3_report.py`), so the bank now has 125.
`granular_report.py` renders each one for 16 s through the firmware's own path (`render_preset`, 44.1 kHz; K1 swept in 0-4 s, K2 4-8 s, K3 8-12 s, K4 12-16 s) and prints the measures
(peak, RMS per 4 s, centroid and RMS at the ends of each sweep with the control taken out, a clean A/B of each macro, 8 s hands-off movement, loudness against the batch median, the 16 macro corners,
an estimate of the load on each core from the owner's FM-1 figures). `python3 granular_report.py [NAME] --host-log LOG --mp3 DIR --json FILE` (LOG: rack_host's output, for the arena and voices).
`level_fm1.py --bank` sets only these presets' levels. `make_presets.py` holds 208 different floats (256 at most). Known: FROZEN PAD stays silent if K4 is above 0.8 before any note has filled Clouds' buffer.

## Update 27: CLOUDS grain controls, WINDOW SPEED JITTER (20 Nov 2026; host-tested and built, NOT heard, NOT run on an FM-1)

What was there: POSITION, SIZE, PITCH, DENSITY, TEXTURE, BLEND, SPREAD (a random pan drawn for every grain), FEEDBACK, REVERB, MODE, FREEZE, QUALITY,
IN GAIN, NOTE TRG, PITCH TRK, GRN SYNC, SPRAY (a random offset, per control block), PITCH RND, PITCH QNT, SIZE RND, SCAN (a whole-buffer scan), TONE;
all matrix destinations and knob-layer targets (NP = 32). Buffers at 32 kHz: 0.77 / 1.54 / 3.07 / 6.14 s for QUALITY 16 ST / 16 MONO / 8 ST / 8 MONO (the
8-bit qualities are stored at 16 kHz).

Added: parameters 22-24, WINDOW, SPEED, JITTER (neutral 0), read by `engine/shim/clouds/dsp/granular_sample_player.h` (a copy of Mutable's, found
first by the include path, like the Rings shims; the grain scheduler is the hook, so JITTER is per grain). `rack.cc` sets `clouds::rack_gr_*` each block.
WINDOW 0 = the old path, no extra random draw: the 46 factory presets and 8 Clouds configurations render bit-identical to 2.26. SPEED is
-2x..+2x real time (2 s |s|), forward = toward the newest audio, wrapping inside the window (kept inside the buffer), per Play call, grains placed at
their moment in the block. RND PAN is SPREAD renamed (old name kept as an alias for presets). Pages: GRAIN 1 / GRAIN 2 first, parameters keep their
numbers (`ui_mod_t.pm`, `clouds_def.LAYOUT`). Saved in user presets (474-byte record, pad bit 3; 470 and older sizes migrate). The picture draws the window
and the playhead (monitor 162-164) and now has the newest audio at the right; its comet's buffer times were wrong for QUALITY 1-3 and are corrected.
Not saved in user presets, as before: SPRAY .. TONE (parameters 16-21; the record keeps 16 a slot, and a store object has 80 spare bytes).

Cost: flash +1,692 B (image 513,952 -> 515,644 B), static RAM +64 B (free 1,820 -> 1,756 B), no arena change. CPU: a few flops a grain and one random
draw with JITTER; on the host, 0.10-0.11 ms per 10 ms with or without the controls (noise); not measured on the FM-1.
Checks: rack_host 6g (rate, direction, wrap, WINDOW 0, JITTER spread, FREEZE scrub; built with -DRACK_GR_STATS, host only), `sim_clouds_grain.txt`,
`clouds_demo --grain DIR` (two demos). rack_host check 10 fails if a frozen Clouds run is added before it, with or without this update (state it leaves
behind; not understood), so 6g runs in a forked child.

## Update 28: JAM mode, moves on the upper keys (21 Nov 2026; host-tested and built, NOT heard, NOT run on an FM-1)

Josh: "Create a jam mode where the keys trigger grain delays, fx stutters, glitches and washes. Achieved through modulating the parameters of the synth engine, fx and filters. Don't add anything new other than the controls and menus. This can go in the fx menu for now, quick access through a quick double tap." Earlier he chose a split keyboard (some keys play notes, the rest trigger moves) and each move deciding whether it holds or latches.

**Entry.** A quick double tap on FX (the second press within 0.3 s of the first let go, as SEL, ARP and GLO) toggles JAM from any page; the first tap opens its page as it always did, the second takes you back (`mn.fx_pg/fx_sub`). The hold-let go-tap gestures (FX 1 / FX 2 off and on) are the `fx_arm` path and are untouched; a press that is held 0.6 s is a hold whatever came before. JAM is also a page of the FX button (`FX_CYC` {FX 1, FX 2, JAM}, seven dots: JAM + SPLIT KEY, then two moves a page, each AMOUNT and KEY = HOLD or LATCH). `PG_JAM` is a fixed page on `jam_v[26]` (`jampage.c`), as SEL, ARP and GLO are on `set`; list texts with `opt >= 0xFF00` come from `JAM_OPT`. A gold JAM tag in the status strip (`frame.c`) on every page, brighter while a move sounds; the move's name and HOLD / LATCH ON / LATCH OFF as a toast when its key fires.

**The split.** `input()` (app.c): with JAM on, key k >= SPLIT KEY sends `PLE_JAM` (move, down or up, velocity) instead of `PLE_KEY_ON`; key-ups the queue refuses are sent again (`ui.jowed`), as the keys'. Move = (k - split) mod 12. SEL's ZONE keys (the lowest 7) win if the split is below 7. MIDI-in notes are not touched.

**Changed from the brief.** (1) **SPLIT default 15, not "the lower half"**: 27 keys, the lower half would leave 14 keys for 12 moves; 15 gives the 12 moves exactly the upper 12 keys (G#4-G5); the SPLIT is settable 1-26, with more keys than moves it wraps. (2) **The moves use what the preset has.** The brief named Clouds' controls, but Clouds is loaded in 1 of the 46 factory presets (TIDE PAD), the delay in 16, the chorus in 27 and the reverb in 45, so each move lists its targets by module and an absent module is ignored: the voices' LEVEL, the filter and its SWEEP (RESO, DRIVE), pitch, Braids' BITS and RATE, two LFO rates are in every preset. On a chorus + reverb preset GRAIN DELAY and FREEZE PAD are swells of the chorus and reverb (measured: audible but not grains), REVERSE SCRUB has no reverse (a reverb swell under an opening filter), and the stutters chop the voices and the wet instead of freezing a slice (with Clouds they also freeze and shorten the grains). Nothing was added to the sound (no extra module, no extra DSP). (3) **JAM's settings are not saved** (the settings record has a chain of older layouts to migrate; a 26-float tail was left for a later update): JAM starts off, SPLIT 15, amounts 80 (WASH and FREEZE PAD 70), all HOLD. (4) The moves are updated once per codec block (256 frames, 5.8 ms), not per engine block; the stutters' gate edges are smoothed over about 10 ms.

**The layer** (`engine/rack.cc`, "JAM mode"). A table of 32 targets lives in the firmware's flash (`JAM_TAB` in jam.c: a slot, or a module id from 30 up for the FX slot that holds it, or 255 for the voice's pitch; a parameter); the engine keeps one float offset for each (`jo_`, 128 B) and adds the sum for a (slot, parameter) in `mod_sum`, in span fractions (the pitch: octaves), as the knob layer does, before the parameter's own range clamps. `set_jam_offsets` hands over all 32 once a block while a move is on or has just ended; all zero turns the layer off (`jam_on_` false: nothing runs, `routed_` as without it), `init()` clears it. Never written into the preset's values (`base_`), the matrix, the knob layer, SAVE and UNDO see the preset's own. `jam.c` (no libm, no app globals; included by play.c and rack_host.c) holds the 12 moves: each has a level (0-1) gliding to its target at its own attack and release (8 ms to 450 ms, 60 ms to 600 ms), snapping to exactly 0 below 0.01; the amount (0-1) and `0.4 + 0.6 vel` scale it; HOLD is on while down, LATCH toggles on the press. `pl_block` calls it with the frames in a 16th (the tempo, or six MIDI-clock ticks as the arp's `step_frames`); `pl_reset` (a preset load, all notes off) ends every move.

**Moves** (targets at full amount; "-" = Clouds only where it says): GRAIN DELAY (Clouds BLEND +.5, FEEDBACK +.4, DENSITY, TEXTURE, PITCH +12 st; delay FB +.45, MIX +.35, WOW; chorus MIX/DEPTH/RATE; reverb MIX/SIZE; LFO 1 and 2 rates), STUTTER 1/8, 1/16, 1/32 (the gate on LEVEL and the reverb, delay and chorus MIX, closed in the second half of each cycle counted from the press; with Clouds FREEZE on, SIZE -.4, DENSITY +.3, BLEND gated), REVERSE SCRUB (Clouds FREEZE, SPEED -.75 = -1.1x, WINDOW +.6, BLEND +.7; reverb MIX/SIZE, SWEEP from -.6 to 0 over 8 16ths), TAPE STOP (pitch -2 octaves, SWEEP -.3, LFO rates -.35, Clouds PITCH -24 st and SPEED, delay WOW), WASH (reverb MIX +.5, SIZE +.45, SHIMMER, DAMP, SWEEP -.3, delay, chorus; Clouds REVERB, FEEDBACK), FREEZE PAD (Clouds FREEZE, WINDOW +.7, JITTER +.5, BLEND +.8; reverb MIX +.6, SIZE +.55, delay FB +.5), GLITCH (each 16th a new draw: pitch in {-12, -7, -5, 0, +5, +7, +12} st, a drop-out, BITS -2, SWEEP +-.35; Clouds POSITION, SIZE), FILTER DIVE (SWEEP -.9, RESO +.45, DRIVE +.15), FILTER RISE (SWEEP from -.85 to +.2 over 12 16ths, RESO +.35 on the way), CRUSH (BITS -4 steps, RATE -3 steps, DRIVE +.45).

**Checks.** With JAM off the 46 factory presets render sample for sample as in 2.27 (`render_preset`, 6 s, a chord, two macros moving: 46 of 46 identical). `rack_host` check 11: for WARM PAD, TIDE PAD and ARP PLUCK (the first preset with a delay), each of the 12 moves in HOLD, LATCH and "cleared while on": at least one of the 32 targets differs from the preset's own value (`eff_param` against `eff_param_nojam`, read at the same instant, so the preset's own modulation cancels) while on, and after the key is let go (or pressed again, or `jam_clear`) every target is bit-for-bit the preset's own (108 runs, 0 wrong); WASH + DIVE together add up on the SWEEP (-0.183 + -0.629 = -0.812 together); velocity 20 gives half the offset of 127; the stutters' gate cycles in 4 s: 1/16 31 at 120 bpm, 16 at 60, 62 at 240; 1/8 16; 1/32 62. The pitch target has no reader in the host build, so it is not in that check. `sim_jam.txt` (the double tap from HOME, from the JAM page and from ENV's AMP page; notes below the split, each of the 12 names on its key, HOLD, stacking, LATCH, leaving JAM with a move held, a preset change, a new split; FX's one tap and its hold-and-tap), `sim_taps.txt` and the other 41 sims pass. `fm1_presets.py --check` and `check_licence_headers.py` pass. rack_host's 7d lines (and 7g, this time one line) fail as before, 6g, 9 and 10 pass.

**Cost.** Device build (fm1-base, 21 Nov 2026): image 515,644 -> 520,876 B (+5,232 B, of 581,632), static RAM .data+.bss 96,548 -> 97,108 B of 98,304 (**free 1,756 -> 1,196 B, +560 B**: `jam_v` 104, the moves' state 144, `jam_off` 128, the engine's `jo_` 128, the stutters' gates, GLITCH's draw, flags 56). CPU: nothing while JAM is off or no move is on; with a move on, a block's work in `jam_block` (about 12 moves x 32 targets at most) and a pass over the table in `mod_sum` for each parameter a target reaches (the layer's `routed_` bits keep the others out). The demo, `/home/claude/b10/jam-moves-demo.mp3`, is TIDE PAD (the preset with Clouds) from the host simulator, measured (spectrum and level, each move against the same run without it) and not listened to.

## Update 10: the second core takes the work around Clouds (host-tested, NOT yet run on an FM-1)

The device bench (update 9's build, 4 held Braids voices) had the first core at 55.7 / 56.2 / 56.7 % with Clouds on the second core at GRANULR DENSITY 0.7 / 0.9 / 1.0,
and 55.9 (LOOPDLY) and 55.3 (SPECTRL): about **24 points** beyond its four voices (31.7) whatever Clouds did. Those 24 points were Clouds' own surroundings, which update 9
had left on the first core (`cl_front`, `cl_post`). **Attribution** (a host proxy: per-piece time of the update 9 code over 1,000 blocks in four cases (GRANULR D0.7 / D1.0, CL+REVERB, CL+REV x6; the shares given are GRANULR D0.7's, the others within a few points); this computer vectorises the filters, so the FM-1's shares may differ): 3:2 down 2 x 22 taps 26 %, 2:3 up 2 x 15 taps 47 %,
BLEND/TONE/dry delay/output stage 17 %, staging (clamp copy, 16-bit conversion, output copy) 10 %: that is ~6, ~11, ~4 and ~2-3 of the 24 points. The sum of the pieces on the host
(2.05 us of a 1 ms block, scaled by the host's four-voice cost 3.5 us = 31.7 points) comes to ~19 points, against 24 measured: the same size, and nothing else on the first core
was found (reverb/delay are separate, listed in update 9's figures).

**What moved.** All of it except two copies. The job (`cl_job`, rack.cc) now does: IN GAIN and the clamp, the 3:2 down, Clouds, the 2:3 up, the dry delay, BLEND, TONE and the
output stage (verbatim the old code, the same order: so `rack_host` 7a-7f pass with the same expectations, sample for sample). The first core copies the block that reached Clouds'
slot into the staging (`cl_front`: two 48-float copies) and the job's output into the signal. **Latency unchanged: one block (48 frames, 1.0 ms), constant, every chain order.**
Parameters are still a snapshot (Clouds' own object, `cl_igain_`, `cl_umix_`, `cl_utone_`, set between the join and the next hand-over). The job's scratch for the 32 kHz blocks is `cl_po_`
(the up filter takes all 32 frames in before it writes its 48 out, so it runs in place): no new static RAM beyond ~16 B (a few scalars) and `par_l_/par_r_` moved earlier in the file.
Expected (not measured) on the device: first core down by ~22-24 points (about 32 + the other effect), second core up by about the same (GRANULR D1.0: 42.4 -> ~66; D0.7: 16.5 -> ~40;
LOOPDLY 20.3 -> ~44; SPECTRL 11.7 -> ~35, bursty).

**Resampler: not changed.** The existing one (43 taps at 96 kHz, cutoff 14.5 kHz, Kaiser beta 6) measures (`cl_resample_check`, down then up; one filter alone in brackets, 96 kHz): droop -0.0 dB at 10 kHz,
-1.4 dB (-0.7) at 12, -8.6 (-4.3) at 14; alias level 17 kHz -30 dB, 18 kHz -40, 19 kHz -74, 20 -73, 22 -73 (the one filter: -13.9 at 16 kHz, -22.4 at 17, -35.8 at 18, -75 from 19). The brief's test (at least 60 dB
down above 16 kHz) is **not met by the existing resampler** (it never was). Cheaper designs (Kaiser, designed alike, MACs a block 2,848 now): 35 taps 2,304 MAC (droop 12 kHz -1.2 dB, 17 kHz -17.9, 18 kHz -26),
31 taps 2,080 (-1.5 / -16.0 / -22.4), 27 taps 1,760 (-2.2 / -18.1 / -24.9): each saves 8-38 % of the resamplers (about 2-9 points of the second core) and has less rejection and more droop, so none passes. The first
design that does pass (>= 60 dB from 16 kHz, 12 kHz droop -1.2 dB, 14 kHz -12 dB) is 69 taps, 4,448 MAC (+56 %): dearer. The 2 stage half-band structures need 96 kHz intermediate work and don't beat one polyphase stage at 3:2.
Not changed, so no flag and no A/B renders.

**Voices on the second core.** With Clouds in FX 1, a live effect in FX 2 and no Rings, the voices above the fourth (`RACK_FX_CORE2_V1`, 4) are rendered by the job (first in the job: they draw random
numbers (drift) as Clouds' grains do, and the deferred host stand-in then draws in the one-core order), into `par_l_/par_r_`, and added to the signal at Clouds' slot (`cl_join`): nothing is before it to miss them.
Why only then (the host proxy, `bench_host --proxy`): the first core then also carries the effect after Clouds and the second core has the room; **with Clouds alone the offload makes the second core the slower one
(GRANULR D0.9 x6: 5.5 us against 4.7 us with the voices on the first core)**, so there they stay. With an effect before Clouds (REVERB > CLOUDS, DELAY > CLOUDS) the voices would have to
wait for the job before the effect: they stay on the first core. Starting polyphony with Clouds on two cores stays 4 (SELECT asks for up to 6). `rk_fx_core2_voices(0)` (the bench's "V1" cases) turns it off at run time.
**Mixing order:** the voices' sums differ in the last bit ((v1..v4) + (v5 + v6) against ((((v1+v2)+v3)+v4)+v5)+v6; the one-core build adds in halves, too), and two cores draw the random numbers in another order
than one: 7g shows LOOPDLY (no random numbers of its own) equal to one core delayed to within 6.6e-5 (a Clouds 16-bit input step is 3e-5), GRANULR and SPECTRL the same sound to within 4.8 % rms (like 7d's real thread).
Not sample for sample, and that cannot be done for voices on another core.

**Overload guard.** `rc_overload` sheds the top voice. It now asks the engine (`shed_helps`): with Clouds on two cores and a clock set (`rk_set_ticks(plat_ticks24)` in `app_init`) the engine knows which core is the slow one
(the first core's wait at the join over 1/16 of the block, smoothed over 16 blocks: second core slow) and whether the top voice is on it (the fifth and above are the job's). A voice is shed only if it relieves the slow core.
When it does not (second core slow with no voice on it, or first core slow with the fifth and sixth on the second), nothing is shed (`rc_shed_skipped` counts the calls) and the overload goes on, audible as crackle, until
the patch changes: no new behaviour (no jump to fewer voices, no switching Clouds off). Not Clouds on two cores, or no clock: as before. 7h checks the decision with a made-up clock.

**Checks.** `rack_host` 7g (six voices) and 7h (the guard) are new; `fx_core2_tsan.sh` runs 4 and 6 voices (336 runs, `FX2_VOICES`). `bench_host --proxy "GRAN D0.7" ...` prints each core's own work on this computer.
Host proxy (microseconds of a 1 ms block, best of 6 runs; a PROXY: this computer is not the FM-1's core; core 1 = block less job): GRANULR D0.7 4.64 / 1.82 -> 3.33 / 3.09 (core 1 / core 2);
GRANULR D1.0 4.68 / 3.72 -> 3.37 / 5.10; CL+REVERB 5.80 / 2.71 -> 4.55 / 4.03; CL+REV x6 7.08 / 2.82 -> 4.52 / 5.34 (voices kept on the first core: 5.84 / 3.97).

## Update 9: Clouds on the second core (RACK_FX_CORE2; host-tested, NOT yet run on an FM-1)

With two cores running and CLOUDS in a slot, the second core runs Clouds (Prepare and Process, the heavy part) while the first core renders **every voice**
and the other effect. In update 8 the second core rendered half the voices and Clouds ran on the first (reverb + Clouds: 85.8 % on the first core).
**Switched by the compile flag `RACK_FX_CORE2`** (in both `fm1.toml`s and the host builds; `RACK_FLAGS=-URACK_FX_CORE2` on a computer builds without it).

**How.** One job a block, inside the block: `cl_fork` hands it over at the start of the block (`plat_cpu2_sync`, then `plat_cpu2_run`), the first core renders
the voices, the effects before Clouds, then at Clouds' slot `cl_join` waits (`plat_cpu2_wait`, `plat_cpu2_sync`), `cl_post` mixes, and the effects after
Clouds run on the first core. The job in block n works on the signal that reached Clouds' input in block n-1: `cl_front` staged it then (3:2 down to 32 kHz,
the dry copy, BLEND and TONE, and the parameters set into Clouds' own object). **Latency: one block (48 frames, 1.0 ms), the same for every chain order**
(CLOUDS in FX 1 or FX 2, with the delay, the reverb or the chorus before or after): Clouds' slot's output is exactly what it was on one core, a block later,
and the chain after the slot with it. The dry path is staged with the signal it belongs to, so BLEND mixes dry and wet that match. Everything upstream
of the slot (voices, modulators, an effect before Clouds) is in real time; an effect after Clouds keeps its own LFO a block ahead of its signal (the reverb's,
the chorus'): inaudible, and the only place the host checks differ from "one core, delayed" (2 % of the signal's rms at most, the chorus).

**When it is off.** Clouds not loaded, or the second core not running when the patch is laid out (`plat_cpu2_on`), or the arena gave Clouds up, or 32 kHz off
(the bench's `rk_clouds_opts`), or `rk_fx_core2(0)` (the bench's comparison cases): as update 8 (voices split, Clouds on the first core). The choice is made where
the patch is laid out (`fm1_layout`, `cl_pipe_decide`) and nothing is staged after it: the first block after a load outputs silence (the load fades out and in around it).
Once chosen it stays until the next layout: if the second core is given up meanwhile, the job is done on the first core **in the same block**
(`plat_cpu2_run` says 0: done at once, where the second core would have run it; `plat_cpu2_wait` says -1: done at the join), with no change of latency, no lost
block, and the "1C*" mark in the strip; a new patch then lays out for one core.

**What each core touches** (rack.cc, the notes at `cl_pipe_` and `cl_job`). During the job, the second core alone touches Clouds' processor object (its Parameters,
buffers and the 98,304 + 49,152 B workspace in the arena), `cl_in32_` (reads) and `cl_out32_` (writes), `st_job_`. The first core, meanwhile, renders the
voices and effects and never touches those. The parameters are handed over as a snapshot: set into Clouds' object in `run_fx` at block n-1, after the previous
job was joined, and not changed until the job of block n has been joined. Hand-over: `plat_cpu2_sync` before `plat_cpu2_run` and after `plat_cpu2_wait`
(the platform's own `run` and `wait` carry barriers too). No allocation. Outside the job the first core touches everything (the main loop, the layout and the
picture run only between blocks, with no job out: a render joins its own). **The one shared object both cores write is stmlib's `Random` state** (one 32-bit word:
the voices' notes and Clouds' grains and spectral glitches draw from it; a lost update repeats or skips a number, nothing else; the voice job already
shares it with the first core). Everything else is read-only (tables) or one core's.

**What "late" means here** (fm1-base `bsp/chip/fm1_cpu1.h`): `plat_cpu2_wait` gives a job up as late when it has waited **8 ms** (`FM1_CPU1_TIMEOUT_US`) for it,
counted from the start of the wait, not the hand-over; a job it misread (the check on function, argument and request number fails) it never ran, and a
faulted core is held. Clouds' job is under 1 ms (GRANULR at DENSITY 1.0: 71.5 % of a block measured on the first core; SPECTRL's slowest block about
twice its average), so the margin is more than 10 times, and the job is not run twice on a misread (it never started). A job given up as late (a stall of
8 ms: not Clouds' own doing) is done again on the first core: Clouds' state advances twice for that block, once. The job's stack on the host build (x86-64) goes
559 B deep (the FM-1's second core has 2,816 B); `plat_cpu2_stack()` on the FM-1 would say.

**Voices.** All the voices are on the first core, so a Clouds patch on two cores starts with **4 voices** (`app.c start_voices`; was 6 with no Clouds and 3 on one core; SELECT asks
for up to 6). No Clouds: 6 on two cores, 4 on one, as before. Arena: 4 voices fit with Clouds and the reverb (rack_host 7f).

**Strip.** The CPU % in the status strip is the time the audio interrupt takes for a block (fm1-base `main_fm1.c`), the first core's, waiting for the second included:
that is the **slower of the two cores** (the block takes as long as it). It is also what the 90 % overload guard watches (`app_overload` sheds voices, which does not help when
the second core is the slow one: Clouds at its heaviest alone on it is about 72 % by update 8's figures, so there is margin). The bench has each core's own load.

**Host checks** (`rack_host`, section 7; `fx_core2_check.c`, with `host_cpu2.h`'s stand-in for the second core, one fresh process per case, 1.5 s, four Braids voices held):
7a/7b two cores = one core delayed 48 frames, sample for sample, in every chain without a modulated effect (7 chain orders x 3 modes, steady, FREEZE on and off,
QUALITY and MODE changed while it plays; the stand-in runs the job at the hand-over, where one core runs it, so Clouds' random numbers come in the one-core
order); the reverb and chorus chains within 0.12 % / 2.4 % rms (their own LFO runs on); 7c a worker that declines or gives up jobs (injected, seeded): declined
= the same samples, given up = the same as a worker that runs its job at the join, both together = the same sound within 0.5 % rms (random order); no NaN, no
lost block; 7d a real second thread: LOOPDLY identical, GRANULR and SPECTRL within 7 % rms (the random numbers' order depends on the threads); 7e Clouds off
and on from the panel, taken out and back, the patch reloaded, while playing: no NaN, largest step within 7 % of one core's own, a gap where Clouds starts
up one block longer; 7f four voices fit and play. `fx_core2_tsan.sh`: 168 runs under ThreadSanitizer with the thread stand-in. **Changed expectations**:
none of the older checks (they run with no worker); the new ones above expect one block of delay and 4 starting voices on two cores with Clouds.

## Update 8: Clouds costs less (16 Nov 2026; host-tested, NOT yet run on an FM-1)

The owner's FM-1 measured (% of one core at 48 kHz, 4 held Braids voices): Braids x4 31.0; Clouds' own share in GRANULR 16 ST 28.2 at DENSITY 0.7,
46.6 at 0.9, 74.3 at 1.0; LOOPDLY 33.9; SPECTRL 21.6 (bursty); 8-bit qualities 14-19 more; Clouds + reverb 95.4 (one core), 84.0 (two). Five changes,
each behind its own compile flag, ON in the FM-1 builds (`fm1.toml`, `build_host.sh`), each switched off by `-U` (host: `RACK_FLAGS=-URACK_CLOUDS_32K`).

1. **`RACK_CLOUDS_32K`: Clouds at 32 kHz**, its design rate. The engine block (48 frames at 48 kHz) is exactly Clouds' own block (32 frames at 32 kHz): the
   stereo input goes down 3:2, Clouds runs ONCE per block (was twice, 24 frames), its all-wet output goes up 2:3. Resampler: one Kaiser-windowed sinc, 43 taps at
   96 kHz, cut into polyphase rows (down 22 taps, up 15; ~2,800 multiply-adds a block), cutoff 14.5 kHz: -0.7 dB at 12 kHz, -4 dB at 14, -22 at 17, -36 at 18,
   -75 from 19 (`engine/cl_resample.h`, written by `make_cl_resample.py`; `cl_resample_check.cc` measures it: gain, error, block-edge steps, alias levels).
   Latency of the wet path 0.45 ms (21.5 samples at 48 kHz). **The dry path stays at 48 kHz, not filtered, only delayed by 21 samples to meet the wet**: Clouds
   runs all wet and BLEND (smoothed across the block), the TONE filter and the output stage are done in the engine at 48 kHz (`run_fx`). Pitch, PITCH TRK, GRN SYNC
   (the tempo grid is by the block), SPRAY, SCAN and the comet's time base are unchanged; what changes is in seconds: buffers, grains, the loop and Clouds' own
   reverb last 1.5 x longer (the same samples at 2/3 the rate; QUALITY buffer lengths in seconds above), the wet loses what is above ~14 kHz, and Clouds' own
   filters (one-pole and reverb damping, in samples) sit 1/3 lower in Hz, as Mutable tuned them. Runtime switch (the bench only): `set_clouds_opts`.
2. **`RACK_CLOUDS_MAX_GRAINS` (default 40): a grain ceiling.** NOT what the brief assumed: Clouds' player does not run 64 grains. 64 is its array size; its count is
   32 (stereo 16-bit), 45 (stereo 8-bit), 40 (mono 16-bit), 57 (mono 8-bit). The ceiling (a changed copy of `granular_processor.cc`, written by `make_units.py`) therefore
   changes nothing at 16 ST (32), takes 8 ST from 45 to 40 and 8 MONO from 57 to 40; 64 and 48 never act, 32 acts on none of 16 ST. The knob range is unchanged. The number is the
   owner's to tune by ear: below 32 it does act on 16 ST (24: -10 % RMS at DENSITY 1.0, 16: -21 %); the host tables are in the report.
3. **`RACK_CLOUDS_SKIP_REVERB`**: Clouds' internal reverb is not run while its amount is exactly 0 (REVERB 0 and no FREEZE with FEEDBACK) and has been for 256 blocks
   (0.13-0.26 s; until then it runs at amount 0, which changes nothing). Coming back, its memory is cleared and the amount rises over 24 blocks (no stale tail, no step).
   Sample-identical to the unskipped path at REVERB 0: `clouds_skip_check.sh` (Clouds alone, hashes of every mode and quality, equal).
4. **Three voices with Clouds**: a poly patch with Clouds in a slot starts with 3 voices on one core (SELECT asks for more; two cores: as before, 6); picking Clouds
   lowers a poly patch to 3 (unless SELECT's number is kept). `RACK_CLOUDS_VOICES` (the voices the reverb is protected for when memory is short) is 3 to match, not 4: with Rings, Clouds and the
   plate reverb 3 voices fit, so the reverb now stays (it left when 4 were asked). Host checks 6c changed to match (clouds_check.c, commented).
5. **Lighter picture, same look** (`vis_clouds.c`): at most 20 pictures a second (was ~33); only the ROWS THAT CHANGED are sent to the LCD (a hash of each of the 230 rows;
   runs of changed rows within 3 rows are one transfer); the picture is composed whole as before. The comet's buffer time follows the 32 kHz length.
   Why: update 7's read-out on the FM-1 said ~24-29 ms a frame (against 130-160 us on the host). The LCD path (fm1-base `device/lcd.c`) sends a canvas as one
   DMA transfer of w x h x 2 bytes and the next `cv_begin` waits for it (`lcd_sync`); a full frame is 230 rows x 480 B = 110 KB, in 6 transfers. The compose is in
   microseconds; the transfer and the wait for it are the frame. The platform has no call to exclude interrupt time: the draw timer is the main loop's wall time and
   includes the audio interrupt running over it (at 80-95 % audio load the main loop gets the rest), so DRAW is not the picture's own CPU. The status strip's CPU %
   (`plat_cpu_pct`, fm1-base `device/main_fm1.c`) is the audio render's time in its interrupt over its budget, smoothed over ~16 blocks: it does NOT include drawing.
   **The debug read-out** (OCT- and OCT+ held together, as before) is now three lines in the big number font, a one-second average, refreshed once a second:
   `DRAW 24.1 MS` (a whole frame: compose + LCD), `LCD 19.8 MS` (the canvas clears, which wait for the last transfer, and the transfers) and `FPS 15`; compose = DRAW - LCD.
   Not done: a USB-MIDI or print channel for these numbers (none cheap exists); skipping the compose of empty bands (compose is microseconds; the hash skips their transfer).

Checks: `rack_host` (6d: the reverb switched 0 -> 0.5 -> 0, no click; 6e: pitch right at 32 kHz; 6b/6c), `clouds_asan.sh` (32-frame blocks, exact-size buffers), `clouds_skip_check.sh`,
`cl_resample_check.cc`, `clouds_demo --cost2` (before/after on the host: a proxy), the bench's comparison cases (bench README), sim_clouds_pic.txt (rows and transfers a frame on stderr).

## Update 7: the CLOUDS picture (15 Nov 2026; simulator-tested, NOT yet seen on an FM-1)

The CLOUDS page is "picture first": the picture fills the screen from under the status strip to a 40 px band at the bottom
that holds the page's four read-outs (name, small arc, value, in the four slider colours; the one being turned grows and
brightens for about a second). The mode name is small at top left, the page dots small at top right. SELECT still pages
through Clouds' six parameter pages; the other modules' pages keep their layout. Code: `vis_clouds.c` (part of `app.c`),
its const tables `clouds_sprites.h` (generated by `make_clouds_sprites.py`, in flash).

- **GRANULR:** the buffer as a ribbon across the lower third, the write head as a comet moving at the buffer's real length
  for the QUALITY (0.51 s at 16 ST, 0.93 s 16 MONO, 1.02 s 8 ST, 1.85 s 8 MONO at 48 kHz; 1.5 x as long since update 8's 32 kHz Clouds: 0.77, 1.40, 1.53, 2.78 s; SPECTRL plays mono), POSITION as a thin
  thread and SPRAY as a soft halo round it, grains as glowing motes (count from DENSITY, none at 0.47-0.53; size from SIZE;
  hue from PITCH, warm up and rising, cool down and sinking; softness from TEXTURE; brightness from the output level, so
  silence is a still, dim picture). FREEZE: the ribbon turns ice blue and stops being written (as the buffer does), the
  comet stops as a hollow ring, the motes hang and shimmer, a small FROZEN mark.
- **LOOPDLY:** the loop as an orbit ring, the lit arc = SIZE (starting at POSITION), a playhead circling at the loop's
  length, colour and tilt from PITCH, bristles from the level. **SPECTRL:** an aurora of 24 soft curtains.
- STRETCH is not in this build (RACK_CLOUDS_NO_STRETCH): nothing was drawn for it.

**Real, modelled, illustrative.** Real: the parameters as the engine hears them (`rk_monitor`, with the matrix's
modulation), the output peak (every brightness, the ribbon's heights), new notes (a burst of grains), the buffer lengths,
FREEZE. Modelled: grain births (rate, place, size, hue, speed are drawn from DENSITY, POSITION, SPRAY, SIZE, PITCH,
TEXTURE: the true grain positions are private members of Mutable's `GranularSamplePlayer`, and reading them would need a
patch of the Mutable code generated at build time like `make_units.py`'s Rings patch; not done). **The SPECTRL
picture is illustrative: Clouds' FFT magnitudes are not read** (that too needs a patch); the curtains follow TEXTURE
(where), SIZE (how wide), DENSITY (shimmer) and the level. The ribbon shows the master output peak, not Clouds' own
input or output, and a small column-to-column texture on it is cosmetic (it only ever scales the real level).

**Cost.** Drawing runs in the main loop (`draw()` and `draw_well_only()`); the sound is rendered by `app_render` from the audio
interrupt (fm1-base's plat.h, docs/SECOND-CORE.md), which preempts the main loop, so a slow frame makes the screen and
the buttons late, not the audio. A dropout can come only indirectly: the picture's LCD transfers sharing the bus
(not measured here), or the render already needing nearly all of a core at that moment (then the CPU figure shows it, with or
without the picture). No floating point or maths
function runs per pixel: the glows are const sprites added with saturation into RGB565; at most 48 motes; the picture is
drawn at most about 30 times a second, and not at all while the sound is silent and nothing on it changes (the read-outs
still draw). Five bands of at most 40 rows, then the read-outs: all rows are redrawn when it draws (the platform sends a canvas
a band at a time; there is no per-row dirty flag).

**Measuring the draw time on the FM-1:** on the CLOUDS page hold **OCT- and OCT+ together** (their octave steps cancel): a
toast says DRAW TIME ON, and the picture shows `DRAW n US` (the last frame) and `MAX n` (the worst since the read-out
was turned on) at its bottom left, in microseconds from the platform's timer (`plat_ticks24`), taken over the whole
picture including its LCD transfers. Hold them again to turn it off. Read it with the status strip's CPU, and XRUNS.
On the host simulator the same figure is a proxy only (an x86 core, and the simulated screen is a memory copy); the
simulator prints its mean to stderr.

`python3 make_clouds_anim.py OUTDIR --host build/host/fm1ph2_host` plays the long scripted run on the simulator (one screenshot every 80 ms) for the video.
`sim_clouds_pic.txt` is the short one in the simulator checks.

## Updates 44-45 (space at no cost to the sound)

- rc_buf and the engine's out_l_ / out_r_ 512 frames (were 1024; a block needs ~280, the bench 480): 8 KB RAM.
- PRESET_OPS: each float is a byte, its place in PRESET_F (164 numbers, bit for bit; make_presets.py): 23,159 ->
  13,586 B + 656. FONT_S not linked (APP_SYS_TEXT: the platform's screens in FONT_XS / FONT_B): 8.6 KB.
  Image 577,660 -> 559,708 B. rack_host: every preset sample for sample as before.
- Overload: the next voice shed after 8 blocks (46 ms; was 20, 116 ms).

## Update 43

- STRUM spreads the sequencer's chord steps as it does the keys' (seq.c sq_step, note j at j x STRUM). FEEL > HUMAN
  (A_HUM, SEL page 6 via data_pg; settings RKL7, ARP 21 values): each chord note up to 35 ms late and +-12 % force at
  random (play.c human_ms, human_vel), live and in the sequencer. sim_feel.

## Update 15: the picture family (option C)

Every page that had a picture is redrawn in the language of the HOME ribbons: one hero object on near-black, depth (slices drawn back to front, each with a dark underside that hides the lines behind it), the real signal in fine 1 px lines or filled gradient profiles, one hue ramp per process family, one accent for "now". The kit is `vis_fam.c` (terrain, filled curve, dashed guide, accent node) on top of `vis_pic.c`; history (a slice every 80 to 150 ms) is kept in the one shared state block (`cp.tr`, on the ribbon arrays: no new RAM beyond about 100 bytes). Pictures are shown at most 12.5 times a second, composed with plain writes, only changed rows go to the LCD.

Hue ramps: teal = effects (Filter, Delay, Reverb, Chorus, Clouds, HOME with Clouds); amber = sources and the voice (Braids, Rings, AMP); indigo = modulators (LFO, Tides, Peaks, Func); violet = rhythm (Grids); ice = a frozen Clouds (and links). Accent: each ramp's own.

Real, modelled, illustrative, per page:
- SOURCE (Braids): real: MODEL, TIMBRE, COLOR, the voices' envelopes and notes (height, pitch, motion); modelled: the waveform shape of each model family and how TIMBRE morphs it (not Braids' own output).
- RINGS: real: strikes (gates), STRUCTURE, BRIGHTNESS, DAMPING, POSITION; modelled: each mode's energy, decay and phase.
- FILTER: real: TYPE, CUTOFF, RESO, envelope amount and the voices' envelopes (the response is the filter's own formula), the 9-band spectrum of the output behind it (the HOME spectrum step, 170 Hz to 13 kHz); modelled: none.
- DELAY: real: FEEDBACK, MIX, the output level history (the profile); modelled: the delay time (division at the tempo or FREE MS) and each repeat as the level history delayed by that many delay times, scaled by FEEDBACK^n.
- REVERB: real: SIZE, DAMP, DIFFUSE, MIX, the output level, the notes' times; modelled: the tail (held level, let go at SIZE's rate), the arches (a diagram of depth, not the reverb's own signal), the wavefront.
- CLOUDS: real: POSITION, SPRAY, SIZE, MODE, FREEZE, the output level written into the buffer (the terrain's slices), the loop head in LOOPDLY; modelled: grain sparks (rate from DENSITY); illustrative: the grain texture on the slices (random, more of it away from DENSITY's middle) and SPECTRL's curtains.
- LFO, TIDES, PEAKS, FUNC: real: the modulator's output, 114 samples 33 ms apart (vz.hist), shown as slices of that history, the node at the newest sample; modelled: the trigger sparks (Peaks, Func). Only the first output of Tides and Func is drawn.
- GRIDS: real: the 96 step levels, the clock step, the triggers; illustrative: the perspective of the lanes.
- CHORUS: real: RATE, DEPTH, WIDTH, MIX, the level; modelled: the voices' lines (the chorus's own LFO is not exported).
- AMP: real: ATTACK, DECAY, SUSTAIN, RELEASE, each voice's envelope value and gate; modelled: which stage a node is in (from its gate and whether it rises).
- HOME: as update 14; the macro read-out names come from a const table (presets 17 to 32 show MORPH, MOTION, SPACE, TEXTURE).
- MATRIX (routes): the table is as before; each row has a fine link line from its source to its destination, brighter the more amount (static: the page is drawn when it changes). RACK: connectors between the windows. Not done (old look kept): the sequencer, arp, scale/chord, save, menus, settings, info, OSC, the MOD sub-pages, by-target matrix.

## Update 40

- GLOBAL page 7 CLICK (G_CLICK, G_CIN, G_CVOL, G_CSND; settings RKL6, glo[23], older records moved up): seq.c
  click_render mixes a decaying sine per beat after the engine, before MASTER. REC READY + PLAY: count-in (clk.cin),
  then sq.req = 1 on the next downbeat; a first note while counting in doesn't start it.
- The lasso as a group: K3 lengths, K4 dynamics, SEL + K4 articulations (seq_lasso_each), as well as K1 / K2.
- Buttons: a press is taken from the frame's edge or the render's count, whichever comes first, once (a late count had
  undone SEL + K4 and turned SEL's release into a tap). sim_click, sim_lasso40.

## Update 39

- Sequencer: SELECT turns its pages (PATTERN / SPICE cursor on ALGORITHM, BANKS / SONG on K1). NOTES: K4 DYNAMICS
  PP-FFF (lvl bits 0-2), SEL held + K4 the articulation (bits 4-6: staccato, tenuto, accent, marcato; seq.c plays
  them: length x0.5 / full +, +1 / +2 dynamics, x0.75). Only EDIT deletes. A lasso moves: K1 steps, K2 semitones.
- Note names by key signature (vis3.c key_sharps; flats '\x01' drawn by cv_text_acc).
- HOME: no scope; a 16-row tab strip (home_strip) and the macros redrawn only when they change.

## Update 38

- Buttons: presses are counted by the render (btn_count, every 5.8 ms) and input() takes each once; ms_stamp()
  replaces plat_ms() | 1, which put half the presses a millisecond in the future so a same-frame hold test took the
  tap for a hold. SEL / ARP / LFO: a single tap acts at once; two or three quick taps go back to the page you were on
  and toggle.
- SEL: two taps CHORD type off / back (single notes still play), three taps CHD SEND. SEL pages 3-5 show ARP's
  settings 8-19 (data_pg): STYLE, LAYER MIDI CH, CHORD + ARP; ARP has 2 pages. RKL5: CHD FM-1 turned on once.
- ZONE: CHORDS (0), OFF (1), BASS (2, make_ui.py): with a STYLE, the 7 lowest keys pick the bass's chord tone
  (R 3 5 7 8 b a, play.c zone_bass_tone) in the style's rhythm.
- Sequencer NOTES: EDIT deletes the picked note or the lasso (hold SEL + K1); hold SAVE copies the lasso / pastes;
  the roll has semitone lanes, a see-through keyboard (RX) and wider notes. sim_u38, sim_zonebass, sim_seqedit.

## The look

The pictures (each page's well, HOME's tiles, the STYLE and layer pages) are drawn through vis.c's vm_* wrappers in a
fine look (update 37): a bar's narrow side halved, shapes laid over what's under them at about 83 % (lines 91 %), each
with a 1 px halo of its own colour at about 27 %. vf_keep 1 keeps boxes with words in them their size (the layer
boxes, HOME's tabs); 2 draws as before (the SEL keyboards). HOME shows a red KEYS / ARP: FM-1 OFF tab while either is
off the FM-1's sound.

## How it works

- `rack_core.c`: presets (INIT, then the preset's differences, then each module's parameter ranges, packed in
  `presets.h` by `make_presets.py`: 22 KB), a note queue from the main loop to the audio interrupt, the voice modes,
  the overload shedding, and the resampler: the engine renders 48 kHz, the codec plays 44.1 kHz, so the output is
  resampled (linear) each block. Loading a preset, swapping a module or refitting the memory (rc_fade_pause) first has
  the audio interrupt fade its next block to silence (the main loop waits for it, 30 ms at most), then play silence while
  the main loop loads, and the new sound fades in over 10 ms; the engine's init leaves nothing of the old patch in its
  block buffer or the master's filters, compressor and limiter. A knob turned on a continuous parameter glides
  (rc_set_glide, the engine's set_param_glide: about 5 ms); macros glide too; lists and steps change at once.
- `app.c`: the screen and controls, on fm1-base (the template's patterns).
- `../common/rk.cc`, `../common/rt.cc`: the engine's C names, and the C++ runtime and maths fm1-base lacks (as in
  `fm1/bench`).

## Checked

`sh build_host.sh && build/host/rack_host` (needs `EURORACK`): every preset loaded as the firmware loads it sounds
the same as the preset's own calls, sample for sample; the resampler keeps the pitch and has no step at block
edges; every preset makes sound through the firmware's audio path; the unison modes play 2 and 4 notes per key.

fm1-base's build (with placeholder SDK files here, so its package was deleted): image 544 KB of the 568 KB app area,
no soft-double routines, pool within its headroom.

## Memory: the big buffers sized to the preset

The arena is 304 KB (what fm1-base's pool leaves, less the USB recording's ring since update 58). After a preset loads, `rk_fm1_fit` (rack.cc `fm1_fit`) sizes the
big buffers to what the preset can use and lays the arena out again, so the room left goes to voices:

- **the delay line** to the preset's delay time (plus TAPE's wow), not 1 s; the full second when a matrix row can
  move its time;
- **the reverb's pre-delay lines** to the preset's pre-delay (a power of two), not 170 ms; **the shimmer line** only
  when SHIMMER is reachable;
- **Rings' own reverb** (64 KB a voice) only when STR+VERB is reachable (the shim's `rings/dsp/fx/fx_engine.h`
  leaves a null buffer alone; render_voice keeps the model off STR+VERB then).

These four change nothing in the sound: with the same voices, every preset comes out sample for sample as before
(one, CH DUSTY KEYS, differs by 1e-5 at most: TAPE's wow reads a shorter line at a float position).

- **Rings' strings in 16-bit** (`-DRACK_RINGS16`, the shim's `rings/dsp/string.h`): 8 strings of float delay lines
  were 99.6 KB a voice, now 49.8 KB. This one changes the sound slightly: on the computer, against the float strings,
  the difference is 55-76 dB below the signal for the Rings presets, and their decays end at the same level (no
  hanging tails). Full scale is +/-4 (the strings peaked at 2.05).

Voices the arena now fits (computer check; the CPU may still shed some on the FM-1): LD RESO STRING and PD SYMPATH 1 to 3; KARPLUS gets its Rings
back (2 voices; before, its delay left no room and Rings was unloaded); PD SHIMMER 1 to 5; PD GLASS AIR and BS DUB
ROUND 1 to 4; PD DARK DRONE and FX TIDAL DRONE 1 to 3 (their delay is a full second); LD SAW SOLO, LD SYNC SCRM, LD CHIP
SQ, LD PWM SOLO, LD SQUARE CHIP, PD EVOLVING WT, CH PLUCK HARP, CH HOUSE STAB, CH STRING PLK, FX RISER, FX LASER ZAP, FX SAMPLEHOLD, FX NOISE SWEEP,
FX METAL RING, GRID GATE and TIDAL FM to 6.

The page's FM-1 preview (GLOBAL > HARDWARE) still uses the full sizes, so it now predicts fewer voices than the
firmware plays.

## Build and install

As `fm1/bench` (its README), with this folder: `tools/build.py ../MODFM1/firmware/fm1/rack`, then
`python3 tools/fm1_install.py ../MODFM1/firmware/fm1/rack/build/fm1ph2.fwsc`. Identity `FM-1_7800`
(`id_prefix` picked for this project, not registered anywhere).
