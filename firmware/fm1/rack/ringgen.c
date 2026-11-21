/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Rack firmware: the rhythm rings' maths, shared by RHYTHM (play.c rg_*) and the GATE PATTERN (gatepat.c), which
 * rack_host.c checks too. Moved out of play.c unchanged (the same dice in the same order: RHYTHM plays as before). */
static uint32_t rgn_rnd = 0x9E3779B9u;
static float rgn_rand(void)
{
    rgn_rnd ^= rgn_rnd << 13; rgn_rnd ^= rgn_rnd >> 17; rgn_rnd ^= rgn_rnd << 5;
    return (float)(rgn_rnd >> 8) * (1.0f / 16777216.0f);
}
static uint32_t rgn_rot(uint32_t m, int n, int r)   /* turned r steps later round n */
{
    uint32_t all = n >= 32 ? 0xFFFFFFFFu : (1u << n) - 1u;
    r %= n;
    return r ? ((m << r) | (m >> (n - r))) & all : m & all;
}
static uint32_t rgn_euclid(int n, int h)      /* h hits as evenly as they go round n steps (Euclid / Bjorklund) */
{
    uint32_t m = 0;
    int i;
    n = n < 2 ? 2 : n > 32 ? 32 : n;
    h = h > n ? n : h;
    for (i = 0; i < n; i++)
        if ((i * h) % n < h) m |= 1u << i;
    return m;
}
static void rgn_mutate(uint32_t *live, uint32_t base, int n, float mut)   /* the end of a cycle: perhaps a change */
{
    int i;
    if (rgn_rand() >= mut)
        return;
    i = (int)(rgn_rand() * 4.0f);
    if (i == 0) {
        *live = base;                         /* back to the pattern as set */
    } else if (i == 1) {
        *live = rgn_rot(*live, n, rgn_rand() < 0.5f ? 1 : n - 1);
    } else {
        uint32_t b = 1u << (int)(rgn_rand() * (float)n);
        if (!(*live & b) || (*live & ~b))     /* (the last hit stays) */
            *live ^= b;
    }
}
static int rgn_chance(float c) { return c >= 0.999f || rgn_rand() < c; }   /* a hit plays? */
