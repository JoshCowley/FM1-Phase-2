#!/bin/sh
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# Installs FM1 Phase 2 2.27 (granular WINDOW, SPEED, JITTER; 20 Nov 2026) on an FM-1 connected to this Mac by USB.
#   sh /Users/josh/Documents/iClaude/Projects/FM1-Rack/install_fm1ph2_2.27.sh
# Put this script and fm1ph2-2.27.fwsc together in Projects/FM1-Rack first.
# Plug the FM-1 straight into the Mac (no hub), and quit anything else using MIDI (DAWs, browser MIDI tabs).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
PKG="$HERE/fm1ph2-2.27.fwsc"
SUM=c73f0ee6fd607096184151822dd6c1ac78cde16697ecc9aa2fa6d2fcb43ecdf7
FM1BASE="$HOME/fm1-work/fm1-base"
REV=c8e05997ce970875038156d2bbd69e712eac1543   # the fm1-base this package was built with

echo "== 1/4 checking the package"
[ -f "$PKG" ] || { echo "Can't find $PKG"; exit 1; }
[ "$(shasum -a 256 "$PKG" | cut -d' ' -f1)" = "$SUM" ] || { echo "fm1ph2-2.27.fwsc isn't the file that was built: stop, tell Claude"; exit 1; }
echo "   ok"

echo "== 2/4 the installer's MIDI library"
python3 -c "import mido, rtmidi" 2>/dev/null || python3 -m pip install --user mido python-rtmidi
echo "   ok"

echo "== 3/4 fm1-base's installer"
if [ ! -d "$FM1BASE/.git" ]; then
  mkdir -p "$(dirname "$FM1BASE")"
  git clone -q https://github.com/charlesvestal/fm1-base "$FM1BASE"
fi
git -C "$FM1BASE" checkout -q $REV 2>/dev/null || { git -C "$FM1BASE" fetch -q origin && git -C "$FM1BASE" checkout -q $REV; }
echo "   ok"

echo "== 4/4 looking for the FM-1"
cd "$FM1BASE"
python3 tools/fm1_install.py --info || {
  echo "The FM-1 wasn't found. Check: it's on, plugged straight into the Mac, nothing else has its MIDI port open. Then run this again."
  exit 1; }
echo
echo "Installing FM1 Phase 2 2.27. Don't unplug the FM-1 until this says it's done."
python3 tools/fm1_install.py "$PKG"
echo
echo "Done. Your saved user presets are kept."
echo "To try (nothing here has been run on an FM-1, seen or heard by a person yet):"
echo "  1. Presets 33-48 are the new classics (WARM PAD to NOISE SWEEP); 1-32 are as before. Play each; hold a chord hands-off to hear the movement, then turn K1 (MORPH) slowly end to end, then K2-K4. Mark them in the feedback form."
echo "  2. Visit each page while playing a chord: source, filter, delay, reverb, Clouds, LFO, Tides, Peaks, Func, AMP, Grids, chorus, Rings, the matrix."
echo "  3. On HOME and on two other pages hold OCT- and OCT+ together and note DRAW, LCD and FPS."
echo "  4. Note the CPU % (top strip), any crackle, and whether the buttons feel sluggish."
