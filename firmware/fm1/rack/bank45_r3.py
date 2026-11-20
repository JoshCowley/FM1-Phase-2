# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""The factory bank of 45, round 3: positions 31 to 45 (basses 31-37, textures 38-45). See presets_bank45.py."""
from presets_bank45 import spec, newp
from presets_batch2 import CHO, LF, AM
from presets_batch3 import TD, FL, PK
from presets_granular import REV, DLY

RND = [["RANDOM", "PITCH", 0.0]]
GRIT = ("Drive and resonance, then a per-note pitch scatter and a bit-crush near the top.",
        "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L")


def round3(pool):
    S = lambda *a, **kw: spec(*a, pool=pool, **kw)
    N = newp
    L = []
    a = L.append

    a(S("SUB BASS", cat="BASS", move="subtle", level=0.5, rows=RND,
        summary="A deep mono sub: a folded sine that stays round at the bottom, a slow Tides motion on the cutoff and a touch of chorus.",
        k=[("TONE", "A pure, deep sub at 0, more fold and bite by the middle; the top is a hard, ringing, buzzing fold.",
            "SHP.AUDIO FM .5 .6 1 SL"),
           ("MOTION", "A slow pulse and a gentle vibrato come in with the Tides speed; the top is a fast wobble.",
            "ROW 13 .02 .4 1 SL; ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L"),
           ("SPACE", "A dry sub at 0, a room and a chorus around it; past 65% the chorus feeds back and the tail runs on.",
            "FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", *GRIT)]))

    a(S("BS WARM MOOG", name="MOOG BASS", cat="BASS", move="subtle", level=0.5, rows=RND,
        slots={"FX1": "CHORUS", "FX2": "REVERB"}, edit={"FX1": CHO(0.0, "CHORUS", 0.2, 0.4, 0.8), "FX2": REV("PLATE", 0.3, 0.0)},
        lfo_tgt=[("PITCH", 0.0), None, ("SHP.CUTOFF", 0.03), None, None, None],
        summary="A warm Moog-style bass: a fat filtered saw with a gentle resonance, drive and a glide the fourth knob brings in.",
        k=[("CUTOFF", "A dark, round thump at 0, opening to a warm growl by the middle; the top is a bright, ringing buzz.",
            "SRC.TIMBRE .3 0 1 L; SHP.AUDIO FM .5 .6 1 SL; SHP.ENV AMT .3 0 1 L"),
           ("SQUELCH", "The resonance and a slow filter wobble grow together; the top sings and whistles.",
            "ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L"),
           ("DRIVE", "A clean bass at 0, driven and thick to the middle; a room and a chorus bloom around it from 40%.",
            "FX 1.MIX .3 .3 1 L; FX 2.MIX .25 .4 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("SLIDE", "Notes glide into each other more and more; past 70% a per-note pitch scatter and a bit-crush join.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L; SHP.DRIVE .4 .3 1 L; SHP.CUTOFF -.25 .5 1 L")]))

    a(S("SNAP BASS", cat="BASS", move="medium", level=0.5, rows=RND,
        summary="A plucky mono bass: a fast filter snap on every note, a Tides motion on the colour and a little chorus.",
        k=[("TONE", "A soft, round pluck at 0, a sharper snap by the middle; the top is a hard, ringing snap.",
            "SHP.AUDIO FM .5 .6 1 SL"),
           ("MOTION", "The snap and the filter movement speed up with the Tides; the top is a fast, chopped wobble.",
            "ROW 13 .02 .4 1 SL; ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L"),
           ("SPACE", "A dry pluck at 0, a room and a chorus around it; past 65% the chorus feeds back.",
            "FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", *GRIT)]))

    a(S("ACID LINE", name="ACID BASS", cat="BASS", move="medium", level=0.5, rows=RND,
        summary="A 303-style mono line: a resonant saw filter with accent and a ping-pong delay, Tides moving the cutoff.",
        k=[("TONE", "A muted, round squelch at 0, a rubbery acid line by the middle; the top screams.",
            "SHP.AUDIO FM .5 .6 1 SL"),
           ("MOTION", "The Tides speed and the accent grow; the top is a fast, wild filter sweep.",
            "ROW 13 .02 .4 1 SL; ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L"),
           ("SPACE", "A dry line at 0, a delay and a plate behind it; past 65% the delay feeds back into a wash.",
            "FX 1.FEEDBACK .45 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", *GRIT)]))

    a(S("BS REESE", name="REESE", cat="BASS", move="medium", level=0.5, rows=RND, voices=4,
        slots={"FX2": "REVERB"}, edit={"SHP": {"CUTOFF": 0.3}, "FX2": REV("PLATE", 0.3, 0.0), "LFO": LF(("SINE", 0.25), ("DRIFT", 0.2), ("TRI", 0.12))},
        lfo_tgt=[("PITCH", 0.0), None, ("SHP.CUTOFF", 0.03), None, None, None],
        summary="A reese bass: two detuned saws beating against each other through a slow filter, chorus and a plate.",
        k=[("CUTOFF", "A dark, close growl at 0, the beating opening up by the middle; the top is bright and buzzing.",
            "SRC.TIMBRE .3 0 1 L; SHP.AUDIO FM .5 .5 1 SL; SHP.CUTOFF .25 0 1 L; SHP.DRIVE .4 .5 1 L; SRC.COLOR .3 .5 1 L"),
           ("DETUNE", "The two saws pull apart: a slow beat at 0 to a fast, nasty phasing; the top is a wide, rough drone.",
            "ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L; ROW 13 .02 .5 1 SL"),
           ("DRIVE", "A clean bass at 0, driven and thick to the middle; a plate and a chorus bloom from 40%.",
            "FX 1.MIX .3 .3 1 L; FX 2.MIX .25 .4 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", "Resonance, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L")]))

    a(S("FOLD BASS", cat="BASS", move="medium", level=0.5, rows=RND,
        summary="A wavefolded mono bass: a harmonic fold that a Func envelope pushes on every note, a filter and a plate.",
        k=[("FOLD", "A round, nearly pure bass at 0, a harmonic fold by the middle; the top is a hard, hollow, metallic buzz.",
            "SHP.AUDIO FM .5 .6 1 SL"),
           ("MOTION", "The fall of the Func envelope and the cutoff wobble speed up; the top is a fast, restless fold.",
            "ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L; ROW 13 .2 .4 1 SL"),
           ("ROOM", "A dry bass at 0, a plate that opens into a hall; past 65% the tail runs on.",
            "AMP.RELEASE .4 .5 1 S; FX 2.DAMP -.3 .5 1 L"),
           ("GRIT", *GRIT)]))

    a(S("TIDAL BASS", cat="BASS", move="bold", level=0.5, rows=RND,
        summary="A bass the Tides moves in time: the cutoff, the colour and the drive follow a loop, with a chorus and a plate.",
        k=[("TONE", "A dark, thick bass at 0, opening and growling by the middle; the top is bright and ringing.",
            "SHP.AUDIO FM .5 .6 1 SL"),
           ("TIDE", "The Tides loop speeds up and the LFOs follow; the top is a fast, chopping, almost rhythmic bass.",
            "ROW 15 .3 .3 1 SL; SHP.CUTOFF .25 0 1 L; ROW 13 .2 .4 1 SL"),
           ("SPACE", "A dry bass at 0, a plate and a chorus around it; past 65% the chorus feeds back.",
            "FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", *GRIT)]))

    # textures: tonal and playable, with the movement doing the work
    a(S("MODEL DIAL", cat="FX", move="subtle", level=0.5, rows=RND, kind="melodic",
        summary="A vowel-organ texture where the first knob dials through four neighbouring Braids models while the Tides moves the cutoff and the colour.",
        k=[("DIAL", "One model at 0, the next three vowels and voices fading in as it turns; the top rings with audio FM.",
            "SHP.AUDIO FM .5 .6 1 SL"),
           ("MOTION", "The Tides loop speeds up and the colour and cutoff LFOs follow; the top is a fast, restless texture.",
            "ROW 13 .3 .3 1 SL; ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L"),
           ("SPACE", "A dry texture at 0, a delay and a hall around it; past 65% the delay feeds back into a wash.",
            "FX 2.SIZE .2 .4 1 L; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", "Drive and resonance, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L")]))

    a(S("FX CHORD DRIFT", name="CHORD DRIFT", cat="FX", move="medium", level=0.5, rows=RND, voices=4, kind="chords", edit={"SRC": {"TIMBRE": 0.2}},
        lfo_tgt=[("SRC.COLOR", 0.05), ("SRC.TIMBRE", 0.04), ("PAN", 0.08), None, None, None],
        summary="Four-voice wavetable chords that drift in colour under a slow two-bar LFO, with a wide chorus and a hall.",
        k=[("COLOUR", "A dark, plain wavetable at 0, the colour opening through bright spectra; the top is glassy and metallic.",
            "SRC.TIMBRE .6 0 1 L; FX 1.DEPTH .3 .5 1 SL; FX 1.MIX .2 .5 1 L; FX 2.MIX .2 .3 1 L"),
           ("DRIFT", "The colour and timbre wander faster and the chords sway in the stereo field; the top is a fast, shimmering drift.",
            "ROW 13 .3 .3 1 SL; ROW 14 .3 .3 1 SL; ROW 15 .3 .4 1 SL; LFO.RATE 2 .4 .3 1 SL; LFO.RATE 1 .3 .5 1 SL"),
           ("HALL", "A dry chord at 0, a big hall and a thick chorus around it; past 65% the chorus feeds back.",
            "FX 2.SIZE .2 .3 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .35 .5 1 S"),
           ("GRIT", "Drive and resonance, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L; SRC.TIMBRE -.3 .5 1 L; FX 1.DEPTH .3 .5 1 L")]))

    a(S("FX STRING BLOOM", name="STRING BLOOM", cat="FX", move="subtle", level=0.5, rows=RND, kind="chords",
        slots={"FX1": "CHORUS", "FX2": "REVERB"}, edit={"FX1": CHO(0.0, "CHORUS", 0.2, 0.4, 0.85), "FX2": REV("HALL", 0.8, 0.0)},
        lfo_tgt=[("PAN", 0.1), ("SHP.POSITION", 0.05), None, None, None, None],
        summary="Harmonic chords that bloom through Rings' string and reverb, the position drifting slowly under an LFO.",
        k=[("BRIGHT", "A dark, damped string at 0, brightening to a singing bloom; the top is bright and ringing.",
            "SRC.TIMBRE .3 .3 1 L; SHP.EXCITE .3 .5 1 L; SHP.MIX .2 .5 1 L"),
           ("STRUCT", "The string's structure changes from a plain string to a metallic one; the top drifts and sways quickly.",
            "ROW 13 .3 .3 1 SL; ROW 14 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.POSITION .3 .5 1 L"),
           ("SPACE", "A close string at 0, a long damped bloom by the middle; a hall and chorus join, and past 65% the chorus feeds back.",
            "FX 2.MIX .35 .3 1 L; FX 2.SIZE .2 .3 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .3 .5 1 S"),
           ("SWELL", "A quick onset at 0 to a slow bloom; past 70% a per-note pitch scatter and a bit-crush join.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L")]))

    a(N("SINGING BOWL", "FX", {"SRC": "BRAIDS", "SHP": "FILTER", "FX1": "CHORUS", "FX2": "REVERB"},
        {"SRC": {"MODEL": "FM", "TIMBRE": 0.08, "COLOR": 0.95, "AD DEC": 0.3, "AD>TIMB": 0.35, "DRIFT": 0.04, "LEVEL": 1.0},
         "SHP": FL("LP", 0.4, 0.05, 0.05, 0.0, 0.7, 0.2), "FX1": CHO(0.4, "CHORUS", 0.12, 0.3, 0.85), "FX2": REV("HALL", 0.9, 0.2)},
        {"ATTACK": 0.0, "DECAY": 0.9, "SUSTAIN": 0.0, "RELEASE": 0.85, "VEL AMT": 0.55},
        k=[("STRIKE", "A soft, round bowl at 0, a harder, brighter strike by the middle; the top is a hard, clanging, metallic bowl.",
            "SRC.TIMBRE .45 0 1 SL; SHP.CUTOFF .5 0 1 L; SRC.AD>TIMB .3 .2 1 L; SHP.AUDIO FM .5 .6 1 SL; SRC.COLOR .03 .6 1 SL"),
           ("BEAT", "The slow beating and the tremolo speed up and deepen; the top is a fast, pulsing wobble.",
            "LFO.RATE 1 .45 .3 1 SL; ROW 13 .3 .4 1 SL; ROW 14 .3 .4 1 SL; LFO.RATE 3 .5 .4 1 SL; ROW 17 .5 .5 1 SL; ROW 16 .4 .5 1 SL"),
           ("HALL", "A dry strike at 0, a big hall and a chorus around it; past 65% the chorus feeds back and the ring runs on.",
            "FX 2.MIX .3 0 .5 L; FX 2.SIZE .2 .3 1 L; FX 1.MIX .3 .4 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("CRACK", "A little resonance, then a per-note pitch scatter and a bit-crush near the top: a cracked, out-of-tune bowl.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .85 1 T; SRC.DRIFT .4 .3 1 L; SHP.RESO .3 .6 1 SL; SHP.DRIVE .3 .6 1 SL")],
        lfo=LF(("SINE", 0.25), ("DRIFT", 0.2), ("TRI", 0.12)),
        lfo_tgt=[("PAN", 0.06), ("SRC.TIMBRE", 0.03), ("PITCH", 0.003), ("SHP.CUTOFF", 0.02), ("AMP.LEVEL", 0.0), ("FX 2.MIX", 0.0)],
        rows=RND, macros=(0.5, 0.3, 0.4, 0.3), voices=4, level=0.5, move="subtle", kind="thumb",
        summary="A struck singing bowl: a high FM overtone over a long, slowly beating ring, a chorus and a big hall."))

    a(S("ATMOSPHERE", cat="FX", move="medium", level=0.5, rows=RND, kind="chords",
        summary="A wavetable atmosphere: a slow Tides loop and two Func cycles move the colour, the timbre and the cutoff, a tape delay and a shimmer reverb.",
        k=[("TONE", "A dark, soft atmosphere at 0, opening and brightening to the middle; the top is bright and glassy.",
            "SHP.AUDIO FM .5 .6 1 SL"),
           ("MOTION", "The Tides loop and the Func cycles speed up and the LFOs follow; the top is a fast, shimmering churn.",
            "LFO.RATE 1 .4 .3 1 SL; ROW 13 .3 .3 1 SL; ROW 15 .3 .3 1 SL; ROW 16 .3 .3 1 SL; SHP.CUTOFF .3 .5 1 L; FX 2.MIX .3 .5 1 L"),
           ("SPACE", "A close atmosphere at 0, a tape delay and a shimmer hall that grow; past 65% the delay feeds back.",
            "FX 1.FEEDBACK .35 .6 1 SL; AMP.RELEASE .3 .5 1 S"),
           ("GRIT", "Drive and resonance, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L; SHP.DRIVE .3 .5 1 L")]))

    a(S("FX TIDAL WASH", name="TIDAL WASH", cat="FX", move="bold", level=0.5, rows=RND, kind="chords",
        lfo_tgt=[("SHP.CUTOFF", 0.05), ("PAN", 0.1), ("SRC.COLOR", 0.05), None, None, None],
        summary="A saw wash the Tides sweeps through a dark filter on a very slow loop, with chorus and a big hall.",
        k=[("TONE", "A dark, muffled wash at 0, the filter opening to a bright sweeping wash; the top is bright and hissing.",
            "SHP.AUDIO FM .5 .6 1 SL; SRC.TIMBRE .3 .3 1 L; FX 1.MIX .2 .5 1 L"),
           ("TIDE", "The Tides loop speeds up and the cutoff and colour LFOs follow; the top is a fast, chopping tide.",
            "ROW 13 .3 .3 1 SL; ROW 15 .3 .3 1 SL; LFO.RATE 1 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L"),
           ("WASH", "A close wash at 0, a huge hall and a thick chorus; past 65% the chorus feeds back.",
            "FX 2.SIZE .2 .3 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .3 .5 1 S"),
           ("GRIT", "Drive and resonance, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L")]))

    a(S("TAPE KEYS", cat="FX", move="medium", level=0.5, rows=RND, drop=[(1, "SRC.TIMBRE"), (1, "SRC.COLOR"), (1, "SHP.CUTOFF")], kind="melodic",
        summary="Tape-worn keys: a saw through a Func envelope, a tape delay with wow and flutter, and a plate.",
        k=[("TONE", "Dull, worn keys at 0, a brighter tape tone by the middle; the top is bright and ringing.",
            "SRC.TIMBRE .7 0 1 SL; SRC.COLOR .7 0 1 SL; SHP.CUTOFF .6 0 1 SL; SHP.AUDIO FM .5 .6 1 SL; SHP.DRIVE .3 .5 1 L"),
           ("WOW", "The tape wow and the filter movement speed up; the top is a seasick, warbling tape.",
            "LFO.RATE 1 .4 .5 1 SL; ROW 13 .3 .5 1 SL; ROW 15 .3 .5 1 SL; SHP.CUTOFF .3 .5 1 L; FX 1.MIX .3 .5 1 L"),
           ("DELAY", "A dry key at 0, a tape delay and a plate that grow; past 65% the delay feeds back into a smear.",
            "FX 1.FEEDBACK .35 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", "Drive and resonance, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L; SHP.CUTOFF -.35 .4 1 L")]))

    a(S("FUNC PULSE", cat="FX", move="medium", level=0.5, rows=RND, kind="chords",
        summary="A pulsing saw chord: two Func envelopes fire on a clock so the cutoff, the colour and the fall times breathe in rhythm, with a delay and a hall.",
        k=[("PULSE", "A soft, slow pulse at 0, a sharper rhythmic pulse by the middle; the top is a hard, bright chop.",
            "SHP.AUDIO FM .5 .6 1 SL; SRC.TIMBRE .3 .5 1 L"),
           ("CLOCK", "The accents and the Func timing move faster and the LFOs follow; the top is a busy, shifting pulse.",
            "LFO.RATE 1 .4 .3 1 SL; ROW 13 .3 .3 1 SL; ROW 15 .3 .3 1 SL; SHP.CUTOFF .3 .5 1 L"),
           ("SPACE", "A dry pulse at 0, a delay and a hall around it; past 65% the delay feeds back into a wash.",
            "FX 1.FEEDBACK .25 .65 1 SL; AMP.RELEASE .4 .5 1 S; FX 2.SIZE .2 .4 1 L"),
           ("GRIT", "Drive and resonance, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L; SHP.CUTOFF -.3 .6 1 SL")]))
    return L
