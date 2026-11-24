// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* Update 41, step 2: what the four factory drum kits sound like, measured; included by rack_host.c after drumseq.c.
 *   rack_host --dsound table          each kit's lanes struck once, dry: peak, kick fundamental (start, settled), T-20 / T-40 dB, share of energy under 120 Hz,
 *                                     hat spectral centroid and share above 8 kHz
 *   rack_host --dsound bus            each kit's factory beat, 4 bars at 120 bpm: the drum bus' peak (the soft ceiling) and the render time against none
 *   rack_host --dsound wav OUT.wav KIT [BARS]   the beat as a 32-bit float stereo WAV at 48 kHz (the drum bus alone, the INIT patch silent) */
#include <time.h>
#define DS_BLK 48
static float ds_l[48000 * 12], ds_r[48000 * 12];
static int ds_n;
static void ds_hit(int voice, int model, float note, float p1, float p2, float decay, float level, float pan, float send)
{ rk_drum_hit(voice, model, note, p1, p2, decay, level, pan, send); }
static void ds_cut(int voice) { rk_drum_cut(voice); }
static void ds_start(void)
{
    rk_seed(0x21u); rc_load(0); rk_all_off(); rk_drum_off();
    dr_hook_hit = ds_hit; dr_hook_cut = ds_cut; dr_hook_midi = 0; dr_hook_run = 0; dr_hz = 48000.0f;
    ds_n = 0;
}
static void ds_render(int blocks)
{
    int b;
    for (b = 0; b < blocks && ds_n + DS_BLK <= 48000 * 12; b++) {
        rk_render(DS_BLK);
        memcpy(ds_l + ds_n, rk_out_l(), DS_BLK * sizeof(float));
        memcpy(ds_r + ds_n, rk_out_r(), DS_BLK * sizeof(float));
        ds_n += DS_BLK;
    }
}
static void ds_fft(double *re, double *im, int n)
{
    int i, j, k, m;
    for (i = 1, j = 0; i < n; i++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t; t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (m = 2; m <= n; m <<= 1) {
        double a = -2.0 * M_PI / m;
        for (k = 0; k < n; k += m)
            for (j = 0; j < m / 2; j++) {
                double c = cos(a * j), s = sin(a * j), xr = re[k + j + m / 2] * c - im[k + j + m / 2] * s, xi = re[k + j + m / 2] * s + im[k + j + m / 2] * c;
                re[k + j + m / 2] = re[k + j] - xr; im[k + j + m / 2] = im[k + j] - xi; re[k + j] += xr; im[k + j] += xi;
            }
    }
}
/* the spectrum of x[from, from+n) (n a power of 2, Hann): power per bin into p[0..n/2) */
static void ds_spec(const float *x, int from, int n, double *p)
{
    static double re[16384], im[16384];
    int i;
    for (i = 0; i < n; i++) { re[i] = x[from + i] * (0.5 - 0.5 * cos(2.0 * M_PI * i / n)); im[i] = 0; }
    ds_fft(re, im, n);
    for (i = 0; i < n / 2; i++) p[i] = re[i] * re[i] + im[i] * im[i];
}
static double ds_peakhz(const float *x, int from, int n, double lo, double hi)
{
    static double p[16384];
    int i, best = 0;
    double bp = -1;
    ds_spec(x, from, n, p);
    for (i = 1; i < n / 2; i++) {
        double f = i * 48000.0 / n;
        if (f >= lo && f <= hi && p[i] > bp) { bp = p[i]; best = i; }
    }
    return best * 48000.0 / n;
}
/* the frequency from the zero crossings between samples a and b (3-25 ms) */
static double ds_zc(const float *x, int a, int b)
{
    int i, n = 0, first = -1, last = -1;
    for (i = a; i < b; i++)
        if (x[i] <= 0.0f && x[i + 1] > 0.0f) { if (first < 0) first = i; last = i; n++; }
    return n > 1 ? (n - 1) * 48000.0 / (last - first) : 0.0;
}
static double ds_tfall(const float *x, int n, double db)   /* ms from the loudest 5 ms to the first 5 ms window db below it (and staying) */
{
    int w = 240, nw = n / w, i, pk = 0, t = -1;
    static double e[2000];
    double top = 0;
    for (i = 0; i < nw && i < 2000; i++) {
        int j;
        double s = 0;
        for (j = 0; j < w; j++) s += (double)x[i * w + j] * x[i * w + j];
        e[i] = sqrt(s / w);
        if (e[i] > top) { top = e[i]; pk = i; }
    }
    for (i = nw - 1; i > pk; i--)
        if (e[i] > top * pow(10.0, -db / 20.0)) { t = i + 1; break; }
    return t < 0 ? 0.0 : (t - pk) * w * 1000.0 / 48000.0;
}
static void ds_one(int kit, int lane, int verbose)
{
    const dr_snd_t *k;
    static double p[8192];
    double tot = 0, low = 0, cen = 0, hi = 0, pw = 0;
    float pk = 0;
    double rms;
    int i;
    dr_load_kit(kit);
    k = &dr_kit[lane];
    ds_start();
    rk_drum_hit(DR_VOICE[lane], k->model, (float)k->pitch, k->p1 * 0.01f, k->p2 * 0.01f, k->decay_cs * 0.01f, k->level * 0.01f, 0.0f, 0.0f);
    ds_render(1500);
    for (i = 0; i < ds_n; i++) { float a = fabsf(ds_l[i]); if (a > pk) pk = a; }
    ds_spec(ds_l, 0, 8192, p);        /* the first 170 ms */
    for (i = 1; i < 4096; i++) {
        double f = i * 48000.0 / 8192.0;
        tot += p[i]; cen += p[i] * f; if (f < 120.0) low += p[i]; if (f > 8000.0) hi += p[i];
        (void)pw;
    }
    { double e = 0; for (i = 0; i < 14400; i++) e += (double)ds_l[i] * ds_l[i]; rms = 10.0 * log10(e / 14400 + 1e-12); }
    printf("  kit %d %-7s %-6s %-14s note %3d dec %3d lvl %3d | peak %.3f rms(0-300ms) %.1f dB", kit, DR_KNAME[kit], DR_LNAME[lane], DR_MNAME[k->model], k->pitch, k->decay_cs, k->level, pk, rms);
    if (lane == 0 || k->model == 0)
        printf(" | f0 start %.0f Hz, settled %.0f Hz | T-20 %.0f ms, T-40 %.0f ms | energy under 120 Hz %.0f%%", ds_zc(ds_l, 144, 2880),
               ds_peakhz(ds_l, 4800, 16384, 25, 200), ds_tfall(ds_l, ds_n, 20), ds_tfall(ds_l, ds_n, 40), 100.0 * low / (tot > 0 ? tot : 1));
    if (lane == 2 || lane == 3)
        printf(" | centroid %.0f Hz, above 8 kHz %.0f%% | T-20 %.0f ms, T-40 %.0f ms", tot > 0 ? cen / tot : 0, 100.0 * hi / (tot > 0 ? tot : 1), ds_tfall(ds_l, ds_n, 20), ds_tfall(ds_l, ds_n, 40));
    printf("\n");
    (void)verbose;
}
static double ds_now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec * 1e-6; }
/* a kit's beat, bars at 120 bpm; returns the bus peak; ms: the time rk_render took (best of 3) */
static float ds_beat(int kit, int bars, int play, double *ms)
{
    int pass;
    float pk = 0;
    double best = 1e30;
    for (pass = 0; pass < (ms ? 3 : 1); pass++) {
        uint32_t now = 0, blocks = (uint32_t)(bars * 2 * 48000 / DS_BLK);
        uint32_t b;
        double t0, t = 0;
        ds_start();
        rk_drum_run(play);
        if (dr.run) dr_stop();
        dr_reset_state(); dr_load_kit(kit); dr_load_beat();
        if (play) dr_start(0);
        for (b = 0; b < blocks; b++) {
            if (play) dr_block(now, DS_BLK, 60.0f / 120.0f / 4.0f * 48000.0f);
            t0 = ds_now();
            ds_render(1);
            t += ds_now() - t0;
            now += DS_BLK;
        }
        if (t < best) best = t;
    }
    { int i; for (i = 0; i < ds_n; i++) { float a = fabsf(ds_l[i]); if (a > pk) pk = a; } }
    if (getenv("DS_DEBUG")) printf("p00 %x l0 %d lcnt %d mute %d hits %d run %d ticks %u nq %d any %d\n", (unsigned)dr_pat[0][0], dr.lstep[0], dr.lcnt[0], dr.mute, (int)dr.hits, dr.run, (unsigned)dr.ticks, dr.nq, dr_any());
    if (ms) *ms = best;
    return pk;
}
static void ds_wav(const char *path)
{
    FILE *f = fopen(path, "wb");
    static float x[2 * 48000 * 12];
    uint32_t n = (uint32_t)ds_n * 8u, v;
    int i;
    for (i = 0; i < ds_n; i++) { x[2 * i] = ds_l[i]; x[2 * i + 1] = ds_r[i]; }
    fwrite("RIFF", 1, 4, f); v = 36u + n; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f); v = 3u | 2u << 16; fwrite(&v, 4, 1, f); v = 48000; fwrite(&v, 4, 1, f);
    v = 48000 * 8u; fwrite(&v, 4, 1, f); v = 8u | 32u << 16; fwrite(&v, 4, 1, f); fwrite("data", 1, 4, f); fwrite(&n, 4, 1, f);
    fwrite(x, 4, (size_t)ds_n * 2, f);
    fclose(f);
}
static int ds_main(int argc, char **argv)
{
    int k, l;
    if (argc >= 3 && !strcmp(argv[2], "table")) {
        for (k = 0; k < DR_NKIT; k++) for (l = 0; l < 4; l++) ds_one(k, l, 0);
        return 0;
    }
    if (argc >= 3 && !strcmp(argv[2], "bus")) {
        double base, ms;
        ds_beat(0, 4, 0, &base);
        for (k = 0; k < DR_NKIT; k++) {
            float pk = ds_beat(k, 4, 1, &ms);
            printf("  kit %d %-7s beat, 4 bars: drum bus + synth peak %.3f (%.1f dB), render %.1f ms against %.1f ms with no drums: +%.2f us a block (host)\n",
                   k, DR_KNAME[k], pk, 20.0 * log10(pk), ms, base, (ms - base) / (ds_n / DS_BLK) * 1000.0);
        }
        return 0;
    }
    if (argc >= 5 && !strcmp(argv[2], "wav")) {
        ds_beat(atoi(argv[4]), argc > 5 ? atoi(argv[5]) : 2, 1, 0);
        ds_wav(argv[3]);
        return 0;
    }
    return 1;
}
