#!/bin/sh
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# The benchmark's check on a computer (bench_host.c): the same cases, the engine built as the FM-1's
# (-DRACK_FM1 -DRACK_LITE, the arena as fm1.toml sets it). Needs a C and C++ compiler and the Mutable code:
#   EURORACK=/path/to/eurorack sh build_host.sh && build/host/bench_host
set -e
cd "$(dirname "$0")"
EURORACK="${EURORACK:-../../../../eurorack}"
ENG=../../../engine
OUT=build/host
mkdir -p $OUT
# RACK_FLAGS: extra compiler flags, e.g. RACK_FLAGS=-DRACK_WITH_GRAIN to build the shelved GRAIN source in (off by default).
# A change of flags clears the objects (a stale .o from the other setting would be linked otherwise).
RACK_FLAGS="${RACK_FLAGS:-}"
[ "$(cat $OUT/.flags 2>/dev/null)" = "$RACK_FLAGS" ] || { rm -f $OUT/*.o $OUT/*.oa $OUT/*.oc; echo "$RACK_FLAGS" > $OUT/.flags; }
ARENA=$(sed -n 's/.*-DRACK_ARENA_KB=\([0-9]*\).*/\1/p' fm1.toml)
CXX="${CXX:-c++} -O2 -ffp-contract=off -fno-exceptions -fno-rtti -std=gnu++11 -w -DTEST -DRACK_FM1 -DRACK_LITE -DRACK_ARENA_KB=$ARENA -DRACK_RINGS16 -DRACK_NO_PLAITS -DRACK_CLOUDS -DRACK_CLOUDS_NO_STRETCH $RACK_FLAGS -I$ENG/shim -I$EURORACK"
rm -f $OUT/plaits_*.o   # FM1 Phase 2: no Plaits
for src in $(grep -vE '^(elements|warps|marbles|plaits)/' $ENG/sources.txt) ; do
  o="$OUT/$(echo "$src" | sed 's#/#_#g; s#\.cc$##').o"
  [ -f "$o" ] || { echo "c++ $src"; $CXX -c "$EURORACK/$src" -o "$o"; }
done
$CXX -c $ENG/grids_nodes.cc -o $OUT/x_grids_nodes.o
$CXX -c $ENG/rack.cc -o $OUT/rack.o
$CXX -c rk.cc -o $OUT/rk.o
${CC:-cc} $RACK_FLAGS -O2 -ffp-contract=off -std=gnu99 -Wall -Wno-unused-function -c bench_host.c -o $OUT/bench_host.o
${CXX%% *} -o $OUT/bench_host $OUT/*.o -lm
echo "built $OUT/bench_host"
