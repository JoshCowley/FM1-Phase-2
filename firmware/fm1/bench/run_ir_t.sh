#!/bin/bash
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# 2.43: run_ir_par.sh with the first core's pre-join work and the reverb job counted apart, so the block's time can be estimated. One line a preset:
#   name, then for the knobs still and moving: core 1, core 2, the first core's own voices (pre-join), the reverb job (all in host instructions a ms, thousands),
#   then the one-core total still / moving.   IR_NOTES=6 IR_RVPOST=0 (2.42's way: the reverb on the first core) bash run_ir_t.sh > ir-LABEL.tsv
# The block's time estimate: T = max(pre, voices job) + max(rest of the first core, reverb job): see /home/claude/b25/ir/est.py
cd "$(dirname "$0")"
export BIN="${OUT:-build/host}/bench_ir"
one() {
  n="$1"; t=$(mktemp); r=""
  for a in "2C still:1" "2C still:2" "2C still:3" "2C still:4" "2C moving:1" "2C moving:2" "2C moving:3" "2C moving:4" "still NEW:0" "moving NEW:0"; do
    valgrind --tool=callgrind --instr-atstart=no --collect-atstart=no --callgrind-out-file=$t.cg $BIN --ir "$n ${a%%:*}" ${a##*:} >/dev/null 2>&1
    v=$(callgrind_annotate $t.cg 2>/dev/null | sed -n 's/^\([0-9,]*\) (100.0%)  PROGRAM TOTALS.*/\1/p' | tr -d , | awk '{printf "%d", $1/200}'); r="$r\t$v"
  done
  rm -f $t $t.cg; printf "%s$r\n" "$n"
}
python3 -c "
import re;print('\n'.join(re.findall(r'\"([^\"]+)\"', re.search(r'MSNAMES\[\] = \{(.*?)\};', open('modsum_all.h').read()).group(1))))" | xargs -d '\n' -P 2 -I{} bash -c "$(declare -f one); one '{}'" | sort
