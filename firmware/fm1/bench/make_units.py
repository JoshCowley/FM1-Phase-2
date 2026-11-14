#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""fm1.toml's gen step: units/, one small C++ file per engine source, each including the real one, so fm1-base's
build.py (which takes its o2 units from inside the app's folder) compiles FM1 Rack's engine and the Mutable modules
the Lite firmware has, from where they are."""
from pathlib import Path

HERE = Path(__file__).resolve().parent
ENG = HERE.parent.parent.parent / "engine"
# FM1 Phase 2: no Plaits (fm1.toml RACK_NO_PLAITS), as the rack firmware; Clouds is in (RACK_CLOUDS)
LITE_OUT = ("elements/", "warps/", "marbles/", "plaits/")

units = HERE / "units"
units.mkdir(exist_ok=True)
want = {}
for src in (ENG / "sources.txt").read_text().split():
    if not src.startswith(LITE_OUT):
        want["mi_" + src.replace("/", "_")] = f'#include "{src}"\n'
want["rack.cc"] = '#include "../../../../engine/rack.cc"\n'
want["grids_nodes.cc"] = '#include "../../../../engine/grids_nodes.cc"\n'
want["rt.cc"] = '#include "../rt.cc"\n'
want["rk.cc"] = '#include "../rk.cc"\n'
for f in units.glob("*.cc"):
    if f.name not in want:
        f.unlink()
for name, text in want.items():
    f = units / name
    if not f.exists() or f.read_text() != text:
        f.write_text(text)
print(f"units: {len(want)} C++ units")
