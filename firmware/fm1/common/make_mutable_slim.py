# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""FM1 Phase 2 2.42 (the space-saving pass): changed copies of a few Mutable files (the checkout itself is never edited).

make_units.py (the rack firmware's) writes what files() returns: the .cc copies as units (mi_*.cc) and the headers under units/, which the
build's include path puts ahead of the Mutable checkout. The MIT notice stays at the top of each copy (NOTICE.md). Two kinds of change,
both leave every sample the firmware makes exactly as it was:

1. Tables that Mutable declares without `const`, so they sit in the FM-1's RAM (static RAM is 98,304 B and was nearly full) although nothing
   writes them: Braids' three render-function tables and its waveform table, Peaks' LFO function table and look-up table
   table, Rings' model gains. Declared const here (header and definition), they go to flash: about 1.4 KB of RAM.
2. Braids' DigitalOscillator (RACK_BRAIDS_TRIM, set by the firmware's build and the host build): the 14 shapes the firmware has withdrawn
   (engine/rack.cc BR_KEPT: FBFM PLUK BOWD BLOW FLUT and the nine drum shapes), and the shape past the end of the list (QuestionMark) never reach it, so their render functions are not compiled and
   their table entries point at a function that writes silence. The drum shapes stay when RACK_HOST_CHECKS is set (rack_host's check 12 compares
   the drum voices' copy to the stock class); the delay-line shapes' code and tables, and the others, go.
"""

HEADERS = {
    "braids/analog_oscillator.h": [("  static RenderFn fn_table_[];", "  static const RenderFn fn_table_[];")],
    "braids/macro_oscillator.h": [("  static RenderFn fn_table_[];", "  static const RenderFn fn_table_[];"),
                                  ("  inline void set_pitch(int16_t pitch) {\n    pitch_ = pitch;\n  }\n",
                                   "  inline void set_pitch(int16_t pitch) {\n    pitch_ = pitch;\n  }\n#ifdef RACK_BRAIDS_TRIM\n  inline void set_comb_buffer(int16_t* p) { digital_oscillator_.set_comb_buffer(p); }   // 2.43: SAW COMB's delay line\n#endif\n")],
    "braids/digital_oscillator.h": [("  static RenderFn fn_table_[];", "  static const RenderFn fn_table_[];"),
                                    ('  union {\n    int16_t comb[kCombDelayLength];\n    int16_t ks[1025 * 4];\n    struct {\n      int8_t bridge[kWGBridgeLength];\n      int8_t neck[kWGNeckLength];\n    } bowed;\n    int16_t bore[kWGBoreLength];\n    struct {\n      int8_t jet[kWGJetLength];\n      int8_t bore[kWGFBoreLength];\n    } fluted;\n  } delay_lines_;\n', "#ifdef RACK_BRAIDS_TRIM\n  int16_t* comb_;   // (2.43: the comb's delay line, kCombDelayLength samples: only SAW COMB reads it now (the other delay-line shapes are withdrawn), so it is the host's, and a patch that cannot reach SAW COMB has none; NULL: the comb is skipped, the saw passes through)\n#else\n  union {\n    int16_t comb[kCombDelayLength];\n    int16_t ks[1025 * 4];\n    struct {\n      int8_t bridge[kWGBridgeLength];\n      int8_t neck[kWGNeckLength];\n    } bowed;\n    int16_t bore[kWGBoreLength];\n    struct {\n      int8_t jet[kWGJetLength];\n      int8_t bore[kWGFBoreLength];\n    } fluted;\n  } delay_lines_;\n#endif\n"),
                                    ("  inline void set_shape(DigitalOscillatorShape shape) {\n    shape_ = shape;\n  }\n",
                                     "  inline void set_shape(DigitalOscillatorShape shape) {\n    shape_ = shape;\n  }\n#ifdef RACK_BRAIDS_TRIM\n  inline void set_comb_buffer(int16_t* p) { comb_ = p; }   // 2.43\n#endif\n"),
                                    ("  // void RenderYourAlgo(const uint8_t*, int16_t*, size_t);",
                                     "  void RenderNone(const uint8_t*, int16_t*, size_t);   // (2.42: where a withdrawn shape's function was)")],
    "braids/resources.h": [("extern const int16_t* waveform_table[];", "extern const int16_t* const waveform_table[];")],
    "peaks/modulations/lfo.h": [("  static ComputeSampleFn compute_sample_fn_table_[];", "  static const ComputeSampleFn compute_sample_fn_table_[];")],
    "peaks/resources.h": [("extern const uint16_t* lookup_table_table[];", "extern const uint16_t* const lookup_table_table[];")],
    "rings/dsp/part.h": [("  static float model_gains_[RESONATOR_MODEL_LAST];", "  static const float model_gains_[RESONATOR_MODEL_LAST];"),
                         ("  Resonator resonator_[kMaxPolyphony];\n",
                          "  // (2.42: no modal resonators: the firmware plays MODAL, SYMPATH and SYM QNT as STRING, engine/rack.cc rings_fm1_model)\n")],
}
RINGS_STRING = [("""  if (enable_dispersion_) {
    ProcessInternal<true>(in, out, aux, size);
  } else {
    ProcessInternal<false>(in, out, aux, size);
  }
""", """  ProcessInternal<true>(in, out, aux, size);   // 2.42: STRING and STR+VERB, the models left, have dispersion; the undispersed one is not compiled
""")]
SOURCES = {
    # (rings/dsp/part.cc is changed in make_units.py, which already carries its chord table change; its edits are RINGS_PART)
    "braids/analog_oscillator.cc": [("\nAnalogOscillator::RenderFn AnalogOscillator::fn_table_[] = {", "\nconst AnalogOscillator::RenderFn AnalogOscillator::fn_table_[] = {")],
    "braids/macro_oscillator.cc": [("\nMacroOscillator::RenderFn MacroOscillator::fn_table_[] = {", "\nconst MacroOscillator::RenderFn MacroOscillator::fn_table_[] = {")],
    "braids/resources.cc": [("\nconst int16_t* waveform_table[] = {", "\nconst int16_t* const waveform_table[] = {")],
    "peaks/modulations/lfo.cc": [("\nLfo::ComputeSampleFn Lfo::compute_sample_fn_table_[] = {", "\nconst Lfo::ComputeSampleFn Lfo::compute_sample_fn_table_[] = {")],
    "rings/dsp/string.cc": RINGS_STRING,
    "peaks/resources.cc": [("\nconst uint16_t* lookup_table_table[] = {", "\nconst uint16_t* const lookup_table_table[] = {"),
                           # only the envelope's three curves are read through this table (multistage_envelope.cc: LUT_ENV_LINEAR + shape; the firmware
                           # uses set_adsr, which uses linear, exponential and quartic); the other entries point at one of them, so the delay-time, gravity
                           # and three filter tables, which nothing else reads, are not linked (2.5 KB)
                           ("  lut_delay_times,\n  lut_gravity,\n  lut_env_linear,\n  lut_env_expo,\n  lut_env_quartic,\n  lut_raised_cosine,\n  lut_svf_cutoff,\n  lut_svf_damp,\n  lut_svf_scale,\n};",
                            "  lut_env_linear,\n  lut_env_linear,\n  lut_env_linear,\n  lut_env_expo,\n  lut_env_quartic,\n  lut_env_linear,\n  lut_env_linear,\n  lut_env_linear,\n  lut_env_linear,\n};")],
}
RINGS_PART = [("\nfloat Part::model_gains_[] = {", "\nconst float Part::model_gains_[] = {"),
              # MODAL is never selected (rings_fm1_model, list_norm): its resonators and their code are not in the firmware; if it ever were, it is silent
              ("""  if (model_ == RESONATOR_MODEL_SYMPATHETIC_STRING ||
      model_ == RESONATOR_MODEL_SYMPATHETIC_STRING_QUANTIZED) {
    num_strings""", """  if (false) {   // 2.42: the sympathetic models (1 and 4) play as STRING (rings_fm1_model): one string, no chord table
    num_strings"""),
              ("""    case RESONATOR_MODEL_MODAL:
      {
        int32_t resolution = 64 / polyphony_ - 4;
        for (int32_t i = 0; i < polyphony_; ++i) {
          resonator_[i].Init();
          resonator_[i].set_resolution(resolution);
        }
      }
      break;
""", """    case RESONATOR_MODEL_MODAL:
      break;
"""),
              ("""  Resonator& r = resonator_[voice];
  r.set_frequency(frequency);
  r.set_structure(patch.structure);
  r.set_brightness(patch.brightness * patch.brightness);
  r.set_position(patch.position);
  r.set_damping(patch.damping);
  r.Process(resonator_input_, out_buffer_, aux_buffer_, size);
""", """  fill(&out_buffer_[0], &out_buffer_[size], 0.0f);
  fill(&aux_buffer_[0], &aux_buffer_[size], 0.0f);
""")]
# the withdrawn shapes' functions, in braids/digital_oscillator.cc
GONE = ("RenderFeedbackFm", "RenderPlucked", "RenderBowed", "RenderBlown", "RenderFluted", "RenderQuestionMark")   # (QuestionMark: digital shape 34 = macro 47: past the list, which list_norm clamps to 46)
GONE_DRUM = ("RenderStruckBell", "RenderStruckDrum", "RenderKick", "RenderCymbal", "RenderSnare", "RenderFilteredNoise",
             "RenderTwinPeaksNoise", "RenderClockedNoise", "RenderParticleNoise")
NOTE = ("// Generated by make_units.py (make_mutable_slim.py, FM1 Phase 2 2.42) from pichenettes/eurorack's %s (MIT): %s. The MIT notice below is the original's.\n")


def _sub(text, old, new, what):
    assert text.count(old) == 1, "make_mutable_slim: %s changed: %s" % (what, old[:60])
    return text.replace(old, new)


def apply_list(text, edits, what):
    for old, new in edits:
        text = _sub(text, old, new, what)
    return text


def _digital(c):
    import re
    lines = c.split("\n")
    out, i, n = [], 0, len(lines)
    fn_re = re.compile(r"^void\s+DigitalOscillator::(\w+)\(")
    seen = set()
    while i < n:
        m = fn_re.match(lines[i])
        if not m or m.group(1) not in GONE + GONE_DRUM:
            out.append(lines[i]); i += 1; continue
        j = i
        while lines[j] != "}":
            j += 1
        cond = "#ifndef RACK_BRAIDS_TRIM" if m.group(1) in GONE else "#if !defined(RACK_BRAIDS_TRIM) || defined(RACK_HOST_CHECKS)"
        out.append(cond); out.extend(lines[i:j + 1]); out.append("#endif"); seen.add(m.group(1))
        i = j + 1
    assert seen == set(GONE) | set(GONE_DRUM), "make_mutable_slim: functions missing: %s" % ((set(GONE) | set(GONE_DRUM)) - seen)
    c = "\n".join(out)
    c = _sub(c, "  int16_t* dl = delay_lines_.comb;\n", "#ifdef RACK_BRAIDS_TRIM\n  int16_t* dl = comb_;\n  if (!dl) return;   // 2.43: no delay line (a patch that cannot reach SAW COMB has none: engine/rack.cc fm1_fit): the saw passes through\n#else\n  int16_t* dl = delay_lines_.comb;\n#endif\n", "RenderComb's delay line")
    for f in GONE:
        e = "  &DigitalOscillator::%s,\n" % f
        if c.count(e) == 0:   # (the table's last entry has no comma)
            c = _sub(c, "  &DigitalOscillator::%s\n};" % f, "  RK_GONE(%s)\n};" % f, "digital table")
        else:
            c = _sub(c, e, "  RK_GONE(%s),\n" % f, "digital table")
    for f in GONE_DRUM:
        c = _sub(c, "  &DigitalOscillator::%s,\n" % f, "  RK_GONE_DRUM(%s),\n" % f, "digital table")
    c = _sub(c, "\n/* static */\nDigitalOscillator::RenderFn DigitalOscillator::fn_table_[] = {", """
void DigitalOscillator::RenderNone(const uint8_t*, int16_t* buffer, size_t size) {
  while (size--) *buffer++ = 0;
}

// RACK_BRAIDS_TRIM: the shapes the firmware withdrew never reach this class (engine/rack.cc list_norm); their entries write silence
#ifdef RACK_BRAIDS_TRIM
#define RK_GONE(f) &DigitalOscillator::RenderNone
#ifdef RACK_HOST_CHECKS
#define RK_GONE_DRUM(f) &DigitalOscillator::f
#else
#define RK_GONE_DRUM(f) &DigitalOscillator::RenderNone
#endif
#else
#define RK_GONE(f) &DigitalOscillator::f
#define RK_GONE_DRUM(f) &DigitalOscillator::f
#endif

/* static */
const DigitalOscillator::RenderFn DigitalOscillator::fn_table_[] = {""", "digital table")
    return c


def files(eur):
    """{path under units/: text}: the headers (braids/..., clouds/..., ...) and the sources as mi_*.cc names."""
    out = {}
    for rel, edits in HEADERS.items():
        out[rel] = (NOTE % (rel, "const tables" if "digital" not in rel else "const table, RenderNone")) + apply_list((eur / rel).read_text(), edits, rel)
    for rel, edits in SOURCES.items():
        out["mi_" + rel.replace("/", "_")] = (NOTE % (rel, "const tables")) + apply_list((eur / rel).read_text(), edits, rel)
    c = (eur / "braids/digital_oscillator.cc").read_text()
    c = _digital(c)
    out["mi_rings_dsp_resonator.cc"] = "// Generated by make_units.py (make_mutable_slim.py): nothing. rings/dsp/resonator.cc is not in the firmware: its MODAL model is never selected (engine/rack.cc rings_fm1_model)\n"
    out["mi_braids_digital_oscillator.cc"] = (NOTE % ("braids/digital_oscillator.cc", "const table, the withdrawn shapes' functions left out with RACK_BRAIDS_TRIM")) + c
    return out
