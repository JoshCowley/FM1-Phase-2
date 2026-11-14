#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
"""Lists (and with --add writes) the licence header of the project's own source files.

   python3 tools/check_licence_headers.py          exit 1 if a tracked source file has no SPDX line in its first 4 lines
   python3 tools/check_licence_headers.py --add    prepends the GPL-3.0 header to the files that lack one

Generated files carry the line from their generator. Mutable Instruments code is not in this repository (a checkout of
pichenettes/eurorack beside it), so every tracked file is the project's own or a marked derivative (NOTICE.md)."""
import subprocess, sys, os
root = subprocess.check_output(["git", "rev-parse", "--show-toplevel"], text=True, cwd=os.path.dirname(os.path.abspath(__file__))).strip()
files = subprocess.check_output(["git", "ls-files"], text=True, cwd=root).split("\n")
SLASH = (".c", ".cc", ".h", ".js", ".mjs")
HASH = (".py", ".sh")
ID = "SPDX-License-Identifier: GPL-3.0"
TEXT = "FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md."
missing = []
for f in files:
    if not f.endswith(SLASH + HASH): continue
    p = os.path.join(root, f)
    s = open(p, encoding="utf-8").read()
    if "SPDX-License-Identifier" in "\n".join(s.split("\n")[:4]): continue
    missing.append(f)
    if "--add" in sys.argv:
        c = "//" if f.endswith(SLASH) else "#"
        h = "%s %s\n%s %s\n" % (c, ID, c, TEXT)
        if s.startswith("#!"):
            first, rest = s.split("\n", 1)
            s = first + "\n" + h + rest
        else:
            s = h + s
        open(p, "w", encoding="utf-8").write(s)
if "--add" not in sys.argv:
    for f in missing: print("no SPDX line:", f)
    sys.exit(1 if missing else 0)
print("added to", len(missing), "files")
