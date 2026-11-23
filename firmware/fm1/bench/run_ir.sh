#!/bin/sh
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# bench_ir over the whole bank: one line a preset: name, then host instructions (thousands) for 20 blocks of 10 ms ... per 1 ms block, for
#   core 1 and core 2 (two cores, four notes) still and moving, and the one-core total still / moving.   sh run_ir.sh > ir-LABEL.tsv
cd "$(dirname "$0")"
ir() { valgrind --tool=callgrind --instr-atstart=no --collect-atstart=no --callgrind-out-file=/tmp/cg.$$ ${OUT:-build/host}/bench_ir --ir "$1" "$2" $FLAG >/dev/null 2>&1; callgrind_annotate /tmp/cg.$$ 2>/dev/null | sed -n 's/^\([0-9,]*\) (100.0%)  PROGRAM TOTALS.*/\1/p' | tr -d , | awk '{printf "%d", $1/200}'; rm -f /tmp/cg.$$; }
python3 -c "
import re;print('\n'.join(re.findall(r'\"([^\"]+)\"', re.search(r'MSNAMES\[\] = \{(.*?)\};', open('modsum_all.h').read()).group(1))))" | while read -r n; do
  printf "%s\t%s\t%s\t%s\t%s\t%s\t%s\n" "$n" "$(ir "$n 2C still" 1)" "$(ir "$n 2C still" 2)" "$(ir "$n 2C moving" 1)" "$(ir "$n 2C moving" 2)" "$(ir "$n still NEW" 0)" "$(ir "$n moving NEW" 0)"
done
