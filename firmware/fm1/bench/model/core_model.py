# SPDX-License-Identifier: GPL-3.0
# FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
# The cores' cost model, calibrated on the FM-1 (bench q45, 25 Nov 2026): what a 2 CORES preset case costs each core, in % of one core, from the host's instruction counts of its parts
# (host_components.py) and a few per-module device costs, fitted by non-negative least squares on the device's C1 and C2 of every preset under 2.42's arrangement and 2.45's.
#   python3 core_model.py [--csv q45.csv] [--out predictions.csv]
# A core's cost = host voice kinstr x A_voice (per class) + voices x A_pervoice + a job's fixed cost (core 2, once a block it has voices) + the reverb, Rings, effects chain and the rest (host kinstr
# x their scales) + per-module constants (Tides, Peaks, Func, Grids, chorus, delay). 2.45's learned split is unknown: for its rows the voices on each core are a nuisance parameter, chosen per row.
import csv, json, os, re, sys
import numpy as np
from scipy.optimize import nnls
H = os.path.dirname(os.path.abspath(__file__))
csvp = sys.argv[sys.argv.index('--csv') + 1] if '--csv' in sys.argv else os.path.join(H, 'q45.csv')
outp = sys.argv[sys.argv.index('--out') + 1] if '--out' in sys.argv else None
rows = list(csv.reader(open(csvp)))
dev = {r[1]: float(r[2]) for r in rows if r[0] == '2 CORES'}
comp = json.load(open(os.path.join(H, 'comp242.json')))
arr = {}
for l in open(os.path.join(H, 'arr245.tsv')):
    p = l.rstrip('\n').split('\t'); arr[p[0]] = tuple(int(x) for x in p[1:])
calls = json.load(open(os.path.join(H, '..', '..', '..', 'tests', 'presets-fm1-calls.json')))
MODS = {}
for p in calls:
    m = {}
    for c in p['calls']:
        if c[0] == 'set_module': m[c[1]] = c[2]
    MODS[p['name']] = [m.get(i, 0) for i in range(6)]
names = sorted(n[:-5] for n in dev if n.endswith(' 2.42') and re.search(r' x\d 2.42$', n) and comp.get(n[:-5] + ' 2.42') and 'TIDE PAD' not in n)
LAB = ['voice', 'voice (Rings preset)', 'per voice', 'job (core 2)', 'reverb', 'Rings', 'effects chain', 'rest', 'reverb job on core 2', 'Tides', 'Peaks', 'Func', 'Grids', 'chorus', 'delay']
def vec(n, k, rvpost, c2_has_rv=False):
    c = comp[n + ' 2.42']; N = arr[n + ' 2.42'][0]
    per = c['voice'] / N; rev = c.get('rev', 0); rings = c.get('rings', 0); fx = c.get('fx', 0)
    rest = c['total'] - c['voice'] - rev - rings - fx
    ring = 1.0 if rings > 0 else 0.0
    mm = MODS[re.sub(r' x\d$', '', n)]
    ind = [sum(1 for q in mm[2:4] if q == i) for i in (21, 20, 24, 23)] + [1.0 if mm[4] == 33 else 0, 1.0 if mm[4] == 31 else 0]
    c1 = np.array([per * k * (1 - ring), per * k * ring, k, 0, 0 if rvpost else rev, rings, fx, rest, 0.0] + ind)
    c2 = np.array([per * (N - k) * (1 - ring), per * (N - k) * ring, N - k, 1.0 if N > k else 0, 0, 0, 0, 0, rev if (rvpost and c2_has_rv) else 0.0] + [0] * 6)
    return c1, c2
sel = []
for n in names:
    sel.append([n, '2.42', arr[n + ' 2.42'][1], False, dev[n + ' 2.42 C1'], dev[n + ' 2.42 C2']])
for n in names:
    sel.append([n, '2.45', arr[n][1], True, dev[n + ' C1'], dev[n + ' C2']])   # (the bench's C2 of q45 left the reverb job out)
