# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# Host instructions of each part of a 2 CORES case (valgrind callgrind, inclusive, thousands of instructions a millisecond): the voices, the reverb's job, Rings, the effects chain.
# BIN: bench_ir built with -DBENCH_IR -DNCASE_MAX=700 (see bench/README.md, 2.46).   python3 host_components.py names.txt comp242.json   (names: one case a line, the 2.42 twins "NAME xN 2.42")
import subprocess,sys,re,os,tempfile,json,csv
from concurrent.futures import ThreadPoolExecutor
BIN=os.environ.get('BIN','/tmp/ir2/bench_ir')
def comp(case, mode=0):
    t=tempfile.mktemp(dir='/tmp')
    subprocess.run(['valgrind','--tool=callgrind','--instr-atstart=no','--collect-atstart=no','--callgrind-out-file='+t,BIN,'--ir',case,str(mode)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
    o=subprocess.run(['callgrind_annotate','--inclusive=yes',t],capture_output=True,text=True).stdout
    os.unlink(t)
    d={}
    for l in o.splitlines():
        m=re.match(r'\s*([\d,]+) \(\s*[\d.]+%\)\s+(?:\S*?:)?(.*?)(?: \[|$)',l)
        if not m: continue
        v=int(m.group(1).replace(',',''))
        f=m.group(2)
        key=None
        if f.startswith('PROGRAM'): key='total'
        elif f.startswith('render_voice'): key='voice'
        elif f.startswith('reverb_wet'): key='rev'
        elif f.startswith('run_rings_shared'): key='rings'
        elif f.startswith('run_master'): key='master'
        elif f.startswith('run_fx'): key='fx'
        elif f.startswith('render(') or f=='render': key='render'
        elif f.startswith('run_mod(') or f.startswith('run_lfos'): key=f.split('(')[0]
        if key=='fx' or key=='rev': d[key]=d.get(key,0)+v/200.0
        elif key and key not in d: d[key]=v/200.0  # kinstr/ms (20 blocks of 10ms=200ms? as ir.sh: /200)
    return d
if __name__=='__main__':
    names=[l.strip() for l in open(sys.argv[1])]
    with ThreadPoolExecutor(8) as ex:
        res=list(ex.map(comp,names))
    json.dump(dict(zip(names,res)),open(sys.argv[2],'w'),indent=0)
