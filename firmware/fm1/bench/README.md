# FM1 Bench: timing FM1 Rack on a real FM-1

**Status: builds and passes fm1-base's own checks, has not yet run on an FM-1.** This is a test firmware, not FM1
Rack: it plays no sound and saves nothing. It answers two questions on the board itself:

1. **Does the FM-1's chip run hardware floating point?** Felucca's plugin guide says the FM-1 has no FPU. fm1-base,
   whose firmwares do run on the FM-1, builds everything with `-mcpu=r3 -mfprev1` (JieLi's single-precision FPU
   flags, "the SDK's own flags" in its build.py) and its build check refuses double precision. This firmware is
   built the same way, then checks a known single-precision sum at power-on.
2. **How much of the CPU does each module take?** It runs FM1 Rack's real engine (`engine/rack.cc`, built
   `-DRACK_FM1 -DRACK_LITE`, and as the FM1 Phase 2 firmware is built: `-DRACK_NO_PLAITS -DRACK_CLOUDS -DRACK_CLOUDS_NO_STRETCH`,
   arena 304 KB) and times 170 cases (update 10; 164 at update 9; 150 at update 8, 138 before): every Braids shape, every Rings model, the filter, Tides,
   Peaks, Grids, the delay, reverb and chorus, 1 to 6 voices, the CLOUDS group (below), and the 16 heaviest of the Phase 2
   factory presets plus the eight earlier ones (the module cases are still built on eight presets of the first set, kept in `tests/bench-base-calls.json`); then, when the second core starts, the 6-voice ones again on two cores (the 2 CORES group: the second core renders half the voices).

**Update 46 (25 Nov 2026): the cores' arrangement, A/B on the device.** Every factory preset on two cores runs three times: `NAME xN` (the engine's arrangement: 2.42's table, then the learner of rack.cc `split_learn`), `NAME xN 2.42` (the table alone, the reverb early on the first core) and `NAME xN 2.45` (2.45's table with the reverb's job after its slot and 2.43's learner, built in only with `RACK_BENCH_AB`); `rk_arrangement(0/1/2)` switches them at run time. C2 now includes the reverb's job when it runs on the second core (q45's C2 had left it out: a reverb job alone read as C2 0.3). NCASE_MAX 660. Host: `bench_host --arr` (one line a case: voices, the second core's first voice, the reverb job, its slot, the reverb in Clouds' job), `--proxy NAME...` (each core on this computer, with the arrangement it ended in). The cost model fitted on the device's q45 CSV is `model/core_model.py` (host counts of each part: `model/host_components.py`, `comp242.json`, `arr245.tsv`; the CSV `model/q45.csv`): `python3 core_model.py --out predictions.csv`.

**Update 46, result (q46, 25 Nov 2026) and 2.47's default.** Busier core max(C1,C2) over 77 presets: 2.42's table 46.0 mean (worst 80.1), 2.45 47.2 (81.5), the engine's learner 46.9 (82.7); the learner beat 2.42 in 3 cases (STRING BLOOM x6 -5.6, TINE EP x6 -1.9, TIDE PAD x4 -1.0) and lost in 71; blocks 50.5 / 52.2 / 51.6. The learner left the split alone in almost every case (C2 up more than 3 in one), and both cores and the block rose by about 1 point everywhere, one voice or six: a cost, not a mis-set split. 2.45's reverb job did help the reverb-heavy presets (ATMOSPHERE x4/x6 -6.4/-7.9, SHIMMER PAD x4/x6 -2.3/-2.7, FUNC PULSE x6 -2.1 on the busy core) but lost on average. So 2.47 builds with the learner and the reverb's job on the second core OFF (`RACK_LEARNER` puts them back; `rk_arrangement(0)` is then 2.42's table, as 1). `model/q46.csv` is q46 (first of each duplicate); `core_model.py` now fits q45 and q46 together (a run offset a core, +3.2 and +3.7 points for q46) and checks its prediction of the learner against q46: it predicted 43.1 mean for the learner (39.0 before the refit) against 46.1 measured on the same 75 cases (error -3.0, rms 3.8), i.e. it did not predict that the learner would lose.