def build():
    A = []; b = []; lab = []
    for n, a, k, rp, d1, d2 in sel:
        c1, c2 = vec(n, k, rp); A += [c1, c2]; b += [d1, d2]; lab += [(n, a, 'C1'), (n, a, 'C2')]
    return np.array(A), np.array(b), lab
for it in range(8):
    A, b, lab = build(); x, _ = nnls(A, b)
    for s in sel:
        if s[1] == '2.45':
            N = arr[s[0] + ' 2.42'][0]
            s[2] = min(range(N + 1), key=lambda kk: (vec(s[0], kk, True)[0] @ x - s[4]) ** 2 + (vec(s[0], kk, True)[1] @ x - s[5]) ** 2)
A, b, lab = build(); x, _ = nnls(A, b); pred = A @ x; err = pred - b
a = np.array([l[1] for l in lab])
def stat(m, t):
    e = err[m]; print('%-34s n=%3d  rms %.2f  mean|e| %.2f  max|e| %.2f points of a core' % (t, m.sum(), np.sqrt((e ** 2).mean()), abs(e).mean(), abs(e).max()))
print('fitted costs (device points per host kinstr/ms, or points):')
for l, v in zip(LAB, x): print('   %-22s %.5f' % (l, v))
stat(a == '2.42', "C1 and C2, 2.42's rows"); stat(a == '2.45', "C1 and C2, 2.45's rows (voices fitted)"); stat(np.ones(len(a), bool), 'C1 and C2, all')
mx = np.array([max(pred[i], pred[i + 1]) - max(b[i], b[i + 1]) for i in range(0, len(b), 2)])
print('max(C1,C2) of a row: rms %.2f  mean|e| %.2f  max|e| %.2f' % (np.sqrt((mx ** 2).mean()), abs(mx).mean(), abs(mx).max()))
# the block's length: the first core's work before the join (its voices, Rings, the reverb when early, the modulators: A1) or the second core's job (voices and the job's fixed cost: A2),
# whichever is longer, then the effects chain and the master (F). C1 = A1 + F, C2 = A2 (+ the reverb when its job is there), block = max(A1, A2) + F.
def parts(n, k, rp):
    c1, c2 = vec(n, k, rp)
    pre = c1 @ (x * np.array([1, 1, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0, 0]))
    post = c1 @ (x * np.array([0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1]))
    return pre, c2 @ x, post
tm = []
for n, a_, k, rp, d1, d2 in sel:
    pre, a2, post = parts(n, k, rp); tm.append(max(pre, a2) + post - dev[n + (' 2.42' if a_ == '2.42' else '')])
tm = np.array(tm); print('block length (CPU %% of a case): rms %.2f  mean|e| %.2f  max|e| %.2f points' % (np.sqrt((tm ** 2).mean()), abs(tm).mean(), abs(tm).max()))
# the engine's arrangement: the learner starts from 2.42's table and makes the moves (a voice to the other core, the reverb's job to the second core) that most lower the busier core's load while the block
# stays within 2 % of its length (engine/rack.cc split_learn). The model takes the arrangement (voices on the second core, the reverb's place) with the lowest busier-core load among those whose block, as the
# model says it changes from 2.42's, is at most 2 % longer than 2.42's measured one; each core's figure is the device's 2.42 row plus the model's change. The reverb's job on core 2 costs its own plus a job's
# fixed cost (the worst case: it shares no job with the voices) and counts against the block only where it is longer than half the chain after the slot.
JOB = x[3]; RV = x[4]
def best(n):
    N = arr[n + ' 2.42'][0]; rev = comp[n + ' 2.42'].get('rev', 0); k42 = arr[n + ' 2.42'][1]
    pre0, a20, post0 = parts(n, k42, False)
    T0 = max(pre0, a20) + post0
    o = None
    for rp in (False, True):
        for k in range(N + 1):
            pre, a2, post = parts(n, k, rp)
            T = max(pre, a2) + post + (max(0.0, RV * rev + JOB - 0.5 * post) if rp else 0.0)
            Tp = dev[n + ' 2.42'] + (T - T0)
            C1 = dev[n + ' 2.42 C1'] + (pre + post) - (pre0 + post0)
            C2 = dev[n + ' 2.42 C2'] + (a2 - a20 if (k42 < N and k < N) else (a2 if k42 >= N else 0.0)) + (RV * rev if rp else 0.0)
            if Tp > dev[n + ' 2.42'] * 1.02 + 1e-9: continue
            key = (max(C1, C2), Tp)
            if o is None or key < o[0]: o = (key, C1, C2, k, rp, Tp)
    return o
