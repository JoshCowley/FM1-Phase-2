// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
// Clouds alone, in the trimmed buffers (engine/rack.cc RACK_CLOUDS_SMALL), each as its own malloc of exactly its size, so
// AddressSanitizer reports any read or write past it. Every mode the Lite firmware offers (GRANULAR, LOOPING DELAY, SPECTRAL;
// STRETCH is left out) and every QUALITY, 32-frame blocks (update 8: RACK_CLOUDS_32K gives Clouds exactly its own block, 48 frames at 48 kHz = 32 at 32 kHz; before: 24 frames, two a block), random parameters, a
// freeze now and then. Run by clouds_asan.sh. Exit 0 = ran with no ASan report.
//   also: CLOUDS_SMALL=n (bytes) tries another size, to show where it stops being safe
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "clouds/dsp/granular_processor.h"
using namespace clouds;
#ifndef SMALL
#define SMALL 49152
#endif
static float rnd() { return (float)rand() / (float)RAND_MAX; }
int main(int argc, char **argv)
{
    size_t small = argc > 1 ? (size_t)atoi(argv[1]) : SMALL, large = 2 * small;
    long frames = 0;
    for (int mode = 0; mode < 3; ++mode) {            // GRANULAR, LOOPING DELAY, SPECTRAL
        int pm = mode == 0 ? PLAYBACK_MODE_GRANULAR : mode == 1 ? PLAYBACK_MODE_LOOPING_DELAY : PLAYBACK_MODE_SPECTRAL;
        for (int q = 0; q < 4; ++q) {
            int qq = pm == PLAYBACK_MODE_SPECTRAL ? (q | 1) : q;     // the engine plays SPECTRAL as its mono QUALITY
            for (int seed = 1; seed <= 3; ++seed) {
                srand(seed * 7 + mode);
                uint8_t *lg = (uint8_t *)malloc(large), *sm = (uint8_t *)malloc(small);
                memset(lg, 0, large); memset(sm, 0, small);
                GranularProcessor *gp = new GranularProcessor();
                gp->Init(lg, large, sm, small);
                gp->set_quality(qq);
                gp->set_playback_mode((PlaybackMode)pm);
                Parameters *p = gp->mutable_parameters();
                float ph = 0;
                static_assert(kMaxBlockSize == 32, "the engine passes Clouds exactly 32 frames");
                const size_t N = 32;
                ShortFrame *in = (ShortFrame *)malloc(N * sizeof(ShortFrame)), *out = (ShortFrame *)malloc(N * sizeof(ShortFrame));   // exactly 32: any read or write past them is reported
                for (int b = 0; b < 3000; ++b) {      // 3000 x 32 frames at 32 kHz = 3 s
                    if (b % 400 == 0) {
                        p->position = rnd(); p->size = rnd(); p->pitch = (rnd() * 2 - 1) * 24; p->density = rnd();
                        p->texture = rnd(); p->dry_wet = 0.5f + 0.5f * rnd(); p->stereo_spread = rnd(); p->feedback = rnd() * 0.8f;
                        p->reverb = rnd() < 0.3f ? 0.0f : rnd(); p->freeze = rnd() < 0.25f;
                    }
                    p->trigger = p->gate = (b % 150) == 0;
                    for (size_t i = 0; i < N; ++i) {
                        ph += 220.0f / 32000.0f * 6.2831853f;
                        in[i].l = (short)(sinf(ph) * 9000 + sinf(ph * 2.7f) * 4000);
                        in[i].r = (short)(sinf(ph * 1.01f) * 9000);
                    }
                    gp->Prepare();
                    gp->Process(in, out, N);
                    frames += N;
                }
                delete gp; free(lg); free(sm); free(in); free(out);
            }
        }
    }
    printf("clouds_asan: small %zu large %zu: %ld frames, 3 modes x 4 qualities x 3 seeds, no report\n", small, large, frames);
    return 0;
}
