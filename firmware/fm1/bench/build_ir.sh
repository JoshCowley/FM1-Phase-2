#!/bin/sh
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# bench_ir (host only): every factory preset whole (knob layer included), four notes, on two cores, the knobs still or moving; the instructions of each core under
# callgrind. Run build_host.sh first (the objects); then   sh build_ir.sh && sh run_ir.sh [LABEL]
set -e
cd "$(dirname "$0")"
python3 make_modsum_cases.py --all >/dev/null
OUT="${OUT:-build/host}"
ARENA=$(sed -n 's/.*-DRACK_ARENA_KB=\([0-9]*\).*/\1/p' fm1.toml)
CLF="-DRACK_CLOUDS_32K -DRACK_CLOUDS_MAX_GRAINS=24 -DRACK_CLOUDS_SKIP_REVERB -DRACK_FX_CORE2 -DRACK_DRUMS -DRACK_HOST_CHECKS"
${CC:-cc} $CLF -DBENCH_IR -DBENCH_MSALL -O2 -ffp-contract=off -std=gnu99 -w -c bench_host.c -o $OUT/bench_ir.oi
ls $OUT/*.o | grep -v "/bench_host.o" | xargs ${CXX:-c++} -o $OUT/bench_ir $OUT/bench_ir.oi -lm
echo "built $OUT/bench_ir"
