#!/bin/sh
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# Clouds in the trimmed buffers under AddressSanitizer (clouds_asan.cc). Not part of the default checks (needs a compiler with
# -fsanitize=address):  EURORACK=/path/to/eurorack sh clouds_asan.sh [small_bytes]
set -e
cd "$(dirname "$0")"
EURORACK="$(cd "${EURORACK:-../../../../eurorack}" && pwd)"
ENG=../../../engine
mkdir -p build/asan
c++ -O1 -g -fsanitize=address -ffp-contract=off -std=gnu++11 -w -DTEST -I$ENG/shim -I$EURORACK clouds_asan.cc \
  $EURORACK/clouds/dsp/correlator.cc $EURORACK/clouds/dsp/granular_processor.cc $EURORACK/clouds/dsp/mu_law.cc \
  $EURORACK/clouds/dsp/pvoc/*.cc $EURORACK/clouds/resources.cc $EURORACK/stmlib/dsp/units.cc $EURORACK/stmlib/utils/random.cc \
  $EURORACK/stmlib/dsp/atan.cc -o build/asan/clouds_asan
ASAN_OPTIONS=detect_leaks=0 build/asan/clouds_asan "$@"