tab = []
for n in names:
    N = arr[n + ' 2.42'][0]
    m42 = max(dev[n + ' 2.42 C1'], dev[n + ' 2.42 C2']); m45 = max(dev[n + ' C1'], dev[n + ' C2'])
    o = best(n)
    tab.append((n, m42, m45, max(o[1], o[2]), N - o[3], o[4], o[5], dev[n + ' 2.42'], dev[n]))
# the other 2 CORES rows (Clouds, drums, TIDE PAD with Clouds): the same arrangement as 2.45's except the reverb with Clouds (its job goes into Clouds' job: the first core's share of the reverb moves to the second)
others = []
for k_, v_ in dev.items():
    if re.search(r' C[12]$', k_) or k_.endswith(' 2.42') or k_.endswith(' 2.45'): continue
    if k_ in [n for n in names]: continue
    c1_, c2_ = dev.get(k_ + ' C1'), dev.get(k_ + ' C2')
    if c1_ is None: continue
    d242 = max(dev[k_ + ' 2.42 C1'], dev[k_ + ' 2.42 C2']) if (k_ + ' 2.42 C1') in dev else None
    p1, p2 = c1_, c2_
    if 'CL+REVERB' in k_: r_ = RV * 7900.0; p1, p2 = c1_ - r_, c2_ + r_
    others.append((k_, d242, max(c1_, c2_), max(p1, p2)))
if outp:
    with open(outp, 'w') as f:
        w = csv.writer(f); w.writerow(['case', 'max core, 2.42 (device)', 'max core, 2.45 (device)', 'max core, engine 2.46 (model)', 'voices on core 2', 'reverb job on core 2', 'block, 2.42 (device)', 'block, 2.45 (device)', 'block, engine (model)'])
        for t in tab: w.writerow([t[0], '%.1f' % t[1], '%.1f' % t[2], '%.1f' % t[3], t[4], int(t[5]), '%.1f' % t[7], '%.1f' % t[8], '%.1f' % t[6]])
        for t in others: w.writerow([t[0], '' if t[1] is None else '%.1f' % t[1], '%.1f' % t[2], '%.1f' % t[3], '', 0, '', '', ''])
t42 = np.array([t[1] for t in tab]); t45 = np.array([t[2] for t in tab]); tn = np.array([t[3] for t in tab]); mn = np.minimum(t42, t45)
tb42 = np.array([t[7] for t in tab]); tb45 = np.array([t[8] for t in tab]); tbn = np.array([t[6] for t in tab])
print('block length: mean 2.42 %.1f, 2.45 %.1f, engine %.1f; worst 2.42 %.1f, 2.45 %.1f, engine %.1f' % (tb42.mean(), tb45.mean(), tbn.mean(), tb42.max(), tb45.max(), tbn.max()))
print('predicted max(C1,C2), %d preset cases: mean 2.42 %.1f, 2.45 %.1f, engine %.1f; worst 2.42 %.1f, 2.45 %.1f, engine %.1f' % (len(tab), t42.mean(), t45.mean(), tn.mean(), t42.max(), t45.max(), tn.max()))
print('engine <= min(2.42, 2.45) in %d of %d cases; worst excess %.1f; mean gain on min(2.42,2.45) %.1f' % ((tn <= mn + 1e-9).sum(), len(tab), (tn - mn).max(), (mn - tn).mean()))
