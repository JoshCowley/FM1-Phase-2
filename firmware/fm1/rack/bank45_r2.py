# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""The factory bank of 45, round 2: positions 16 to 30 (keys 16-18, thumb pianos 19-24, leads 25-30). See presets_bank45.py."""
from presets_bank45 import spec, newp
from presets_batch2 import CHO, LF, AM
from presets_batch3 import TD, FL, PK
from presets_granular import REV, DLY


def round2(pool):
    S = lambda *a, **kw: spec(*a, pool=pool, **kw)
    N = newp
    L = []
    a = L.append

    a(S("FM BELL", cat="KEYS", move="subtle", level=0.45, rows=[["RANDOM", "PITCH", 0.0]], lfo_set={5: ("AMP.LEVEL", 0.0)},
        edit={"LFO": {"SHAPE 3": "SQUARE"}}, kind="arp",
        summary="A two-operator FM bell: a sharp inharmonic strike that mellows as it rings, Tides breathing the tone, chorus and hall.",
        k=[("STRIKE", "A soft mallet at 0, a bright glassy strike by the middle; the top is a hard, clanging, ringing edge.",
            "SHP.AUDIO FM .4 .65 1 SL; SRC.COLOR .2 .8 1 SL"),
           ("SHIMMER", "Tides speeds up and the tremolo and pan open; the top is a fast stutter over each ring.",
            "LFO.RATE 1 .4 .35 1 SL; ROW 13 .3 .5 1 SL; LFO.RATE 3 .5 .45 1 SL; ROW 18 .5 .5 1 SL; ROW 14 .4 .5 1 S"),
           ("HALO", "Chorus and hall grow around the bell; past 65% the chorus feeds back and the ring runs on into a wash.",
            "FX 1.FEEDBACK .5 .65 1 SL; AMP.RELEASE .4 .5 1 S; FX 2.SIZE .15 .5 1 L"),
           ("RUST", "Drive and a little resonance, then a per-note pitch scatter and a bit-crush: a cracked, out-of-tune bell.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .85 1 T; SRC.DRIFT .4 .3 1 L")]))

    a(S("SYMPATH HARP", name="SYMPATH HARP", cat="KEYS", move="medium", level=0.7, rows=[["RANDOM", "PITCH", 0.0]], lfo_set={4: ("AMP.LEVEL", 0.0)},
        edit={"LFO": {"SHAPE 3": "SQUARE"}, "SRC": {"MODEL": "//x3", "TIMBRE": 0.3, "COLOR": 0.7}, "SHP": {"MODEL": "STRING"}}, kind="arp",   # update 61: Braids' PLUK is withdrawn (-> //x3, chosen by measurement), Rings' SYMPATH played STRING already
        summary="A harp: a bright triple-saw exciter into Rings' strings, triggered by Grids for the accents, delay and hall.",
        k=[("PLUCK", "Muted and woody at 0, a ringing bright pluck at the middle; the top is a hard strum with the strings fully sympathetic.",
            "SHP.STRUCT .3 .6 1 SL; SHP.EXCITE .3 .65 1 SL; SRC.TIMBRE .35 0 1 L; SHP.BRIGHT .3 .3 1 L; SRC.COLOR .2 .5 1 L"),
           ("GRIDS", "The Grids pattern fills in with accents and the LFOs speed up; the top chops the strings into a rapid tremolo.",
            "LFO.RATE 1 .45 .3 1 SL; ROW 17 .5 .45 1 SL; LFO.RATE 3 .5 .45 1 SL; MOD A.CHAOS .5 .6 1 SL; SHP.POSITION .3 0 1 L; SHP.BRIGHT .25 0 1 L"),
           ("HALL", "A dry strum at 0 to a washed harp in a cathedral; past 65% the delay feeds back.",
            "FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .3 .5 1 S; FX 2.SIZE .15 .5 1 L"),
           ("DRIFT", "A little spread and drift, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .85 1 T; SRC.DRIFT .4 .3 1 L")]))

    # thumb pianos: Braids FM (the COLOUR sets the tine's overtone ratio), a short filter and a ping-pong or chorus into a room
    SL = {"SRC": "BRAIDS", "SHP": "FILTER", "FX1": "CHORUS", "FX2": "REVERB"}
    SD = {"SRC": "BRAIDS", "SHP": "FILTER", "FX1": "DELAY", "FX2": "REVERB"}
    LT = [("PAN", 0.06), ("SRC.TIMBRE", 0.03), ("PITCH", 0.003), ("SHP.CUTOFF", 0.02), ("AMP.LEVEL", 0.0), ("FX 2.MIX", 0.0)]

    def thumb(name, slots, src, shp, amp, fx1, fx2, summary, ks, level=0.5, lfo=None, move="subtle", macros=(0.5, 0.3, 0.4, 0.3), kind="thumb", cat="KEYS"):
        base = {"DRIFT": 0.02, "LEVEL": 1.0}
        base.update(src)
        a(N(name, cat, slots, {"SRC": base, "SHP": shp, "FX1": fx1, "FX2": fx2},
            {"ATTACK": 0.0, "DECAY": 0.45, "SUSTAIN": 0.0, "RELEASE": 0.35, "VEL AMT": 0.6, **amp},
            k=ks, lfo=lfo or LF(("SINE", 0.25), ("DRIFT", 0.2), ("TRI", 0.12)), lfo_tgt=LT, rows=[["RANDOM", "PITCH", 0.0]],
            macros=macros, voices=6, level=level, move=move, summary=summary, kind=kind))

    def tk(tine, shiv, room, brk):
        return [("TINE", tine[0], tine[1]), ("SHIVER", shiv[0], shiv[1]), ("ROOM", room[0], room[1]), ("BREAK", brk[0], brk[1])]

    BRK = ("A little resonance, then a per-note pitch scatter and a bit-crush near the top: a loose, cracked tine.",
           "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .85 1 T; SRC.DRIFT .4 .3 1 L; SHP.RESO .3 .6 1 SL; SHP.DRIVE .3 .6 1 SL")
    SHV = ("Tides-style LFO speed, pan and tremolo grow; the top is a fast shimmering stutter over each note.",
           "LFO.RATE 1 .45 .3 1 SL; ROW 13 .3 .4 1 SL; ROW 14 .3 .4 1 SL; LFO.RATE 3 .5 .4 1 SL; ROW 17 .5 .5 1 SL; ROW 16 .4 .5 1 SL")

    thumb("KALIMBA", SL, {"MODEL": "FM", "TIMBRE": 0.06, "COLOR": 0.92, "AD DEC": 0.2, "AD>TIMB": 0.4},
          FL("LP", 0.45, 0.05, 0.05, 0.0, 0.7, 0.2), {"DECAY": 0.7}, CHO(0.25, "CHORUS", 0.2, 0.3, 0.8), REV("PLATE", 0.4, 0.15),
          "A kalimba: an FM tine with a quick bright tink that dies into a round fundamental, a touch of chorus and a plate.",
          tk(("The tine goes from a dull thud at 0 to a bright, ringing tink; the top is hard and clangy.",
              "SRC.TIMBRE .45 0 1 SL; SHP.CUTOFF .5 0 1 L; SRC.AD>TIMB .3 .2 1 L; SHP.AUDIO FM .5 .6 1 SL; SRC.COLOR .05 .6 1 SL"),
             SHV,
             ("A dry pluck to a plate and chorus around it; past 65% the chorus feeds back and the ring runs on.",
              "FX 2.MIX .3 0 .5 L; FX 2.SIZE .3 .3 1 L; FX 1.MIX .3 .4 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
             BRK))
    thumb("MBIRA", SL, {"MODEL": "FM", "TIMBRE": 0.1, "COLOR": 0.94, "AD DEC": 0.25, "AD>TIMB": 0.35},
          FL("LP", 0.4, 0.15, 0.1, 0.0, 0.7, 0.2), {"DECAY": 0.72}, CHO(0.3, "CHORUS", 0.3, 0.4, 0.9), REV("PLATE", 0.35, 0.15),
          "An mbira: a buzzing, slightly inharmonic tine with a longer ring, a little resonance and a wide chorus.",
          tk(("The buzz and the edge go from a soft hum at 0 to a bright, rattling tine; the top is clangy and metallic.",
              "SRC.TIMBRE .45 0 1 SL; SHP.CUTOFF .5 0 1 L; SHP.RESO .2 .3 1 SL; SHP.AUDIO FM .5 .6 1 SL; SRC.COLOR .05 .6 1 SL"),
             SHV,
             ("A dry tine to a plate and a wide chorus; past 65% the chorus feeds back and the ring runs on.",
              "FX 2.MIX .3 0 .5 L; FX 2.SIZE .3 .3 1 L; FX 1.MIX .3 .4 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
             BRK), macros=(0.5, 0.3, 0.4, 0.3))

    thumb("SANSULA", SL, {"MODEL": "FM", "TIMBRE": 0.05, "COLOR": 0.9, "AD DEC": 0.3, "AD>TIMB": 0.3},
          FL("LP", 0.4, 0.05, 0.05, 0.0, 0.7, 0.2), {"DECAY": 0.75, "RELEASE": 0.45}, CHO(0.4, "CHORUS", 0.2, 0.5, 0.9), REV("HALL", 0.6, 0.25),
          "A sansula: a soft, round tine with a long warm ring, a deep chorus and a hall.",
          tk(("The tine goes from a dark, muted hum to a bell-like shimmer; the top is bright and glassy.",
              "SRC.TIMBRE .4 0 1 SL; SHP.CUTOFF .5 0 1 L; SRC.AD>TIMB .3 .2 1 L; SHP.AUDIO FM .5 .6 1 SL; SRC.COLOR .05 .6 1 SL"),
             SHV,
             ("A small room to a cathedral with the chorus swelling; past 65% the chorus feeds back and notes pile into a wash.",
              "FX 2.MIX .3 0 .5 L; FX 2.SIZE .3 .3 1 L; FX 1.MIX .3 .4 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
             BRK), macros=(0.4, 0.3, 0.5, 0.3))

    thumb("BOX KALIMBA", SL, {"MODEL": "FM", "TIMBRE": 0.06, "COLOR": 0.91, "AD DEC": 0.12, "AD>TIMB": 0.5},
          FL("LP", 0.6, 0.15, 0.1, 0.0, 0.7, 0.3), {"DECAY": 0.65, "RELEASE": 0.3}, CHO(0.15, "CHORUS", 0.2, 0.3, 0.7), REV("PLATE", 0.25, 0.12),
          "A box kalimba: a woody, resonant body behind a short bright tine, a touch of chorus and a small room.",
          tk(("The body goes from a dull wooden knock to a hollow, resonant box; the top is a hard, ringing, nasal tone.",
              "SRC.TIMBRE .4 0 1 SL; SHP.CUTOFF .45 0 1 L; SHP.RESO .35 .2 1 SL; SHP.AUDIO FM .5 .6 1 SL; SRC.AD>TIMB .3 .2 1 L"),
             SHV,
             ("A dry box to a bigger room and a swelling chorus; past 65% the chorus feeds back and the knock rings on.",
              "FX 2.MIX .3 0 .5 L; FX 2.SIZE .3 .3 1 L; FX 1.MIX .3 .4 1 L; FX 1.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
             BRK), macros=(0.5, 0.3, 0.3, 0.3))

    thumb("BELL KALIMBA", SD, {"MODEL": "FM", "TIMBRE": 0.1, "COLOR": 0.93, "AD DEC": 0.15, "AD>TIMB": 0.4},
          FL("LP", 0.55, 0.05, 0.05, 0.0, 0.7, 0.2), {"DECAY": 0.7, "RELEASE": 0.5}, DLY("1/8D", 0.3, 0.2, "PING"), REV("HALL", 0.6, 0.25),
          "A kalimba with a bell tine: a higher, glassier overtone that rings on, a ping-pong delay and a hall.",
          tk(("The strike goes from a soft thud to a glassy, ringing bell; the top is a hard clanging edge.",
              "SRC.TIMBRE .4 0 1 SL; SHP.CUTOFF .45 0 1 L; SRC.AD>TIMB .3 .2 1 L; SHP.AUDIO FM .5 .6 1 SL; SRC.COLOR .05 .6 1 SL"),
             SHV,
             ("The delay and hall open; past 65% the delay feeds back and its repeats darken into a wash.",
              "FX 1.MIX .3 0 .5 L; FX 1.FEEDBACK .4 .6 1 SL; FX 1.TONE -.4 .6 1 L; FX 2.SIZE .3 .3 1 L; AMP.RELEASE .4 .5 1 S"),
             BRK), macros=(0.5, 0.3, 0.4, 0.3))

    thumb("HAZE KALIMBA", SD, {"MODEL": "FM", "TIMBRE": 0.06, "COLOR": 0.95, "AD DEC": 0.2, "AD>TIMB": 0.35},
          FL("LP", 0.4, 0.05, 0.05, 0.0, 0.7, 0.2), {"DECAY": 0.8, "RELEASE": 0.6}, DLY("1/4D", 0.45, 0.3, "PING"), REV("HALL", 0.85, 0.4),
          "A kalimba lost in a haze: soft tines into a long dotted delay and a very large hall, so each note blooms and fades.",
          tk(("The tine goes from a faint, muffled pluck to a bright pluck above the haze; the top is glassy and ringing.",
              "SRC.TIMBRE .4 0 1 SL; SHP.CUTOFF .5 0 1 L; SRC.AD>TIMB .3 .2 1 L; SHP.AUDIO FM .5 .6 1 SL; SRC.COLOR .05 .6 1 SL"),
             SHV,
             ("The haze deepens: a clear pluck at 0 to a drowned bloom by the top; past 65% the delay feeds back and runs away.",
              "FX 1.MIX .3 0 .5 L; FX 1.FEEDBACK .35 .6 1 SL; FX 2.SIZE .25 .3 1 L; FX 2.MIX .25 .4 1 L; AMP.RELEASE .4 .5 1 S"),
             BRK), level=0.5, macros=(0.5, 0.3, 0.5, 0.3))

    # leads: five older leads rebuilt on the knob layer, one new flute and bowed line; all mono and legato
    RND = [["RANDOM", "PITCH", 0.0]]
    GRIT = ("Resonance and drive, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L")
    a(S("SAW LEAD", cat="LEAD", move="medium", level=0.5, rows=RND,
        summary="A mono saw lead: a filter that opens with the playing, Tides on the colour and a slow vibrato that fades in.",
        k=[("BITE", "A soft, dark saw at 0, brighter and more open by the middle; the top is a hard, ringing, fizzy edge.",
            "SHP.AUDIO FM .5 .65 1 SL"),
           ("VIBRATO", "The vibrato and the filter wobble speed up and deepen; the top is a fast shake and a trembling pan.",
            "ROW 15 .02 .3 1 SL; ROW 16 .3 .4 1 SL; ROW 17 .5 .4 1 SL; LFO.RATE 3 .4 .4 1 SL"),
           ("SPACE", "A dry line at 0, a ping-pong delay and a plate behind it; past 65% the delay feeds back into a wash.",
            "FX 1.FEEDBACK .45 .6 1 SL; AMP.RELEASE .4 .5 1 S; FX 2.SIZE .25 .4 1 L"),
           ("GRIT", *GRIT)]))

    a(S("HOLLOW LEAD", cat="LEAD", move="subtle", level=0.5, rows=RND,
        summary="A hollow, square-ish mono lead with a soft filter swell and a slow Tides drift on the cutoff.",
        k=[("WOOD", "A closed, woody tone at 0, opening to a reedy hollow sound; the top is a hard, nasal buzz.",
            "SHP.AUDIO FM .5 .65 1 SL"),
           ("DRIFT", "The Tides drift and the vibrato speed and deepen; the top is a restless wobble.",
            "ROW 15 .02 .3 1 SL; ROW 16 .3 .4 1 SL; ROW 17 .5 .4 1 SL; LFO.RATE 3 .4 .4 1 SL"),
           ("SPACE", "A dry line at 0, delay and hall behind it; past 65% the delay feeds back.",
            "FX 1.FEEDBACK .45 .6 1 SL; AMP.RELEASE .4 .5 1 S; FX 2.SIZE .25 .4 1 L"),
           ("GRIT", *GRIT)]))

    a(S("FORMANT LEAD", cat="LEAD", move="medium", level=0.5, rows=RND,
        summary="A vowel lead: the formants glide between sung vowels under Tides, with a delay and a hall.",
        k=[("VOWEL", "A closed 'oo' at 0, opening through 'ah' to a bright 'ee'; the top rings with audio FM.",
            "SHP.AUDIO FM .5 .65 1 SL"),
           ("GLIDE", "Tides sweeps the vowel faster and the vibrato deepens; the top is a fast, babbling wobble.",
            "ROW 15 .4 .5 1 L; ROW 16 .4 .5 1 L; LFO.RATE 3 .5 .5 1 SL; LFO.RATE 1 .4 .4 1 SL; SRC.COLOR .4 .4 1 L; SHP.CUTOFF .45 .3 1 L; FX 2.MIX .35 .5 1 L; FX 2.SIZE .3 .5 1 L"),
           ("SPACE", "A dry vowel at 0 to a hall full of choir; past 65% the delay feeds back.",
            "AMP.RELEASE .4 .5 1 S; FX 2.SIZE .25 .4 1 L"),
           ("GRIT", "Resonance and drive, then a per-note pitch scatter, a bit-crush and a feedback smear near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.5 .7 1 T; SRC.DRIFT .4 .3 1 L; SRC.COLOR -.3 .6 1 SL; SHP.CUTOFF -.35 .6 1 SL")]))

    a(S("HARD SYNC", cat="LEAD", move="medium", level=0.5, rows=RND,
        summary="A hard-sync lead whose sweep rings out as each note opens: the classic tearing sync tone, with Tides on the timbre.",
        k=[("SWEEP", "A gentle, almost pure tone at 0, the sync sweep opening wider to the middle; the top tears.",
            "SHP.AUDIO FM .5 .65 1 SL"),
           ("TEAR", "The Tides rate and the vibrato speed and deepen; the top is a fast, chopped, tearing wobble.",
            "ROW 15 .02 .3 1 SL; ROW 16 .3 .4 1 SL; ROW 17 .5 .4 1 SL; LFO.RATE 3 .4 .4 1 SL"),
           ("SPACE", "A dry sync line at 0, a delay and plate behind it; past 65% the delay feeds back.",
            "FX 1.FEEDBACK .45 .6 1 SL; AMP.RELEASE .4 .5 1 S; FX 2.SIZE .25 .4 1 L"),
           ("GRIT", "Resonance and drive, then a per-note pitch scatter, a bit-crush and a feedback smear near the top.",
            "ROW @1 .08 .6 1 SL; SRC.BITS -.4 .78 1 T; SRC.DRIFT .5 .3 1 L; FX 1.FEEDBACK .35 .6 1 SL")]))

    a(S("LD FLUTE", name="FLUTE", cat="LEAD", move="subtle", level=0.5, rows=RND, drop=[(2, "SRC.COLOR"), (1, "SRC.TIMBRE")],
        slots={"SHP": "FILTER", "FX2": "DELAY"},
        edit={"SRC": {"MODEL": "HARM", "COLOR": 0.2, "TIMBRE": 0.1}, "SHP": FL("LP", 0.3, 0.05, 0.05, 0.0, 0.5, 0.1), "FX2": DLY("1/8D", 0.25, 0.1, "PING"), "LFO": LF(("SINE", 0.7), ("DRIFT", 0.2), ("TRI", 0.15))},
        lfo_tgt=[("PITCH", 0.0), ("SHP.CUTOFF", 0.03), ("PAN", 0.05), ("SRC.TIMBRE", 0.0), None, None],
        summary="A mono flute: a few soft harmonics with a slow attack, a vibrato that fades in and the knob brings up, and a hall.",
        k=[("BREATH", "A soft, pure tone at 0, more harmonics and edge by the middle; the top overblows, hard and reedy.",
            "SHP.CUTOFF .6 0 1 L; SRC.TIMBRE .35 0 1 L; SHP.AUDIO FM .5 .6 1 SL; SHP.DRIVE .3 .5 1 SL"),
           ("VIBRATO", "A steady note at 0, then a slow vibrato and a drifting breath; the top is a fast, wide flutter.",
            "ROW 13 .02 .5 1 SL; LFO.RATE 1 .45 .3 1 SL; ROW 14 .4 .2 1 SL; ROW 15 .4 .3 1 SL; SHP.CUTOFF .25 0 1 L; SRC.COLOR .25 0 1 L"),
           ("AIR", "A dry flute at 0, a hall and a ping-pong delay around it; past 65% the delay feeds back into a mist.",
            "FX 1.MIX .3 0 1 L; FX 2.MIX .25 .3 1 L; FX 2.FEEDBACK .3 .65 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", "A little resonance, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.5 .78 1 T; SRC.DRIFT .4 .3 1 L; SHP.RESO .3 .6 1 SL")]))

    a(N("STRING LEAD", "LEAD", {"SRC": "BRAIDS", "SHP": "FILTER", "FX1": "REVERB", "FX2": "DELAY"},
        {"SRC": {"MODEL": "CSAW", "TIMBRE": 0.3, "COLOR": 0.3, "DRIFT": 0.08, "LEVEL": 1.0},
         "SHP": FL("LP", 0.35, 0.1, 0.05, 0.25, 0.5, 0.3, "OWN", 0.25, 0.5, 0.6, 0.4),
         "FX1": REV("HALL", 0.7, 0.2), "FX2": DLY("1/8D", 0.25, 0.1, "PING")},
        {"ATTACK": 0.12, "DECAY": 0.5, "SUSTAIN": 0.9, "RELEASE": 0.35, "VEL AMT": 0.5, "MODE": "LEGATO", "GLIDE": 0.08},
        k=[("BOW", "A soft, dark bow at 0, a bright singing bow by the middle; the top is a harsh, scraping, fizzy bow.",
            "SHP.CUTOFF .4 0 1 SL; SRC.TIMBRE .5 .5 1 L; SRC.COLOR .4 .5 1 L; SHP.ENV AMT .3 0 1 L; SHP.AUDIO FM .9 .5 1 L; SHP.DRIVE .4 .5 1 L"),
           ("VIBRATO", "A steady note at 0, a slow vibrato and a drifting bow pressure; the top is a wide, urgent shake.",
            "ROW 13 .02 .3 1 SL; LFO.RATE 1 .45 .3 1 SL; ROW 14 .3 .3 1 SL; ROW 15 .3 .4 1 SL; SRC.COLOR .25 0 1 L; SHP.CUTOFF .25 0 1 L"),
           ("HALL", "A dry bowed line at 0, a hall and a delay around it; past 65% the delay feeds back into a wash.",
            "FX 1.MIX .3 0 1 L; FX 2.MIX .25 .3 1 L; FX 2.FEEDBACK .4 .6 1 SL; AMP.RELEASE .4 .5 1 S"),
           ("GRIT", "Resonance and drive, then a per-note pitch scatter and a bit-crush near the top.",
            "ROW @1 .05 .7 1 SL; SRC.BITS -.3 .88 1 T; SRC.DRIFT .4 .3 1 L; SHP.RESO .3 .5 1 SL; SHP.DRIVE .3 .5 1 SL")],
        lfo=LF(("SINE", 0.7), ("DRIFT", 0.2), ("TRI", 0.15)),
        lfo_tgt=[("PITCH", 0.0), ("SHP.CUTOFF", 0.03), ("PAN", 0.05), None, None, None], rows=RND, macros=(0.5, 0.3, 0.4, 0.3),
        voices=1, level=0.5, move="subtle", kind="",
        summary="A bowed-string lead: a detuned saw with a slow filter bloom on every note, a vibrato the knob brings in, a hall and a ping-pong delay."))
    thumb("GLASS BELLS", SD, {"MODEL": "FM", "TIMBRE": 0.08, "COLOR": 0.95, "AD DEC": 0.3, "AD>TIMB": 0.5},
          FL("LP", 0.5, 0.05, 0.05, 0.0, 0.7, 0.2), {"DECAY": 0.85, "RELEASE": 0.6}, DLY("1/8D", 0.3, 0.2, "PING"), REV("HALL", 0.75, 0.3),
          "Glass bells: a bright FM strike with a long, glassy ring, a ping-pong delay into a large hall.",
          tk(("The strike goes from a soft, round chime to a bright, glassy bell; the top is hard and clanging.",
              "SRC.TIMBRE .45 0 1 SL; SHP.CUTOFF .5 0 1 L; SRC.AD>TIMB .3 .2 1 L; SHP.AUDIO FM .5 .6 1 SL; SRC.COLOR .03 .6 1 SL"),
             SHV,
             ("The delay and hall open; past 65% the delay feeds back and its repeats darken into a wash.",
              "FX 1.MIX .3 0 .5 L; FX 1.FEEDBACK .4 .6 1 SL; FX 1.TONE -.4 .6 1 L; FX 2.SIZE .3 .3 1 L; AMP.RELEASE .4 .5 1 S"),
             BRK), kind="arp", macros=(0.5, 0.3, 0.4, 0.3), level=0.45)
    names = ["FM BELL", "GLASS BELLS", "SYMPATH HARP", "KALIMBA", "MBIRA", "SANSULA", "BOX KALIMBA", "BELL KALIMBA", "HAZE KALIMBA",
             "SAW LEAD", "HOLLOW LEAD", "FORMANT LEAD", "HARD SYNC", "FLUTE", "STRING LEAD"]
    L.sort(key=lambda q: names.index(q["name"]))
    return L
