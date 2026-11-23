# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""The factory bank of 45, round 1: positions 1 to 15 (pads and drones 1-9, keys 10-15). See presets_bank45.py."""
from presets_bank45 import spec, newp
from presets_granular import REV
from presets_batch2 import CHO, LF, AM
from presets_batch3 import TD, FL, PK


def round1(pool):
    S = lambda *a, **kw: spec(*a, pool=pool, **kw)
    N = newp
    L = []
    a = L.append

    a(S("WARM PAD", cat="PAD", move="subtle", level=0.45, rows=[["RANDOM", "PITCH", 0.0]],
        summary="Six-voice analogue pad: four detuned saws, a low-pass filter, a slow Tides loop, chorus and hall.",
        k=[("MORPH", "Dark and close at 0, warm and open by the middle, then a ringing folded edge from about 70% as filter FM and timbre fold arrive.",
            "SHP.AUDIO FM .35 .7 1 SL; SRC.TIMBRE .35 .8 1 SL"),
           ("MOTION", "Tides drifts faster, then vibrato, a pan sweep and a cutoff wobble build; the top is a fast seasick tremble.",
            "LFO.RATE 1 .45 .3 1 SL; LFO.RATE 3 .4 .5 1 SL; ROW 13 .035 .45 1 SL; ROW 14 .5 .3 1 S; ROW 17 .25 .6 1 SL"),
           ("WASH", "Reverb and chorus open from the middle; past 65% the chorus feeds back and the tails stretch into a wash.",
            "FX 1.FEEDBACK .55 .65 1 SL; AMP.RELEASE .35 .5 1 S; FX 1.DEPTH .4 .6 1 SL"),
           ("GRIT", "A little resonance and drift, then drive and a random per-note pitch scatter past 70%; the very top crushes the bits.",
            "SHP.DRIVE .5 .5 1 SL; ROW @1 .06 .7 1 SL; SRC.DRIFT .5 .3 1 L; SRC.BITS -.5 .88 1 T")]))

    a(S("SILK STRINGS", cat="PAD", move="subtle", level=0.3, rows=[["RANDOM", "PITCH", 0.0]],
        summary="Slow ensemble strings: a Peaks LFO breathes the filter, chorus widens the section, a hall holds it.",
        k=[("BOW", "A soft bow at 0, brighter and slower to speak towards the middle, then a glassy filter-FM edge in the top third.",
            "SHP.AUDIO FM .3 .75 1 SL; SRC.TIMBRE .3 .8 1 SL"),
           ("BREATH", "The Peaks LFO speeds up and deepens; the section starts to bloom with pitch and chorus movement and at the top it shudders.",
            "MOD A.K2 .5 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; ROW 15 .03 .5 1 SL; ROW 13 .4 .5 1 S"),
           ("HALL", "Reverb and chorus width open up; past 65% the chorus feeds back and the release lengthens into a wash.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .3 .5 1 S; FX 2.SIZE .3 .4 1 L"),
           ("ROSIN", "Resonance and drift, then drive and a random pitch scatter per note near the top, and a bit-crush at the end.",
            "SHP.DRIVE .5 .5 1 SL; ROW @1 .05 .7 1 SL; SRC.BITS -.4 .9 1 T")]))

    a(S("SLOW STRINGS", cat="PAD", move="medium", level=0.3, rows=[["RANDOM", "PITCH", 0.0]],
        summary="Slow-attack strings with a Peaks envelope and a Tides loop that move the filter and the timbre.",
        k=[("SWELL", "Hardly any attack at 0 to a long swell and brighter saw by the middle; the top folds the timbre and rings the filter.",
            "SHP.AUDIO FM .3 .75 1 SL; SRC.TIMBRE .3 .8 1 SL"),
           ("TIDE", "Tides speeds up and the filter and timbre moves follow it; the top is a fast stutter of the whole sound.",
            "LFO.RATE 1 .45 .35 1 SL; MOD A.K2 .5 .5 1 SL; ROW 13 .5 .5 1 SL; MOD A.K1 .3 .6 1 L"),
           ("CATHEDRAL", "Chorus and reverb grow together; past 65% the hall runs on and the chorus feeds back.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .35 .5 1 S; FX 2.SIZE .15 .5 1 L"),
           ("DUST", "Resonance and drive rise gently; random pitch scatter and a bit-crush take over near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.4 .88 1 T; SRC.DRIFT .4 .3 1 L")]))

    a(S("VOX PAD", cat="PAD", move="subtle", level=0.35, rows=[["RANDOM", "PITCH", 0.0]], lfo_set={5: ("AMP.LEVEL", 0.0)}, edit={"LFO": {"SHAPE 3": "SQUARE"}},
        summary="A choir-like pad from Braids' vowel model, a slow Tides loop and a wide chorus.",
        k=[("VOWEL", "Round and quiet at 0, the vowel opens through the middle; the top breaks into a hard, ringing edge.",
            "SHP.AUDIO FM .6 .6 1 SL; SRC.TIMBRE .3 .7 1 SL"),
           ("BREATH", "Tides slowly moves the vowel and the filter; the top adds a fast tremolo and vibrato.",
            "LFO.RATE 1 .4 .3 1 SL; LFO.RATE 3 .5 .5 1 SL; ROW 13 .03 .5 1 SL; ROW 15 .3 .6 1 SL; ROW 18 .5 .55 1 SL"),
           ("CHAPEL", "Chorus and hall open; past 65% the choir smears into a long wash.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .35 .5 1 S; FX 2.SIZE .2 .4 1 L"),
           ("AIR", "Resonance and drive add a grainy edge; past 70% a per-note pitch scatter and a bit-crush.",
            "ROW @1 .1 .65 1 SL; SRC.BITS -.4 .9 1 T; SRC.DRIFT .4 .3 1 L")]))

    a(S("VOWEL PAD", cat="PAD", move="medium", level=0.5, rows=[["RANDOM", "PITCH", 0.0]], edit={"SHP": {"CUTOFF": 0.0}},
        summary="A formant pad where a FUNC envelope pair sweeps the vowel and the filter on every note.",
        k=[("OPEN", "Closed and dark at 0, the vowel opens with a longer attack; the top folds the formants and rings.",
            "SRC.RATE -.6 .7 1 SL; SHP.AUDIO FM .6 .4 1 L"),
           ("RISE", "The FUNC rise and fall times stretch, then LFO speeds and vibrato join; the top is a fast wobble.",
            "MOD A.RISE 1 .3 .3 1 L; LFO.RATE 1 .45 .4 1 SL; ROW 13 .35 .5 1 SL; MOD A.FALL 1 .3 .5 1 SL"),
           ("ROOM", "Reverb size and mix, and the chorus feedback, take the sound from a room to a flood.",
            "FX 1.FEEDBACK .55 .65 1 SL; AMP.RELEASE .3 .5 1 S; FX 1.DEPTH .4 .5 1 L"),
           ("CRUSH", "Drive and resonance, then a random pitch scatter and a bit-crush in the last third.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.4 .88 1 T; SRC.DRIFT .5 .3 1 L")]))

    a(S("TIDE PAD", cat="PAD", move="bold", level=0.6, rows=[["RANDOM", "PITCH", 0.0]],
        drop=[(3, "FX 2.MIX"), (3, "FX 2.SIZE")], slots={"FX2": None}, edit={"FX1": {"REVERB": 0.3}},
        lfo_tgt=[("SHP.CUTOFF", 0.15), ("SRC.COLOR", 0.1), ("FX 1.BLEND", 0.1), None, ("SRC.TIMBRE", 0.1), ("AMP.LEVEL", 0.0)],
        summary="A saw pad through light Clouds with a Tides AR that moves the colour, the cutoff and the grain blend.",
        k=[("COLOUR", "A dark saw at 0, colour and cutoff opening and the Tides speed rising to the middle; the top scans the grains hard.",
            "FX 1.POSITION .4 .3 1 L; FX 1.SIZE -.5 .6 1 SL; SHP.AUDIO FM .5 .6 1 SL; FX 1.TEXTURE .4 .65 1 SL"),
           ("MOTION", "The shape of the Tides AR and the LFO speeds move together; the top is a restless, fast-moving cloud.",
            "LFO.RATE 1 .45 .3 1 SL; LFO.RATE 2 .4 .4 1 SL; ROW 13 .3 .5 1 SL; ROW 14 .3 .5 1 SL; LFO.RATE 3 .5 .4 1 SL; ROW 18 .6 .5 1 SL"),
           ("CLOUD", "The reverb opens; Clouds' own reverb and feedback follow, and the tails run on and on.",
            "FX 1.REVERB .5 .4 1 SL; FX 1.FEEDBACK .45 .65 1 SL; AMP.RELEASE .3 .5 1 S; FX 1.TONE .25 .5 1 SL"),
           ("SCATTER", "Resonance and drive, then Clouds' grain spray and pitch random: the pad breaks into drifting pitched grains.",
            "FX 1.SPRAY .7 .5 1 SL; FX 1.PITCH RND .7 .55 1 SL; ROW @1 .08 .7 1 SL; FX 1.TONE -.5 .7 1 L; FX 1.SIZE RND .6 .6 1 SL; FX 1.BLEND .4 .6 1 SL")]))

    a(S("HORN SECT", cat="PAD", move="medium", level=0.35, rows=[["RANDOM", "PITCH", 0.0]],
        summary="A brass section: four saws with a quick filter swell and a Tides envelope, vibrato that the knob brings in.",
        k=[("BITE", "Mellow at 0, a harder bite and more filter attack to the middle; the top is a blaring ringing edge.",
            "SHP.AUDIO FM .35 .7 1 SL; SRC.TIMBRE .3 .8 1 SL"),
           ("VIBRATO", "A slow vibrato grows with the speed, then an uneven tremolo; the top shakes like a fast shake on a trumpet.",
            "ROW 15 .03 .45 1 SL; LFO.RATE 3 .4 .4 1 SL; ROW 17 .25 .5 1 SL; MOD A.FREQ .3 .5 1 SL"),
           ("HALL", "A little hall at 0, a big room by the top, the chorus feeding back from 65%.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .35 .5 1 S; FX 2.SIZE .3 .3 1 L; FX 1.DEPTH .4 .5 1 L"),
           ("BLARE", "Resonance and colour rise; a random pitch scatter per note and a bit-crush at the very top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.4 .9 1 T; SHP.DRIVE .4 .6 1 SL")]))

    a(S("PD SHIMMER", name="SHIMMER PAD", cat="PAD", move="subtle", level=0.3,
        slots={"SHP": "FILTER", "MODA": "TIDES", "FX2": "CHORUS"},
        edit={"SHP": FL("LP", 0.5, 0.05, 0.05, 0.0, 0.5, 0.1), "MODA": TD("LOOP", 0.12, 0.5),
              "FX2": CHO(0.25, "CHORUS", 0.2, 0.4, 0.9), "LFO": LF(("SINE", 0.1), ("DRIFT", 0.12), ("TRI", 0.08))},
        lfo_tgt=[("PITCH", 0.005), ("PAN", 0.1), ("FX 2.DEPTH", 0.1), None, ("SHP.CUTOFF", 0.04), ("AMP.LEVEL", 0.0)],
        rows=[["A TIDES", "SHP.CUTOFF", 0.25, "M2"], ["A TIDE+", "SRC.TIMBRE", 0.3, "M2"], ["RANDOM", "PITCH", 0.0]],
        macros=[0.5, 0.2, 0.5, 0.4],
        summary="A harmonic pad into a shimmer reverb (an octave of the tail rising) with a slow Tides loop and chorus.",
        k=[("TIMBRE", "Pure and soft at 0 to fuller harmonics by the middle; the top rings the filter with FM.",
            "SHP.CUTOFF .45 0 .7 L; SRC.COLOR .4 0 1 L; SHP.AUDIO FM .7 .55 1 SL; SHP.DRIVE .3 .6 1 SL"),
           ("MOTION", "Tides speeds up and the chorus widens; vibrato and tremolo come in at the top.",
            "MOD A.FREQ .35 0 1 L; LFO.RATE 1 .4 .3 1 SL; ROW 13 .03 .5 1 SL; ROW 15 .4 .4 1 S; LFO.RATE 3 .5 .45 1 SL; ROW 18 .5 .5 1 SL"),
           ("SHIMMER", "The octave tail grows from a hint to a rising cloud; past 70% the shimmer feeds on itself.",
            "FX 2.FEEDBACK .45 .65 1 SL; AMP.RELEASE .3 .5 1 S; FX 1.SIZE .1 .5 1 L"),
           ("GRIT", "A little drive, then a per-note pitch scatter and a bit-crush near the top.",
            "SHP.DRIVE .5 .5 1 SL; ROW @1 .05 .7 1 SL; SRC.BITS -.4 .9 1 T; SRC.DRIFT .4 .2 1 L")]))

    a(S("FX DEEP DRONE", name="DEEP DRONE", cat="PAD", move="bold", level=0.25,
        slots={"MODA": "TIDES"},
        edit={"MODA": TD("LOOP", 0.04, 0.5, 0.0, 0.7), "LFO": LF(("SINE", 0.05), ("DRIFT", 0.08), ("TRI", 0.06))},
        lfo_tgt=[("SHP.CUTOFF", 0.06), ("PAN", 0.15), ("PITCH", 0.004), None, ("SRC.TIMBRE", 0.05), None],
        rows=[["A TIDES", "SHP.CUTOFF", 0.3, "M2"], ["A TIDE+", "SRC.COLOR", 0.3, "M2"], ["RANDOM", "PITCH", 0.0]],
        macros=[0.5, 0.2, 0.5, 0.3],
        summary="A low sub drone, slowly opening and closing with a long chorus and a huge hall; a held note is a room tone.",
        k=[("DEPTH", "A deep sine-like hum at 0, the filter and timbre open into a buzzing saw drone; the top rings with filter FM.",
            "SRC.COLOR .3 0 1 L; SRC.TIMBRE .3 .3 1 L; SHP.AUDIO FM .35 .7 1 SL"),
           ("TIDE", "A very slow filter breathing at 0; the speed rises until it pulses; the top tremors the pitch.",
            "MOD A.FREQ .45 0 1 L; LFO.RATE 1 .5 .3 1 SL; ROW 15 .02 .5 1 SL; ROW 13 .3 .5 1 SL"),
           ("VOID", "Hall mix and chorus width grow; the release stretches and the chorus feeds back into a wash.",
            "FX 1.FEEDBACK .5 .6 1 SL; AMP.RELEASE .15 .5 1 S; FX 2.SIZE .1 .5 1 L"),
           ("ROT", "Drive and resonance, then a pitch scatter that spreads the drone into a cluster, and a bit-crush.",
            "ROW @1 .05 .65 1 SL; SRC.BITS -.4 .88 1 T; SRC.DRIFT .5 .2 1 L")]))

    a(S("TINE EP", cat="KEYS", move="medium", level=0.6, rows=[["RANDOM", "PITCH", 0.0]], lfo_set={4: ("AMP.LEVEL", 0.0)}, edit={"LFO": {"SHAPE 3": "SQUARE"}},
        summary="A tine electric piano: Braids wavetable bark that dies away, Tides as a tremolo and wah, chorus and plate.",
        k=[("BARK", "Mellow at 0, a harder tine and a longer decay to the middle; past 70% the filter FM and drive turn it to a growl.",
            "SHP.AUDIO FM .3 .75 1 SL; SRC.TIMBRE .3 .85 1 SL"),
           ("TREMOLO", "The tremolo speeds up and the pan swings wider; the top is a fast chopper.",
            "LFO.RATE 1 .45 .35 1 SL; ROW 16 .4 .4 1 S; ROW 15 .4 .45 1 SL; LFO.RATE 3 .55 .45 1 SL; ROW 17 .55 .5 1 SL"),
           ("CHORUS", "Chorus and plate open; past 65% the chorus feeds back and the tail lengthens.",
            "FX 1.FEEDBACK .5 .7 1 SL; AMP.RELEASE .4 .5 1 S; FX 1.DEPTH .4 0 1 L"),
           ("GROWL", "Drive and filter FM at 0 gentle; a pitch scatter per note and a bit-crush in the last third.",
            "ROW @1 .04 .7 1 SL; SRC.BITS -.5 .88 1 T")]))

    a(S("E PIANO", cat="KEYS", move="subtle", level=0.5, rows=[["RANDOM", "PITCH", 0.0]],
        summary="A Rhodes-like FM piano with a looping Tides as tremolo, chorus and plate.",
        k=[("TONE", "Dull and soft at 0, bell-like by the middle; the top rings out with FM and fold.",
            "SHP.AUDIO FM .3 .7 1 SL; SRC.COLOR .25 .8 1 SL"),
           ("TREMOLO", "Slow tremolo to a fast chop; the top pushes the Tides rate and the pan.",
            "LFO.RATE 1 .45 .35 1 SL; ROW 13 .3 .5 1 SL; MOD A.SHAPE .4 .5 1 SL"),
           ("SPACE", "Chorus width and plate mix; the chorus feeds back past 65% and the release stretches.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .4 .5 1 S; FX 2.SIZE .3 0 1 L"),
           ("DIRT", "A little drive and drift, then a per-note pitch scatter and a bit-crush at the top.",
            "ROW @1 .04 .7 1 SL; SRC.BITS -.5 .88 1 T; SHP.RESO .3 .6 1 SL")]))

    a(S("E PIANO", name="SOFT PIANO", cat="KEYS", move="subtle", level=0.5, rows=[["RANDOM", "PITCH", 0.0]], macros=[0.5, 0.0, 0.4, 0.3],
        edit={"SRC": {"TIMBRE": 0.1, "COLOR": 0.3}, "SHP": {"CUTOFF": 0.35}, "AMP": {"DECAY": 0.65, "SUSTAIN": 0.1, "RELEASE": 0.45},
              "MODA": TD("LOOP", 0.06, 0.5)},
        summary="A felt piano: Braids FM as the hammer and string, a dark filter, a long gentle decay, chorus and a hall; no tremolo until K2 brings it in.",
        k=[("TOUCH", "Felt and dark at 0, brighter hammers by the middle; the top is hard and metallic with filter FM and fold.",
            "SHP.AUDIO FM .5 .65 1 SL; SRC.COLOR .25 .8 1 SL"),
           ("TREMOLO", "None at 0; a slow tremolo and wah grow with the knob and the pan swings wider; the top is a fast chop.",
            "LFO.RATE 1 .45 .3 1 SL; ROW 13 .3 .5 1 SL; MOD A.SHAPE .4 .5 1 SL"),
           ("ROOM", "A dry upright at 0, a small hall by the middle, a cathedral wash by the top with the chorus feeding back.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .3 .5 1 S; FX 2.SIZE .3 .5 1 L"),
           ("HAMMER", "Drift and a little drive, then a per-note pitch scatter and a bit-crush near the top: a broken, out-of-tune piano.",
            "ROW @1 .05 .65 1 SL; SRC.BITS -.5 .85 1 T; SHP.RESO .3 .6 1 SL")]))

    a(S("TONEWHEEL", cat="KEYS", move="medium", level=0.4, rows=[["RANDOM", "PITCH", 0.0]],
        summary="A drawbar organ with a Peaks LFO that works the key click and cutoff, chorus as the rotary.",
        k=[("DRAWBARS", "Fewer harmonics at 0 to a full, bright registration; the top folds the timbre and rings the filter.",
            "SHP.AUDIO FM .3 .75 1 SL; SRC.TIMBRE .3 .85 1 SL"),
           ("LESLIE", "The chorus rotor speeds up from chorale to tremolo, and the LFOs follow; the top is a spinning blur.",
            "FX 1.RATE .6 .3 1 SL; LFO.RATE 1 .5 .3 1 SL; ROW 13 .4 .5 1 SL; MOD A.K2 .3 .5 1 SL; ROW 15 .3 .55 1 S"),
           ("HALL", "A little room at 0, a cathedral at the top with the rotor feeding back.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .4 .4 1 S; FX 2.MIX .2 .5 1 L"),
           ("DIRT", "A gentle overdrive; random pitch scatter and a bit-crush at the top.",
            "ROW @1 .04 .7 1 SL; SRC.BITS -.4 .88 1 T; SHP.AUDIO FM .2 .6 1 SL")]))

    a(S("ROTARY ORGAN", cat="KEYS", move="medium", level=0.4, rows=[["RANDOM", "PITCH", 0.0]], lfo_set={5: ("AMP.LEVEL", 0.0)},
        summary="A rotary-speaker organ: an LFO swings the level, pan and cutoff and the chorus rate follows the knob.",
        k=[("REGISTER", "Mellow flute at 0, bright reedy by the middle; the top folds the timbre and rings.",
            "SHP.AUDIO FM .3 .75 1 SL; SRC.TIMBRE .3 .85 1 SL"),
           ("ROTOR", "The rotor speeds from a slow chorale to a fast spin; the top tilts into a flutter.",
            "LFO.RATE 1 .45 .3 1 SL; ROW 15 .3 .55 1 SL; LFO.RATE 3 .5 .4 1 SL; ROW 18 .5 .5 1 SL; FX 1.DEPTH .4 .5 1 L"),
           ("CABINET", "Room reverb opens; past 65% the rotor feeds back and the release lengthens.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .4 .4 1 S; FX 2.SIZE .2 .5 1 L"),
           ("DRIVE", "Drive and resonance; a per-note pitch scatter and a bit-crush at the top.",
            "ROW @1 .04 .7 1 SL; SRC.BITS -.4 .88 1 T; SHP.AUDIO FM .25 .6 1 SL")]))

    a(S("ARP PLUCK", cat="KEYS", move="subtle", level=0.6, rows=[["RANDOM", "PITCH", 0.0]],
        summary="A short saw pluck for arpeggios, with a Peaks LFO and a ping-pong delay.",
        k=[("PLUCK", "Round and short at 0, open and bright by the middle; the top rings and bites with FM and timbre.",
            "SHP.AUDIO FM .3 .7 1 SL; SRC.TIMBRE .25 .8 1 SL"),
           ("MOTION", "The Peaks LFO speeds up and the pan swings; the top is a stuttering gate on the filter.",
            "LFO.RATE 1 .45 .35 1 SL; ROW 17 .3 .5 1 SL; MOD A.K2 .4 .5 1 SL; ROW 13 .3 .6 1 S"),
           ("ECHO", "The ping-pong delay and the plate open; past 65% the delay feeds back and its repeats darken.",
            "FX 1.FEEDBACK .45 .6 1 SL; FX 1.TONE -.4 .7 1 L; AMP.RELEASE .3 .5 1 S"),
           ("EDGE", "Resonance and decay at 0 gentle; a random pitch scatter and a bit-crush at the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.5 .88 1 T; SHP.DRIVE .4 .6 1 SL")]))
    return L
