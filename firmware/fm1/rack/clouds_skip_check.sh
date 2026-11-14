#!/bin/sh
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# RACK_CLOUDS_SKIP_REVERB's proof, on Clouds alone (clouds_skip.cc): the output of every mode and quality at REVERB 0, hashed, from Mutable's
# own granular_processor.cc and from the update 8 copy (make_units.py's units/): the two hashes must be equal. Also prints the step across
# a REVERB change from the copy. Needs make_units.py to have run (build_host.sh does):  EURORACK=... sh clouds_skip_check.sh
set -e
cd "$(dirname "$0")"
EURORACK="$(cd "${EURORACK:-../../../../eurorack}" && pwd)"
ENG=../../../engine
mkdir -p build/skip
D="$EURORACK/clouds/dsp"
COMMON="$D/correlator.cc $D/mu_law.cc $D/pvoc/*.cc $EURORACK/clouds/resources.cc $EURORACK/stmlib/dsp/units.cc $EURORACK/stmlib/utils/random.cc $EURORACK/stmlib/dsp/atan.cc"
CXX="c++ -O1 -ffp-contract=off -std=gnu++11 -w -DTEST -I$ENG/shim -I$EURORACK"
$CXX clouds_skip.cc $D/granular_processor.cc $COMMON -o build/skip/orig
$CXX -DRACK_CLOUDS_SKIP_REVERB clouds_skip.cc units/mi_clouds_dsp_granular_processor.cc $COMMON -o build/skip/skip
a=$(build/skip/orig | grep hash); b=$(build/skip/skip | grep hash)
echo "Mutable's own: $a"; echo "skip copy:     $b"
build/skip/orig | grep step | sed 's/^/Mutable own: /'
build/skip/skip | grep step | sed 's/^/skip copy:     /'
[ "$a" = "$b" ] && echo "reverb skip: sample-identical at REVERB 0" || { echo "reverb skip: DIFFERENT"; exit 1; }
