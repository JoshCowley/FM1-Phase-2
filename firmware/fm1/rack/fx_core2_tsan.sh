#!/bin/sh
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# Clouds on the second core (update 9) under ThreadSanitizer: the engine built with -fsanitize=thread, the second core a real thread
# (host_cpu2.h HC2_THREAD), every chain order and Clouds mode, with FREEZE, QUALITY and MODE changes, patch changes and a worker that fails on
# some blocks. Not part of the default checks:  EURORACK=/path/to/eurorack sh fx_core2_tsan.sh [nosupp]
# By default one race is suppressed (tsan.supp): stmlib::Random's one 32-bit state word, which the voices and Clouds' grains both draw from (the
# voices on the two cores already did, since update 3; a lost update at worst). "nosupp" runs with no suppression: that one race shows, nothing else should.
set -e
cd "$(dirname "$0")"
OUT=build/tsan RACK_FLAGS="-fsanitize=thread -g" LDFLAGS="-fsanitize=thread" sh build_host.sh > build/tsan.log 2>&1 || { tail -20 build/tsan.log; exit 1; }
SUPP="suppressions=$PWD/tsan.supp"; [ "$1" = nosupp ] && SUPP=
export TSAN_OPTIONS="$SUPP halt_on_error=0 exitcode=0 second_deadlock_stack=1"
bad=0; n=0
for scen in 0 1 2 3; do
  for chain in 0 1 2 3 4 5 6; do
    for mode in 0 1 2; do
      for fail in "0 0" "25 25"; do
        n=$((n + 1))
        build/tsan/rack_host --fx2 $chain $mode $scen 2 $fail > /dev/null 2> build/tsan.err || true
        if grep -q "WARNING: ThreadSanitizer" build/tsan.err; then
          bad=$((bad + 1)); echo "chain $chain mode $mode scenario $scen failing '$fail':"; grep -A12 "WARNING: ThreadSanitizer" build/tsan.err | head -30
        fi
      done
    done
  done
done
echo "ThreadSanitizer: $n runs (7 chain orders x 3 modes x 4 scenarios x worker working / failing), $bad with a report"
[ $bad -eq 0 ]
