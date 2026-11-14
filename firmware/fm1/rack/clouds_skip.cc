// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// Clouds alone, 32-frame blocks (the size the 32 kHz path gives it), for clouds_skip_check.sh: prints a hash of every output sample of
// runs with REVERB 0 (every mode, quality, with FREEZE and FEEDBACK on and off): the same hash with Mutable's own granular_processor.cc as
// with the update 8 copy (RACK_CLOUDS_SKIP_REVERB) = the reverb skip changes no sample. Then REVERB 0 -> 0.5 -> 0 (GRANULAR): the largest
// step between neighbouring samples in the 20 ms after each change (the copy only: the skip comes back from empty memory, ramped).
#include <new>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <stdint.h>
#include "clouds/dsp/granular_processor.h"
#include "stmlib/utils/random.h"
using namespace clouds;
static float rnd() { return (float)rand() / (float)RAND_MAX; }
static uint32_t fnv(uint32_t h, int16_t v) { return (h ^ (uint16_t)v) * 16777619u; }
static void run(int mode, int q, int seed, uint32_t *hash, double *stepmax, int sched)
{
    size_t small = 49152, large = 2 * small;
    uint8_t *lg = (uint8_t *)malloc(large), *sm = (uint8_t *)malloc(small);
    memset(lg, 0, large); memset(sm, 0, small);
    void *mem = malloc(sizeof(GranularProcessor));
    memset(mem, 0, sizeof(GranularProcessor));              // as the engine's arena is: the modules expect zeroed memory
    GranularProcessor *gp = new (mem) GranularProcessor();
    stmlib::Random::Seed(seed * 31u + 7u);
    srand(seed * 7 + mode);
    gp->Init(lg, large, sm, small);
    gp->set_quality(mode == 2 ? (q | 1) : q);
    gp->set_playback_mode(mode == 0 ? PLAYBACK_MODE_GRANULAR : mode == 1 ? PLAYBACK_MODE_LOOPING_DELAY : PLAYBACK_MODE_SPECTRAL);
    Parameters *p = gp->mutable_parameters();
    float ph = 0;
    ShortFrame in[32], out[32];
    int16_t last = 0;
    uint32_t h = 2166136261u;
    double sm_ = 0;
    int blocks = sched ? 9 * 1000 : 4 * 1000, win = 0;
    for (int b = 0; b < blocks; ++b) {
        if (sched) {
            p->reverb = (b / 1500) % 2 ? 0.5f : 0.0f;
            if (b % 1500 == 0 && b) win = 20;                 // 20 blocks of 1 ms (about 20 ms) after each change
            p->position = 0.3f; p->size = 0.5f; p->density = 0.8f; p->texture = 0.4f; p->dry_wet = 0.6f; p->feedback = 0.1f; p->freeze = false;
        } else if (b % 400 == 0) {
            p->position = rnd(); p->size = rnd(); p->pitch = (rnd() * 2 - 1) * 24; p->density = rnd(); p->texture = rnd();
            p->dry_wet = 0.5f + 0.5f * rnd(); p->stereo_spread = rnd(); p->feedback = seed == 1 ? rnd() * 0.8f : 0.0f;           // (FREEZE with FEEDBACK > 0 puts a reverb of its own in, so not together)
            p->reverb = 0.0f; p->freeze = seed == 2 && (b / 400) % 5 == 3;
        }
        p->trigger = p->gate = (b % 150) == 0;
        for (int i = 0; i < 32; ++i) {
            ph += 220.0f / 32000.0f * 6.2831853f;
            in[i].l = (short)(sinf(ph) * 9000 + sinf(ph * 2.7f) * 4000);
            in[i].r = (short)(sinf(ph * 1.01f) * 9000);
        }
        gp->Prepare();
        gp->Process(in, out, 32);
        for (int i = 0; i < 32; ++i) {
            h = fnv(fnv(h, out[i].l), out[i].r);
            if (win > 0) { double d = fabs((double)out[i].l - last); if (d > sm_) sm_ = d; }
            last = out[i].l;
        }
        if (win > 0) win--;
    }
    *hash = h; *stepmax = sm_ / 32768.0;
    gp->~GranularProcessor(); free(mem); free(lg); free(sm);
}
int main()
{
    uint32_t all = 2166136261u, h;
    double s;
    for (int mode = 0; mode < 3; ++mode)
        for (int q = 0; q < 4; ++q)
            for (int seed = 1; seed <= 2; ++seed) { run(mode, q, seed, &h, &s, 0); all = (all ^ h) * 16777619u; }
    printf("hash REVERB 0: %08x\n", all);
    run(0, 0, 1, &h, &s, 1);
    printf("step after a REVERB change: %.5f\n", s);
    return 0;
}
