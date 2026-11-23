#!/bin/bash
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# run_ir.sh, two presets at a time. The same table (name, then host instructions a millisecond: core 1 and core 2 of two cores four notes, still and moving, then the one-core total still / moving).
#   IR_NOTES=6 IR_Q=0 sh run_ir_par.sh > ir-LABEL.tsv      IR_NOTES: voices and notes held (default 4); IR_Q: the quality mask (0 = the 2.37 sound; unset = the engine's own)
cd "$(dirname "$0")"
export BIN="${OUT:-build/host}/bench_ir"
[ -n "$IR_Q" ] || unset IR_Q
one() {
  n="$1"; t=$(mktemp); r=""
  for a in "2C still:1" "2C still:2" "2C moving:1" "2C moving:2" "still NEW:0" "moving NEW:0"; do
    valgrind --tool=callgrind --instr-atstart=no --collect-atstart=no --callgrind-out-file=$t.cg $BIN --ir "$n ${a%%:*}" ${a##*:} >/dev/null 2>&1
    v=$(callgrind_annotate $t.cg 2>/dev/null | sed -n 's/^\([0-9,]*\) (100.0%)  PROGRAM TOTALS.*/\1/p' | tr -d , | awk '{printf "%d", $1/200}'); r="$r\t$v"
  done
  rm -f $t $t.cg; printf "%s$r\n" "$n"
}
python3 -c "
import re;print('\n'.join(re.findall(r'\"([^\"]+)\"', re.search(r'MSNAMES\[\] = \{(.*?)\};', open('modsum_all.h').read()).group(1))))" | xargs -d '\n' -P 2 -I{} bash -c "$(declare -f one); one '{}'" | sort
