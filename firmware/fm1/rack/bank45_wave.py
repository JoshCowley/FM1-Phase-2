# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""The six wavetable presets of the factory bank (2.48: WAVE, Plaits' wavetable engine as a source), each in the place of an older preset
it was too much like. The bank stays at 45 (+ INIT). See presets_bank45.py for the knob grammar (gentle first, glitch or wash at the top).

The banks as the engine has them (z = BANK x 3, X = SCAN X x 7, Y = MORPH Y x 7; measured on the wave data, spectral centroid in harmonics):
  BANK 0 (0.00)  y 0-2 a single harmonic per step (x = its number): clean additive tones; y 3 a saw-like series, brighter to the right (1.5 -> 16);
                 y 4-7 fundamentals with more and more partials
  BANK 1 (0.33)  y 0-1 buzzy, y 2-5 a sweep from soft to bright along x (centroid 4 -> 25), y 6-7 fundamental-heavy and brightening to the right
  BANK 2 (0.67)  y 0-1 hollow, wide and nasal; y 2-5 vocal and reedy shapes; y 6-7 (x 3-7) near-sine tines (centroid 1 -> 4), x 0-2 bright
  BANK 3 (1.00)  the same 192 waves in Plaits' fixed shuffle: neighbours have nothing to do with each other, so a sweep in it is a glitch"""
from presets_bank45 import newp
from presets_batch2 import CHO, LF, AM
from presets_batch3 import FL, TD
from presets_granular import REV, DLY

N = newp
RND = [["RANDOM", "PITCH", 0.0]]