**Update 59 (21 Nov 2026): 48 kHz with the grain ceiling 24, A/B on the device.** 279 cases (60 more). The firmware's ceiling is now 24 (rack/README.md, update 59); `RACK_CLOUDS_MAX_GRAINS` of this build is 24 too,
but every older Clouds row still runs at 40 as before (at 16 ST that is no ceiling: 32 grains), so the CSV compares with the modsum1 run row for row. New CLOUDS rows (CLC rows 32 to 36): `G.7 CAP24`, `G.9 CAP24` (32 kHz, 24; `G1.0 CAP24` was there),
`G.7 48K CAP24`, `G.9 48K CAP24`, `G1.0 48K CAP24` (RACK_CLOUDS_32K off: no resampler, with 24). New DRUMS (one core) `CHEAP x4 D1.0 CAP24`, `RICH x4 D1.0 CAP24` and `... 48K CAP24`; new 2 CORES `GRAN D1.0 CAP24`, `CHEAP x4 D1.0 CAP24`,
`RICH x4 D1.0 CAP24` and the same three `48K CAP24`, each with C1 and C2. **At 48 kHz Clouds does not go to the second core** (`cl_pipe_decide` needs the 32 kHz block): the 48K 2 CORES cases run Clouds on the first core and split the voices and the
drums, as update 8 did; read C1 as well as C2. The PRESETS group and the 2 CORES presets now play the current bank (the 45 presets after INIT, from `tests/presets-fm1-calls.json`; the older sets' names are gone from it; `make_cases.py` takes the list from there).
The ops of cases.h are 16 bytes (`bop_t`: x, three whole-number ints, five small ints; was 24) so the 54 presets fit the app slot (image 571,624 of 581,564 B); `NCASE_MAX` 320. `bench_host` (the 2-core twin check): a 48 kHz row has no block delay; a preset with Clouds (TIDE PAD) is piped
like the rows. Its 29 failures are the 21 of update 10 (the kits' fourth voice, host only) and the eight new kit cases of the same kind. CSV name for the run: `fm1-bench-k48cap1-2026-11-22.csv`.

**Update 64 (24 Nov 2026): the quality A/B.** Every factory preset on two cores at six voices (the mono presets one note) twice: `NAME x6` as the engine is (the 2.38 quality trims, `Q_DEFAULT`) and `NAME x6 OLDQ` with the 2.37 quality (`set_quality(0)`: reverb sum, halfband and reverb diffusion trims off; the exact savings are in both). 90 cases, in the 2 CORES group after the presets on their own voices. The difference between a pair's core 1 and core 2 figures is the CPU the trims save on the FM-1. The overload guard is the firmware's (GLOBAL > CPU + MEMORY, K4, default OFF), not the engine's: the bench never sheds a voice; a worst block over 90 % of a core is where the guard, if ON, would shed one. Host figures: `run_ir_par.sh` (IR_NOTES=6 for six voices, IR_Q=0 for the 2.37 quality).

**MODSUM (23 Nov 2026): the mod_sum precompute, A/B on the device.** 12 new cases (219 in all, group MODSUM, one core): ROTARY ORGAN (the most LFO routes), TIDE PAD (the heaviest) and SHIMMER PAD, whole (`modsum_cases.h`, from `make_modsum_cases.py`; cases.h has no knob layer), four voices held, each with the knobs `still` or `moving` (all four glide every step, so the knob layer's sums are rebuilt every block), each as `NEW` (the engine as the firmware is: the matrix, knob and JAM sums kept ahead) and `OLD` (`rk_modsum_precompute(0)`, built with `-DRACK_BENCH_AB`: every call scans the 18 rows and the knob targets, as 2.32 did). OLD's own figure is OLD minus NEW: the CPU the precompute saves. `bench_host` checks that each OLD/NEW pair gives the same samples, bit for bit (6 pairs). On the host (instruction counts, 16 presets) the scratch patch saved 1.4 to 5.6 %; the device figure is what this run is for.

**Drum track, step 1 (21 Nov 2026): the DRUMS cases.** The firmware now has four drum voices (engine/rack.cc "DRUMS": Braids' nine drum models in a trimmed oscillator, 472 bytes a voice, built
by `common/make_drum_osc.py` from the Mutable checkout). 37 new cases (207 in all) measure what they cost. The DRUMS group (one core) and the same in 2 CORES: `CHEAP x3`, `CHEAP x4`, `RICH x3`, `RICH x4`
(CHEAP: KICK, SNARE, a CLOCKED NOISE hat, and for x4 a FILTERED NOISE clap; RICH: KICK, SNARE, the CYMBAL hat, a STRUCK DRUM perc), each alone (own = minus `BRAIDS x4`: the firmware's P2 INIT, four notes) and with
Clouds (GRANULR 16 ST, the CLOUDS group's settings) at DENSITY 0.7, 0.9 and 1.0 (`... D.7`, `D.9`, `D1.0`; own = minus the same Clouds case without drums), and `CHEAP x4 D1.0 EASED` and `RICH x4 D1.0 EASED` (the engine's
IF BUSY = CLOUD guard on: Clouds' DENSITY held at 0.9 while the drums run; in the other cases it is off, so D1.0 is a true 1.0). Every voice of the kit is struck every 10 ms, so all of them sound all the time: the worst
case, not the average. On two cores the second core renders the drums (in Clouds' job when Clouds is loaded, else the voices' job); each such case gives the block, `C1` and `C2` as before, and `BRAIDS x4` on two cores is
their reference. CSV name for the run: `fm1-bench-drums1-2026-11-21.csv`. `bench_host` checks that every voice of every kit sounds.

**Update 10: the work around Clouds is the second core's too.** Six new cases (170 in all): in the 2 CORES group `GRAN D0.9 x6` and `CL+DELAY x6` (six voices; the fifth and sixth ride in
the second core's job when Clouds is in FX 1 with an effect after it: `CL+DELAY x6`, `CL+REV x6`), and `GRAN D0.9 x6 V1` and `CL+REV x6 V1` (the same with the voices kept on the first core, `rk_fx_core2_voices(0)`: update 9's
arrangement, to see whether the offload helps on the device); in the CLOUDS group the one-core twins `GRAN D0.9 x6` and `CL+DELAY x6`. Read "C1" and "C2" as before; at update 9 C1 was 55.7 / 56.2 / 56.7 % at GRANULR D0.7 / D0.9 / D1.0,
and the aim is C1 near 32 and C2 under 80. `bench_host --proxy NAME...` prints each core's work on this computer (a proxy). The host check of a six-voice Clouds case against its one-core twin compares rms (within 8 %): the voices' drift and Clouds' grains draw random numbers in another order on two cores.

**Update 9 (16 Nov 2026): Clouds on the second core, measured per core.** The build has `RACK_FX_CORE2` on: with two cores, Clouds runs alone on the second core (a block behind), every voice
and the other effect on the first (rack/README.md, Update 9). The 2 CORES group now has, besides the three Clouds cases it had (`GRAN D1.0`, `CL+REVERB`, `CL+DELAY`, now the new split),
`GRAN D0.7`, `GRAN D0.9`, `CL+REV x6` (CL+REVERB with six voices), `LOOPDLY`, `SPECTRL`, and **the eight again with the arrangement switched off at run time** (`... OFF`: update 8's: the voices split,
Clouds on the first core; `rk_fx_core2(0)`), 14 new cases (164 in all; the one-core twin `CL+REVERB x6` is in the CLOUDS group). **Each 2 CORES case is three lines** in the screen's list, the CSV and the reader (same columns):
the case itself (CPU %: the block, i.e. the slower core, what the status strip shows), then `<name> C1` (the first core's work: the block less the time it spent waiting for the second) and `<name> C2` (the second
core's job), each as % of the block; "slowest run us" is that core's slowest run. On the screen the case's line shows `C2 x.x` beside its CPU %. On a computer the cores' lines mean little (one thread).
All with 4 voices unless the name says 6. CSV name for the update 9 run: `fm1-bench-clouds3-2026-11-15.csv`.

**Update 8 (16 Nov 2026): Clouds' three changes, each measurable from one run.** The build has the three flags on (`RACK_CLOUDS_32K`, `RACK_CLOUDS_MAX_GRAINS=40`,
`RACK_CLOUDS_SKIP_REVERB`; rack/README.md, Update 8) and the bench's Clouds cases run with them; twelve cases (own = minus `BRAIDS x4`) change them per case at run time
(`rk_clouds_opts`; the firmware never does): `G.7 48K`, `G.9 48K`, `G1.0 48K` (GRANULR 16 ST at DENSITY 0.7, 0.9, 1.0 at 48 kHz, to compare with `GRAN D0.7`, `GRAN 16ST`, `GRAN D1.0`,
which run with all three on); `G.7 OLD`, `G.9 OLD`, `G1.0 OLD` (all three off: update 7); `G1.0 NOCAP` (no grain ceiling: Mutable's own 32 at 16 ST: the same as `GRAN D1.0`, as the
ceiling does not act there), `G1.0 CAP24` (a ceiling of 24: what a lower one would buy); `8MO D1.0` and `8MO NOCAP` (8-bit mono, where the ceiling takes 57 to 40); `CL REV0` and `CL REV0 NOSKIP`
(Clouds' own REVERB 0, the skip on and off). Every Clouds case now warms for 2 s (was 1 s): at 32 kHz the 8-bit mono buffer fills 1.5 x slower and GRAN 8MO was still silent at 1 s.

**Plaits is out of this build (15 Nov 2026), as in the FM1 Phase 2 firmware.** The 24 PLAITS cases are gone; the eight
first-set presets the module cases stand on ask for Plaits and now play Braids at their own settings (a source that
asks for Plaits plays Braids), so the FILTER, MOD, FX and VOICES figures (the VOICES cases are now called `INIT x1` to `x6`) are NOT comparable with
earlier runs. `cases.h` was also regenerated from the current presets (`tests/presets-fm1-calls.json`: the 54 re-voiced on
Braids), which the 14 Nov file predated. That freed the flash the CLOUDS code needs.

**CLOUDS (FM1 Phase 2, 15 Nov 2026).** A group for Clouds (module 30, FX 1) on the firmware's own INIT (`P2 INIT`: a
Braids tone) with **four notes held** unless the name says otherwise. Every case warms for 1 s (not 0.1 s): Clouds' grains read what it has recorded,
and after 0.1 s a granular cloud is still silent. Settings of every Clouds case: POSITION 0.2, SIZE 0.5, PITCH 0,
**DENSITY 0.9** (cases named D0.7 and D1.0 say otherwise), TEXTURE 0.4, BLEND 1.0 (full wet), SPREAD 0.5, **FEEDBACK 0.3**,
**REVERB 0.3** (Clouds' own), the rest at the page's defaults (no spray, pitch random or quantise, scan or tone); MODE and QUALITY as the name says.
Each case's "own" figure is its difference from the case named in brackets:

- `BRAIDS x4`: the reference, no effects.
- `GRAN`, `LOOP`, `SPEC` (GRANULR, LOOPDLY, SPECTRL) x `16ST`, `8ST`, `16MO`, `8MO` (QUALITY 16 ST, 8 ST, 16 MONO, 8 MONO): Clouds alone, 12 cases; own = minus `BRAIDS x4`.
- `GRAN D0.7` (the default density) and `GRAN D1.0` (the most): GRANULR 16 ST, to show the range.
- `GRAN FREEZE`: GRANULR 16 ST with FREEZE switched on half way through the warm-up (with the notes still held).
- `REVERB x4` and `DELAY x4`: the same Braids patch with CH GLASS PNO's reverb or LD SQUARE CHIP's delay in FX 2 (no Clouds); own = minus `BRAIDS x4`.
- `CL+REVERB` and `CL+DELAY`: `GRAN 16ST` plus that reverb or delay in FX 2; own = minus the same without Clouds (`REVERB x4`, `DELAY x4`), i.e. Clouds' share when the other effect is there. **The firmware lets the reverb (then the delay) give way when the arena is short:** the `dropped` column says if it did (0 for none; 20 hex = FX 2 dropped).
- `BRAIDS x1`, `x2`, `x6` and `CL 16ST x1`, `x2`, `x4`, `x6`: GRANULR 16 ST (as `GRAN 16ST`; `x4` is a repeat of it) with 1, 2, 4, 6 notes held, each minus Braids with the same notes. The `voices` column says how many the arena fits.
- In 2 CORES: `GRAN D1.0`, `CL+REVERB`, `CL+DELAY`, the three with the highest total CPU in each of four runs on a computer (a rough guide: the computer is far faster than the FM-1 and its figures are noisy). The second core takes two of the four Braids voices; Clouds runs once, on the first core.

**GRAIN (FM1 Phase 2, 14 Nov 2026; shelved at step 4: these cases exist only when built with `-DRACK_WITH_GRAIN`, off by default).** A group for the new granular source (`engine/grain.h`, rendered in rack.cc):
one voice at 4, 8, 16 and 32 grains, then 4 and 6 voices at 8, 16 and 32, and the heavier ones again on two cores.
Every grain slot is kept full (316 ms grains wanted 1000 times a second), so each figure is the worst case for that
many grains. The buffer holds a test recording until capture is built. These cases answer how many grains a voice
the FM-1 carries. Seven of the played presets (all measured on 13 Nov) were left out to make room.

## What you see

The FPU check comes first, at the top of the screen: **FPU OK: HARDWARE FLOAT, EXACT** means the float instructions
ran and gave the IEEE result. (If the chip had no FPU, the firmware would stop at its first float instruction
instead, and fm1-base's crash screen would show.) Then the cases run, one after another (the CLOUDS ones warm for 1 s each: a few minutes in all).
Each line shows:

- **CPU %**: how much of one core that case takes, at the engine's 48 kHz. 100 % is the whole core; over 100 %
  means it can't run in real time. It's the fastest of 5 timed runs (interrupts only ever add time).
- in brackets, for modules measured by difference, **its own share**: the case minus the same preset without it
  (Rings, the filter, Tides, Peaks, Grids, the effects).
- otherwise the **voices** playing (the arena may fit fewer than asked for).
- **SIL** or **NAN** if a case made silence or a bad number (none should).

PRESET or ALGORITHM scrolls; OCT- and OCT+ page; REC runs everything again. **PLAY sends every result over USB-MIDI**:
open `reader.html` in Chrome or Edge, press Listen, then PLAY on the FM-1, and save the table as CSV.

Results are what to send back, so the page's CPU model and the module choice for FM1 Rack Lite can use real numbers.

## Before you install it

Only after you have done the Felucca round trip (PORTING.md, step 1): Felucca installed with its web installer, then
**Return to official V15**, at least once. That proves your FM-1, cable and computer can get back to stock.

This firmware is built on fm1-base. From fm1-base's code and installer page (not yet tried with this firmware):
fm1-base firmwares keep Felucca's update entry, so a running one takes the next update over USB the way Felucca
does, and its installer's "Back to the stock firmware" writes M-VAVE's firmware back from it; after two failed starts
it comes up in a safe mode (no audio, USB on) that takes updates too, and after four, the chip's own update mode,
where M-VAVE's M-UPGRADE works. The benchmark runs the engine in the main loop, not the audio interrupt, so a slow case
only takes longer; it can't stall the sound, and it does 10 ms of sound per screen frame, so a step stays far
under the 8 s watchdog unless a case needs hundreds of cores.

## Build it (Mac or Linux)

You need Docker (on a Mac; build.py runs JieLi's Linux toolchain in a linux/amd64 container), Python 3, git, and
three checkouts side by side:

```
work/
  MODFM1/          this repo
  fm1-base/        git clone --recursive https://github.com/charlesvestal/fm1-base
  eurorack/        git clone --recursive https://github.com/pichenettes/eurorack
```

Then, from `fm1-base/`:

```
tools/get_toolchain.sh                     # JieLi's toolchain into ~/.jieli (prints JIELI_TOOLCHAIN=...)
bsp/tools/get_sdk.sh                       # the three AC79 SDK files, from JieLi's GitHub, hash-checked
export JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/.jieli/ac79-sdk
tools/build.py ../MODFM1/firmware/fm1/bench
```

It should end with `ok` checks (no soft-double routines, image about 496 KB of the 568 KB app area (15 Nov 2026 build), pool within its
headroom) and `package .../build/fm1bench.fwsc, identity FM-1_7700`. Use only a package you built yourself from
fetched SDK files whose hashes passed.

Install it with fm1-base's installer (needs `pip install mido python-rtmidi`):

```
python3 tools/fm1_install.py ../MODFM1/firmware/fm1/bench/build/fm1bench.fwsc
```

To go back: install Felucca again, or M-VAVE's firmware with M-UPGRADE (or fm1-base's installer page, made by
`tools/make_site.py`, which has "Back to the stock firmware").

## Checked on a computer first

`sh build_host.sh && build/host/bench_host` (with `EURORACK` set if the checkout isn't beside MODFM1) builds the same
cases and engine with the computer's compiler and checks that every case makes sound, that each preset loaded as
INIT plus its differences (`cases.h`) sounds the same as the preset's own calls, sample for sample, and the FPU
check's expected value. Its timings are the computer's, not the FM-1's.

## Files

| File | What |
|---|---|
| `fm1.toml`, `app.h`, `app.c` | The fm1-base app: settings, screen, buttons, the SysEx dump |
| `bench.c` | The cases and how each is timed (shared with `bench_host.c`) |
| `cases.h` | The presets' engine calls, from `firmware/tests/presets-calls.json` by `make_cases.py` |
| `rk.cc` | The engine's C interface under names that don't clash with fm1-base's |
| `rt.cc` | What C++ needs and fm1-base doesn't link: a few runtime stubs and single-precision maths on fm1-base's `fastmath.h` |
| `make_units.py` | Run by the build: `units/`, one file per engine source |
| `../../hw/libc/` | C headers for a build with no C library |
| `reader.html` | Shows and saves the results the FM-1 sends |

## Known limits

- The maths functions (`sinf`, `expf`, `powf`...) are fastmath.h's approximations, not a full libm: the sound is
  close to the computer's, not identical. Timing is what matters here.
- `id_prefix = "FM-1_77"` was picked for this test and isn't registered anywhere.
- The FM-1's codec runs at 44.1 kHz and the engine at 48 kHz: the figures are for rendering 48,000 frames a second.
- The engine keeps some state (delay lines, the clock) from one case to the next; each case starts from the same
  random seed, so runs are comparable, but a case's sound can differ slightly from the same case run alone.

## Update 42 cases

The bench firmware runs the A/B cases only: BRAIDS x4, 18 Clouds cases (GRAN / LOOP / SPEC, D0.7 / D1.0, HIGH / NORMAL / LITE) and, for every preset, `NAME x6` / `x6 OLDQ` / `x4` / `x4 OLDQ` (OLDQ is the 2.40 quality mask). The host tools (`bench_host`, `bench_ir`) build the whole set. The exact, non-quality savings cannot be switched off here.
