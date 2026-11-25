# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""CLOUDS in the Lite firmware (FM1 Phase 2 step 5, 14 Nov 2026): the module's parameter ranges, as the page's set_meta
calls give them (web/src/6-engine.js meta()). No preset uses CLOUDS yet, so presets.h's META has no entry for it from
the presets' calls; make_presets.py adds this one, and make_ui.py checks it against the page's definition."""
CLOUDS_ID = 30
# the page's parameters in order (web/src/1-data.js CLOUDS): "n" 0..1, "b" -1..1, an int: a list of that many options
# (the Lite firmware's MODE list has 3 options, GRANULR LOOPDLY SPECTRL: lite_page below; the page's has 4)
KINDS = ["n", "n", "b", "n",              # POSITION SIZE PITCH DENSITY
         "n", "n", "n", "n",              # TEXTURE BLEND SPREAD FEEDBACK
         "n", 3, 2, 4,                    # REVERB MODE FREEZE QUALITY
         "b", 2, 2, 5,                    # IN GAIN NOTE TRG PITCH TRK GRN SYNC
         "n", "n", 4, "n",                # SPRAY PITCH RND PITCH QNT SIZE RND
         "b", "b",                        # SCAN TONE
         "n", "b", "n",                   # WINDOW SPEED JITTER (update 27, the GR-1 style grain controls; the firmware's own, not on the page)
         3]                               # RENDER (update 42: HIGH NORMAL LITE, how much of Clouds' CPU to spend; the firmware's own, not on the page)

# update 27: the firmware's CLOUDS pages, as lists of parameter numbers. The parameters keep their numbers (so every saved preset and
# route still points where it did); only the order of the pages changes: the grain-shaping set first (GRAIN 1: POSITION WINDOW SPEED
# JITTER, GRAIN 2: SIZE DENSITY RND PAN FREEZE), then the rest in the order they had. SCAN stays (older presets use it) on the last page.
GRAIN_PAGE = [{"l": "WINDOW", "t": "num", "def": 0.0}, {"l": "SPEED", "t": "bip", "def": 0.0}, {"l": "JITTER", "t": "num", "def": 0.0}]
# update 42: RENDER (parameter 25) shares the last page with SCAN. Update 44: LIGHT (0, the default: a preset or save that says nothing has it) is Clouds at 16 kHz in half
# the CPU and memory of 2.43's NORMAL; HIGH (1) is 2.40's Clouds exactly; NORMAL (2) is 2.41's default (2.40's sound for 35 % less CPU). LITE (2.41) is retired.
RENDER_PAGE = [{"l": "RENDER", "t": "list", "opts": ["LIGHT", "HIGH", "NORMAL"], "def": 0}]
LAYOUT = [[0, 22, 23, 24], [1, 3, 6, 10], [2, 4, 5, 7], [8, 9, 11, 12], [13, 14, 15, 21], [16, 17, 18, 19], [20, 25]]


def clouds_meta():
    """param -> (lo, hi, span), as func_def.func_meta"""
    return {i: ((0, k - 1, k - 1) if isinstance(k, int) else (-1, 1, 2) if k == "b" else (0, 1, 1)) for i, k in enumerate(KINDS)}


def lite_page(page):
    """the page's CLOUDS entry (evaluated by make_ui.py) as the Lite firmware has it: STRETCH out of MODE (silent with the
    Mutable checkout used: engine/rack.cc RACK_CLOUDS_NO_STRETCH), and DENSITY at 0.7 (at the page's 0.5 GRANULR plays
    no grains: Clouds' DENSITY is silent at noon). Checked against KINDS"""
    flat = [p for pg in page["pages"] for p in pg]
    for p in flat:
        if p["l"] == "MODE":
            assert p["opts"] == ["GRANULR", "STRETCH", "LOOPDLY", "SPECTRL"], "clouds_def: the page's MODE list changed"
            p["opts"] = ["GRANULR", "LOOPDLY", "SPECTRL"]
            p["def"] = 0
        if p["l"] == "DENSITY":
            p["def"] = 0.7
        if p["l"] == "SPREAD":                # the grain's random place in the stereo field (Clouds draws one for each grain)
            p["l"] = "RND PAN"
            p["alias"] = "SPREAD"           # (the presets and routes written with the old name still find it)
    page["pages"].append([dict(p) for p in GRAIN_PAGE])   # parameters 22-24, after the page's 22
    page["pages"].append([dict(p) for p in RENDER_PAGE])  # parameter 25 (update 42)
    page["layout"] = LAYOUT
    check_page(page)
    return page


def check_layout():
    flat = [i for pg in LAYOUT for i in pg]
    if sorted(flat) != list(range(len(KINDS))) or any(len(pg) > 4 for pg in LAYOUT):
        raise SystemExit("clouds_def: LAYOUT is not every parameter once, four to a page")


def check_page(page):
    """page: the CLOUDS entry of the page's MODULES (evaluated by make_ui.py): the same parameters in the same order"""
    flat = [p for pg in page["pages"] for p in pg]
    kinds = [len(p["opts"]) if p["t"] == "list" else "b" if p["t"] == "bip" else "n" for p in flat]
    check_layout()
    if kinds != KINDS:
        raise SystemExit("make_ui: clouds_def.KINDS differs from the page's CLOUDS parameters")
