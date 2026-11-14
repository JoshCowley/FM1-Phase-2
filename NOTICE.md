# Licences and notices

What each body of code in (or built with) FM1 Phase 2 is licensed under, **as stated in the files themselves**. This is
a record of what was found, not legal advice and not a legal opinion. Where the files do not settle something it says so.
Every row names the evidence. Copyright lines and the full permission text stay in the headers of the files they belong
to; this document does not replace them.

## Summary

| Body of code | Licence found | Where | Evidence |
|---|---|---|---|
| This repository (the project's own code: `engine/`, `firmware/`, `web/`, `sound/`, `manual/`, `tools/`) | **GPL-3.0** | in this repo | `LICENSE` (GPL version 3 text); README "Licence"; `firmware/fm1/rack/fm1.toml` `licence = "GPL-3.0"`; each own source file now starts with `SPDX-License-Identifier: GPL-3.0` (check: `python3 tools/check_licence_headers.py`) |
| Mutable Instruments STM32 module code: Braids, Rings, Tides 2, Peaks (device build); Clouds (this step: simulator and later device); Marbles, Warps, Elements, Plaits (browser/bench builds only) | **MIT** | not in this repo: a checkout of `pichenettes/eurorack`, found beside it at build time (`EURORACK=...`) | the MIT permission text in the header of the module's `.cc`/`.h` files. Counted in the checkout used here (commit `f4cfa76`): Clouds 53 of 53 files, Warps 41 of 41, Braids 43 of 44, Rings 54 of 55, Tides 2 42 of 44, Peaks 47 of 48, Marbles 53 of 56. Files in those modules without the MIT text, and `test/` files (which carry GPL text), are not compiled in |
| stmlib (Mutable's support library) | **MIT** "except when noted otherwise" (its `LICENSE`) | same checkout | MIT header on the `dsp/`, `utils/`, `algorithms/` and `fft/` files the engine includes. Not used by the DSP path and not MIT: `third_party/STM` (ST's own notice), `programming/serial` (GPL), `ui/event_queue.h` (GPL header). In the checkout used, 40 of 345 files under `stmlib/` carry the MIT text, so check any new stmlib include file by file |
| Mutable Instruments **Grids** (pattern generator: the drum-map tables and the pattern lookup) | **GPL-3.0, or any later version** | tables in `engine/grids_nodes.cc` (copied from `grids/resources.cc`); the pattern lookup reimplemented in `engine/rack.cc` (`grids_level`) | the header of `grids/resources.cc` in the checkout: "version 3 of the License, or (at your option) any later version"; the Mutable README names GPL3.0 for the AVR projects. Josh's decision: Grids is kept. Fits the GPL-3.0 whole; it is the reason Grids cannot be counted among the MIT parts |
| fm1-base (the FM-1 platform the firmware is built on: drivers, USB, build tool) | **MPL-2.0**, file by file | not vendored: fetched at build time. This repo's `app.c`/`app.h` and ten other firmware UI files (`firmware/fm1/rack/{frame,menus,play,save,seq,seqpages,vis,vis3}.c`, `firmware/fm1/bench/app.{c,h}`) are MPL-2.0 files, modelled on fm1-base's template | `SPDX-License-Identifier: MPL-2.0` in those 12 files; fm1-base's `LICENSING.md` ("fm1-base is MPL-2.0 ... Your own files, such as an app built on fm1-base, may use any licence") |
| Felucca (heritage) | GPL-3.0-only upstream; its board layer (felucca-bsp) MPL-2.0; "its six engines are MIT" (as `firmware/PORTING.md` records from Felucca's own LICENSING.md) | the FM-1 platform work this project builds on. fm1-base's `LICENSING.md` says it holds its Felucca-derived copies under MPL-2.0 by permission of the code's owner | `firmware/PORTING.md` ("Felucca's licence"); README ("Grids and the Felucca platform are GPL-3.0, so the project as a whole is GPL-3.0"); `firmware/hw/fm1_platform.h` says Felucca's own function names are not copied. **No Felucca source file was identified in this repository by this audit**; the README's wording is kept as the repo states it |
| Fonts: IBM Plex Sans Condensed (the FM-1 screen font is baked from it by `make_font.py`; `.woff` files in `firmware/fm1/rack/fonts/`); DotGothic16 (the browser page) | **SIL Open Font License 1.1** | `firmware/fm1/rack/fonts/OFL-IBM-Plex.txt`; the page loads DotGothic16 from Google Fonts | the OFL text in the repo; README "Screen font ... (SIL Open Font License)" |
| GRAIN (the home-made granular source: `engine/grain.h`, `render_grain` in `engine/rack.cc`) | **GPL-3.0** (project's own code) | shelved behind `RACK_WITH_GRAIN` (off by default) | its files carry the project's header; not Mutable code. Its mu-law table is the standard G.711 scheme |
| JieLi AC79 SDK files, Bluetooth stack parts (SBC, micro-ecc) in fm1-base | Apache-2.0, BSD-2-Clause (per fm1-base's `LICENSING.md`) | not in this repo; the firmware here sets `ble = false` | fm1-base `LICENSING.md`. Never downloaded or used in this work |

## Notes on what the files say

- **Whole firmware.** The firmware as a whole is declared GPL-3.0 (`fm1.toml`). It contains MIT code (Mutable modules,
  stmlib), GPL-3.0-or-later data and logic (Grids), MPL-2.0 files (fm1-base and the 12 app files), and OFL fonts. The
  Mutable engine code used is MIT; that is a rule set for this project, the parts above are what the headers say.
- **"GPL-3.0" identifier.** The repo states "GPL-3.0" (README, `fm1.toml`, `LICENSE`) without saying "only" or "or
  later". The headers use the SPDX id `GPL-3.0` to match it. Whether to say `-only` or `-or-later` is Josh's choice.
- **MIT notice.** The MIT permission text and the Mutable copyright lines are in each Mutable source file's header. A
  binary that includes those modules should carry that notice text; the text, with its copyright line, is not
  reproduced in this repository yet. See the report's "Check these".
- **Trademark.** The README says "Mutable Instruments" is a registered trademark and that the module names stay on
  screen for now. A copyright licence and a trademark are different things; the README plans renaming before release.
- **Authorship.** The git history has more than one author identity. Who holds copyright in which part is not recorded
  in the files; the headers added here therefore carry no copyright holder line.

## Files that are not the project's own code

`engine/grids_nodes.cc` (Grids data, header says so) and the 12 MPL-2.0 files above. Generated files
(`firmware/fm1/rack/presets.h`, `ui_defs.h`, `firmware/fm1/bench/cases.h`, `cases_full.h`) carry the project's header, written by
their generators.
