# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""WAVE, the firmware's own source module (2.48: Plaits' wavetable engine, Mutable Instruments, MIT; engine/rack.cc, common/make_wave_osc.py).
Not on the browser page, so defined here: make_ui.py writes its page, make_presets.py its ranges. Parameters 0-15 sit where Braids' do
(DRIFT, BITS, RATE, SIGN, LEVEL, OCTAVE, FINE, the AD, its routes and its VCA), so JAM's source moves and the matrix's habits carry over;
16 and 17 are the page 5 extras."""


def _n(l, d): return {"l": l, "t": "num", "def": d}
def _b(l, d): return {"l": l, "t": "bip", "def": d}
def _I(l, lo, hi, d): return {"l": l, "t": "int", "min": lo, "max": hi, "def": d}
def _L(l, o, d, mod=True): return {"l": l, "t": "list", "opts": o, "def": d, **({} if mod else {"mod": False})}


WAVE = {"kind": "src", "poly": True, "cpu": 4, "ram": 0, "ramV": 1.1, "blurb": "4 BANKS / 8 X 8 WAVES", "pages": [
    [_n("BANK", .0), _n("SCAN X", .3), _n("MORPH Y", .3), _n("DRIFT", .1)],
    [_L("BITS", ["2", "3", "4", "6", "8", "12", "16"], 6), _L("RATE", ["4K", "8K", "16K", "22K", "32K", "48K"], 5), _n("SIGN", 0), _n("LEVEL", .8)],
    [_I("OCTAVE", -3, 3, 0), _b("FINE", 0), _n("AD ATK", .05), _n("AD DEC", .4)],
    [_b("AD>X", 0), _b("AD>Y", 0), _b("AD>PTCH", 0), _L("AD VCA", ["OFF", "ON"], 0)],
    [_n("LOWPASS", 1.0), _L("QUANT", ["OFF", "ON"], 0)]]}
WAVE_ID = 5


def wave_meta():
    """param -> (lo, hi, span), as the page's set_meta calls give the other modules"""
    out, i = {}, 0
    for p in [p for pg in WAVE["pages"] for p in pg]:
        if p["t"] == "list": out[i] = (0, len(p["opts"]) - 1, len(p["opts"]) - 1)
        elif p["t"] == "int": out[i] = (p["min"], p["max"], p["max"] - p["min"])
        elif p["t"] == "bip": out[i] = (-1, 1, 2)
        else: out[i] = (0, 1, 1)
        i += 1
    return out