def wave_presets():
    W = {}

    # PAD --------------------------------------------------------------------------------------------------------------- (SLOW STRINGS)
    W["WAVE PAD"] = N("WAVE PAD", "PAD", {"SRC": "WAVE", "SHP": "FILTER", "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"BANK": 0.34, "SCAN X": 0.3, "MORPH Y": 0.45, "DRIFT": 0.2, "LEVEL": 1.0},
         "SHP": FL("LP", 0.55, 0.1, 0.1, 0.0, 0.5, 0.1),
         "MODA": TD("LOOP", 0.15, 0.5),
         "FX1": CHO(0.4, "CHORUS", 0.2, 0.4, 0.9), "FX2": REV("HALL", 0.8, 0.3, SHIMMER=0.1)},
        {"ATTACK": 0.4, "DECAY": 0.5, "SUSTAIN": 0.9, "RELEASE": 0.6, "VEL AMT": 0.2, "SPREAD": 0.7, "VINTAGE": 0.2},
        k=[("TONE", "A dark, round wave at 0, the scan opening to a bright, buzzing one by the middle; the top is hard, bright and stepping from wave to wave.",
            "SRC.SCAN X .5 0 .85 L; SHP.CUTOFF .3 0 1 L; SRC.MORPH Y .25 .35 1 SL; SRC.QUANT 1 .93 1 T"),
           ("MOTION", "The Tides loop and the LFOs speed up and the scan wanders further; the top is a restless, fast-scanning pad.",
            "LFO.RATE 1 .4 .3 1 SL; ROW 13 .25 .4 1 SL; ROW 14 .25 .4 1 SL; LFO.RATE 2 .4 .4 1 SL; ROW 16 .2 .5 1 SL; MOD A.FREQ .3 .3 1 SL; ROW 18 .3 .6 1 SL"),
           ("SPACE", "A close pad at 0, the chorus widens and a hall opens; past 65% the shimmer rises and the chorus feeds back, so the tail runs on.",
            "FX 1.MIX .3 .1 .6 L; FX 2.MIX .3 0 .6 L; FX 2.SIZE .2 .3 1 L; FX 2.SHIMMER .45 .6 1 SL; FX 1.FEEDBACK .4 .65 1 SL; AMP.RELEASE .3 .5 1 S"),
           ("GRIT", "Resonance and drive rise gently; near the top a per-note pitch scatter, a bit-crush and the shuffled bank break the pad into pieces.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.4 .85 1 T; SRC.BANK .6 .75 1 SL; SHP.RESO .3 .5 1 SL; SHP.DRIVE .25 .5 1 SL; SRC.DRIFT .4 .3 1 L")],
        lfo=LF(("SINE", 0.2), ("DRIFT", 0.25), ("TRI", 0.1)),
        lfo_tgt=[("SRC.SCAN X", 0.04), ("SRC.MORPH Y", 0.03), ("PAN", 0.1), ("SHP.CUTOFF", 0.04), ("PITCH", 0.002), ("AMP.LEVEL", 0.0)],
        rows=RND + [["A TIDES", "SRC.SCAN X", 0.2, "M2"], ["A TIDE+", "SHP.CUTOFF", 0.25, "M2"]],
        macros=(0.4, 0.3, 0.3, 0.2), voices=6, level=0.4, move="medium", kind="",
        summary="A wavetable pad: a bank-1 sweep from soft to bright scanned slowly by a Tides loop and two LFOs, a chorus and a hall.")

    # KEYS -------------------------------------------------------------------------------------------------------------- (BOX KALIMBA)
    W["WAVE KEYS"] = N("WAVE KEYS", "KEYS", {"SRC": "WAVE", "SHP": "FILTER", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"BANK": 0.667, "SCAN X": 0.8, "MORPH Y": 0.9, "DRIFT": 0.05, "AD ATK": 0.02, "AD DEC": 0.3, "AD>X": -0.4, "LEVEL": 1.0},
         "SHP": FL("LP", 0.7, 0.05, 0.05, 0.0, 0.6, 0.3),
         "FX1": CHO(0.25, "CHORUS", 0.2, 0.3, 0.8), "FX2": REV("PLATE", 0.4, 0.15)},
        {"ATTACK": 0.0, "DECAY": 0.65, "SUSTAIN": 0.05, "RELEASE": 0.3, "VEL AMT": 0.6},
        k=[("STRIKE", "A soft, round tine at 0; the envelope throws the scan across the bank by the middle for a bright, metallic strike; the top is hard and clanging.",
            "SRC.AD>X -.35 0 1 L; SHP.CUTOFF .3 0 1 L; SRC.MORPH Y -.35 .5 1 SL; SRC.AD DEC .2 .3 1 L; SHP.AUDIO FM .4 .65 1 SL"),
           ("MOTION", "A tremolo and a pan sweep grow with the speed; the top is a fast, uneven wobble with the scan moving under each ring.",
            "LFO.RATE 1 .4 .3 1 SL; ROW 13 .3 .3 1 SL; ROW 15 .2 .4 1 SL; ROW 17 .4 .5 1 SL; LFO.RATE 3 .5 .45 1 SL; ROW 18 .1 .6 1 SL"),
           ("HALO", "Chorus and plate grow around the tine; past 65% the chorus feeds back and the ring runs on into a wash.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .4 .5 1 S; FX 2.SIZE .15 .5 1 L; FX 2.MIX .3 0 .5 L; FX 1.MIX .3 .3 1 L"),
           ("RUST", "Drive and a little resonance, then a per-note pitch scatter, a bit-crush and the shuffled bank near the top: a cracked, out-of-tune tine.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .85 1 T; SRC.BANK .4 .7 1 SL; SRC.SIGN .5 .7 1 SL; SRC.DRIFT .4 .3 1 L")],
        lfo=LF(("SINE", 0.3), ("DRIFT", 0.2), ("TRI", 0.2)),
        lfo_tgt=[("PAN", 0.0), ("SRC.MORPH Y", 0.0), ("SHP.CUTOFF", 0.0), ("SRC.SCAN X", 0.0), ("AMP.LEVEL", 0.0), ("PITCH", 0.0)],
        rows=RND, macros=(0.4, 0.3, 0.4, 0.3), voices=6, level=0.45, move="subtle", kind="arp",
        summary="A wavetable electric piano: a near-sine tine in bank 2 that the AD envelope strikes bright from the side of the map and settles, a chorus and a plate.")

    # LEAD -------------------------------------------------------------------------------------------------------------- (HOLLOW LEAD)
    W["WAVE LEAD"] = N("WAVE LEAD", "LEAD", {"SRC": "WAVE", "SHP": "FILTER", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"BANK": 0.0, "SCAN X": 0.45, "MORPH Y": 0.43, "DRIFT": 0.1, "AD ATK": 0.02, "AD DEC": 0.5, "AD>X": 0.25, "LEVEL": 1.0},
         "SHP": FL("LP", 0.7, 0.15, 0.1, 0.0, 0.6, 0.2),
         "FX1": DLY("1/8D", 0.3, 0.2, "PING"), "FX2": REV("HALL", 0.55, 0.2)},
        {"ATTACK": 0.01, "DECAY": 0.4, "SUSTAIN": 0.85, "RELEASE": 0.18, "VEL AMT": 0.45, "GLIDE": 0.15, "MODE": "LEGATO"},
        k=[("BITE", "A soft, almost pure tone at 0, the scan opening through the saw-like row to a bright, reedy lead; the top is hard and buzzing.",
            "SRC.SCAN X .4 0 .9 L; SHP.CUTOFF .25 .1 1 L; SRC.AD>X .3 .1 1 L; SHP.AUDIO FM .4 .65 1 SL"),
           ("VIBRATO", "A slow vibrato fades in and grows with the speed, the scan wobbling with it; the top is a fast, shaking wobble.",
            "ROW 17 .02 .3 1 SL; LFO.RATE 3 .4 .4 1 SL; ROW 13 .25 .4 1 SL; LFO.RATE 1 .4 .4 1 SL; ROW 14 .2 .5 1 SL"),
           ("SPACE", "A dry line at 0, a ping-pong delay and a hall behind it; past 65% the delay feeds back into a wash.",
            "FX 1.FEEDBACK .45 .6 1 SL; AMP.RELEASE .4 .5 1 S; FX 2.SIZE .25 .4 1 L; FX 1.MIX .3 0 .5 L; FX 2.MIX .2 .3 1 L"),
           ("CRUSH", "Resonance and drift, then a per-note pitch scatter, a bit-crush, stepping and a slide into other rows near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.5 .7 1 T; SRC.MORPH Y .35 .55 1 SL; SRC.QUANT 1 .9 1 T; SRC.DRIFT .4 .3 1 L")],
        lfo=LF(("SINE", 0.3), ("DRIFT", 0.2), ("TRI", 0.25)),
        lfo_tgt=[("SRC.SCAN X", 0.0), ("SRC.MORPH Y", 0.0), ("SHP.CUTOFF", 0.0), ("PAN", 0.05), ("PITCH", 0.0), ("AMP.LEVEL", 0.0)],
        rows=RND, macros=(0.4, 0.3, 0.4, 0.3), voices=1, level=0.45, move="subtle", kind="",
        summary="A wavetable mono lead: the saw-like row of bank 0 with the AD envelope blooming the scan on every note, a vibrato the knob brings in, a delay and a hall.")

    # BASS -------------------------------------------------------------------------------------------------------------- (SNAP BASS)
    W["WAVE BASS"] = N("WAVE BASS", "BASS", {"SRC": "WAVE", "SHP": "FILTER", "FX2": "REVERB"},
        {"SRC": {"BANK": 0.0, "SCAN X": 0.15, "MORPH Y": 0.43, "DRIFT": 0.05, "AD ATK": 0.0, "AD DEC": 0.25, "AD>X": 0.45, "LEVEL": 1.0},
         "SHP": FL("LP", 0.5, 0.2, 0.2, 0.0, 0.7, 0.3),
         "FX2": REV("PLATE", 0.3, 0.1)},
        {"ATTACK": 0.0, "DECAY": 0.4, "SUSTAIN": 0.8, "RELEASE": 0.12, "VEL AMT": 0.5, "GLIDE": 0.1, "MODE": "MONO"},
        k=[("TONE", "A round fundamental with a little second harmonic at 0, the scan walking up the row to a brighter, edged bass; the top rings with filter FM.",
            "SRC.SCAN X .5 0 1 L; SHP.CUTOFF .35 0 1 L; SRC.MORPH Y .15 .4 1 SL; SHP.AUDIO FM .4 .65 1 SL"),
           ("SNAP", "A plain note at 0; the envelope throws the scan and the filter open on every note, then a pitch dive joins at the top.",
            "SRC.AD>X .4 0 1 L; SHP.ENV AMT .3 .1 1 L; SRC.AD DEC .15 .3 1 L; SRC.AD>PTCH .12 .6 1 SL"),
           ("WOBBLE", "The cutoff and the scan wobble with the LFOs, slow at the middle and fast at the top.",
            "LFO.RATE 1 .45 .3 1 SL; ROW 13 .3 .3 1 SL; ROW 14 .25 .3 1 SL; ROW 15 .3 .4 1 SL; LFO.RATE 2 .4 .4 1 SL; FX 2.MIX .2 .5 1 L"),
           ("GRIT", "Drive and resonance rise gently; near the top a bit-crush, a per-note pitch scatter and the shuffled bank tear the bass apart.",
            "SHP.DRIVE .4 0 1 L; SHP.RESO .3 .3 1 SL; ROW @1 .05 .7 1 SL; SRC.BITS -.4 .85 1 T; SRC.BANK .6 .75 1 SL; SRC.SIGN .4 .4 1 SL")],
        lfo=LF(("SINE", 0.2), ("TRI", 0.25), ("DRIFT", 0.15)),
        lfo_tgt=[("SHP.CUTOFF", 0.0), ("SRC.SCAN X", 0.0), ("SRC.MORPH Y", 0.0), ("PAN", 0.0), ("PITCH", 0.0), ("AMP.LEVEL", 0.0)],
        rows=RND, macros=(0.4, 0.3, 0.2, 0.2), voices=1, level=0.45, move="medium", kind="",
        summary="A wavetable mono bass: the low end of bank 0's saw-like row, an AD envelope that snaps the scan open on every note, and a plate.")

    # TEXTURE ----------------------------------------------------------------------------------------------------------- (TIDAL WASH)
    W["WAVE TEXTURE"] = N("WAVE TEXTURE", "FX", {"SRC": "WAVE", "SHP": "FILTER", "MODA": "TIDES", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"BANK": 0.667, "SCAN X": 0.35, "MORPH Y": 0.3, "DRIFT": 0.2, "LEVEL": 1.0},
         "SHP": FL("LP", 0.45, 0.15, 0.1, 0.0, 0.5, 0.1),
         "MODA": TD("LOOP", 0.1, 0.5, smooth=0.7),
         "FX1": CHO(0.4, "CHORUS", 0.15, 0.4, 0.9), "FX2": REV("HALL", 0.9, 0.4, SHIMMER=0.3)},
        {"ATTACK": 0.45, "DECAY": 0.5, "SUSTAIN": 0.9, "RELEASE": 0.7, "VEL AMT": 0.2, "SPREAD": 0.8, "VINTAGE": 0.2},
        k=[("SCAN", "A hollow, muffled wash at 0, the scan walking through bank 2's hollow and vocal shapes; the top is bright and steps from wave to wave.",
            "SRC.SCAN X .5 0 .9 L; SRC.MORPH Y .35 .2 1 L; SHP.CUTOFF .3 0 1 L; SRC.QUANT 1 .93 1 T"),
           ("TIDE", "The Tides loop speeds up and the LFOs follow, taking the scan with them; the top is a fast, churning tide.",
            "ROW 13 .3 .3 1 SL; ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; MOD A.FREQ .35 .3 1 SL; SRC.BANK .3 .5 1 SL"),
           ("WASH", "A close wash at 0, a huge hall and a thick chorus; past 55% the shimmer rises and past 65% the chorus feeds back.",
            "FX 2.SIZE .2 .3 1 L; FX 2.MIX .3 0 .6 L; FX 1.MIX .3 .2 1 L; FX 1.FEEDBACK .4 .6 1 SL; FX 2.SHIMMER .4 .55 1 SL; AMP.RELEASE .3 .5 1 S"),
           ("GRIT", "Drive and resonance, then a per-note pitch scatter, a bit-crush and the shuffled bank near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L; SHP.DRIVE .3 .5 1 L; SRC.BANK .5 .7 1 SL")],
        lfo=LF(("SINE", 0.1), ("DRIFT", 0.15), ("TRI", 0.08)),
        lfo_tgt=[("SRC.SCAN X", 0.05), ("SHP.CUTOFF", 0.05), ("SRC.MORPH Y", 0.05), ("PAN", 0.1), ("PITCH", 0.0), ("AMP.LEVEL", 0.0)],
        rows=RND + [["A TIDES", "SRC.SCAN X", 0.3, "M2"], ["A TIDE+", "SRC.MORPH Y", 0.25, "M2"]],
        macros=(0.4, 0.4, 0.4, 0.2), voices=4, level=0.4, move="bold", kind="chords",
        summary="A wavetable wash: chords held on bank 2's hollow and vocal shapes while a very slow Tides loop and the LFOs walk the scan, a thick chorus and a huge hall.")

    # DRONE ------------------------------------------------------------------------------------------------------------- (ATMOSPHERE)
    W["WAVE DRONE"] = N("WAVE DRONE", "FX", {"SRC": "WAVE", "SHP": "FILTER", "FX1": "DELAY", "FX2": "REVERB"},
        {"SRC": {"BANK": 0.34, "SCAN X": 0.2, "MORPH Y": 0.86, "DRIFT": 0.15, "OCTAVE": -1, "LOWPASS": 0.45, "LEVEL": 1.0},
         "SHP": FL("LP", 0.5, 0.1, 0.1, 0.0, 0.4, 0.1),
         "FX1": DLY("1/4D", 0.4, 0.25, "PING"), "FX2": REV("HALL", 0.9, 0.4, SHIMMER=0.35)},
        {"ATTACK": 0.6, "DECAY": 0.5, "SUSTAIN": 1.0, "RELEASE": 0.7, "VEL AMT": 0.1, "SPREAD": 0.8, "VINTAGE": 0.3},
        k=[("TONE", "A dark, low hum at 0, the lowpass and the scan opening to a bright, buzzing drone; the top is hard and bright.",
            "SRC.LOWPASS .5 0 1 L; SRC.SCAN X .45 0 1 L; SHP.CUTOFF .3 0 1 L; SRC.MORPH Y -.3 .3 1 L"),
           ("DRIFT", "The LFOs speed up and the scan, the pan and the bank drift further; the top is a restless, shifting drone.",
            "LFO.RATE 1 .4 .3 1 SL; LFO.RATE 2 .4 .3 1 SL; ROW 13 .3 .4 1 SL; ROW 14 .3 .4 1 SL; ROW 17 .35 .5 1 SL; ROW 15 .2 .5 1 SL"),
           ("SPACE", "A close drone at 0, a dotted delay and a huge hall that grow; past 60% the shimmer rises and the delay feeds back.",
            "FX 1.FEEDBACK .35 .5 1 SL; FX 2.SIZE .1 0 .5 L; FX 2.MIX .25 0 .6 L; FX 2.SHIMMER .45 .6 1 SL; AMP.RELEASE .3 .5 1 S; FX 1.MIX .25 .2 1 L"),
           ("GRIT", "Drive and resonance, then a per-note pitch scatter, a bit-crush and stepping near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L; SHP.DRIVE .3 .5 1 L; SRC.QUANT 1 .9 1 T")],
        lfo=LF(("SINE", 0.08), ("DRIFT", 0.1), ("TRI", 0.06)),
        lfo_tgt=[("SRC.SCAN X", 0.12), ("SRC.MORPH Y", 0.1), ("SHP.CUTOFF", 0.1), ("PAN", 0.15), ("SRC.BANK", 0.03), ("AMP.LEVEL", 0.0)],
        rows=RND, macros=(0.4, 0.3, 0.4, 0.2), voices=4, level=0.4, move="medium", kind="chords",
        summary="A wavetable drone: a low, fundamental-heavy wave from bank 1 that three slow LFOs scan, a dotted delay and a huge shimmer hall.")
    return W
