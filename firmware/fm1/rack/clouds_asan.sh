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
# update 8: the granular_processor.cc that is built (make_units.py's changed copy, with the grain ceiling and the reverb skip, when there is one)
GP="$EURORACK/clouds/dsp/granular_processor.cc"; GF=
if [ -f units/mi_clouds_dsp_granular_processor.cc ]; then GP=units/mi_clouds_dsp_granular_processor.cc; GF="-DRACK_CLOUDS_MAX_GRAINS=${GRAINS:-40} -DRACK_CLOUDS_SKIP_REVERB"; fi
c++ -O1 -g -fsanitize=address -ffp-contract=off -std=gnu++11 -w -DTEST $GF -I$ENG/shim -I$EURORACK clouds_asan.cc \
  $EURORACK/clouds/dsp/correlator.cc $GP $EURORACK/clouds/dsp/mu_law.cc \
  $EURORACK/clouds/dsp/pvoc/*.cc $EURORACK/clouds/resources.cc $EURORACK/stmlib/dsp/units.cc $EURORACK/stmlib/utils/random.cc \
  $EURORACK/stmlib/dsp/atan.cc -o build/asan/clouds_asan
ASAN_OPTIONS=detect_leaks=0 build/asan/clouds_asan "$@"
