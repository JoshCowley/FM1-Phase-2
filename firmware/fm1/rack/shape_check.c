/* SPDX-License-Identifier: GPL-3.0 */
/* FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md. */
/* rack_host check 16 (FM1 Phase 2, update 61): the Braids shapes and Rings models the firmware no longer offers.
 * 16a the 14 withdrawn Braids shapes read back, once set, as the kept shape that is their nearest (the table below, written out here on purpose, not read from the engine),
 *     the 33 kept ones as themselves; Rings: MODAL, SYMPATH and SYM QNT read back as STRING, the other three as themselves;
 * 16b a withdrawn shape plays sample for sample as the shape it maps to (INIT, one held note, the first 9 blocks, 53 ms: the noise sources of Braids' own keep their state from one
 *     render to the next, so a shape that uses one differs from about the tenth block whichever shape it is compared with). */
#define SHC_BLOCKS 6   /* (2.48: 9 before; with check 6 and 7 gone the noise sources start this check in another state, and one shape (31) leaves its twin from the seventh block) */
static int shc_render(int shape, int32_t *out)
{
    int b;
    rk_seed(0x21u); rc_pause(); rc_load(0); rc_resume();
    rc_set(RS_SRC, 0, (float)shape);
    rc_note_on(57, 0.8f);
    for (b = 0; b < 24; b++)
        rc_render(out + b * 512, 256, 8388607.0f * 0.7f);
    return 0;
}
static void shape_checks(int *fails)
{
    static const int GONE[14][2] = {{26, 25}, {28, 0}, {29, 21}, {30, 17}, {31, 22}, {32, 13}, {33, 12}, {34, 3}, {35, 14}, {36, 44}, {41, 16}, {42, 46}, {43, 16}, {45, 46}};
    static int32_t a[24 * 512], b[24 * 512];
    int i, j, bad = 0, kept = 0, same = 0;
    rk_seed(0x21u); rc_pause(); rc_load(0); rc_resume();
    for (i = 0; i < 47; i++) {
        int want = i;
        for (j = 0; j < 14; j++)
            if (GONE[j][0] == i) want = GONE[j][1];
        rc_set(RS_SRC, 0, (float)i);
        if ((int)(rk_get_param(RS_SRC, 0) + 0.5f) != want || rk_list_map(2, i) != want) { bad++; printf("16a Braids shape %d reads %d, wanted %d\n", i, (int)(rk_get_param(RS_SRC, 0) + 0.5f), want); }
        kept += want == i;
    }
    rk_set_module(RS_SHP, 10);
    for (i = 0; i < 6; i++) {
        int want = i == 0 || i == 1 || i == 4 ? 2 : i;
        rc_set(RS_SHP, 0, (float)i);
        if ((int)(rk_get_param(RS_SHP, 0) + 0.5f) != want) { bad++; printf("16a Rings model %d reads %d, wanted %d\n", i, (int)(rk_get_param(RS_SHP, 0) + 0.5f), want); }
    }
    for (j = 0; j < 14; j++) {
        int k;
        shc_render(GONE[j][0], a); shc_render(GONE[j][1], b);
        for (k = 0; k < SHC_BLOCKS * 512 && a[k] == b[k]; k++) ;
        if (k < SHC_BLOCKS * 512) { bad++; printf("16b Braids shape %d does not play as %d (sample %d)\n", GONE[j][0], GONE[j][1], k); }
        else same++;
    }
    printf("16 withdrawn shapes: %d of 47 Braids shapes kept, the 14 withdrawn read back as their nearest and %d of 14 play sample for sample as it; Rings MODAL, SYMPATH, SYM QNT read STRING: %d wrong%s\n", kept, same, bad, bad ? "   FAIL" : "");
    if (bad || kept != 33)
        ++*fails;
    {   /* 16c (2.43): SAW COMB's delay line (16 KB a voice) is in the arena only for a patch that can reach the shape: INIT has none; setting the shape asks for a fit
         * (fm1_fit_needed, the menu's MEMORY FITTED); before it, the comb is skipped (the saw plays) and nothing breaks */
        static int32_t c1[24 * 512], c3[24 * 512];
        int u0, u1, need0, need1, v, k, diff13 = 0, bad3 = 0;
        rk_seed(0x21u); rc_pause(); rc_load(0); rc_resume();
        v = rk_fm1_status(2); u0 = rk_fm1_status(0);
        rc_set(RS_SRC, 0, 15.0f);                        /* SAW COMB */
        need0 = rk_fm1_fit_needed();
        shc_render(15, c3);                              /* no refit: no delay line */
        rk_seed(0x21u); rc_pause(); rc_load(0); rc_resume();
        rc_set(RS_SRC, 0, 15.0f); rc_pause(); rk_fm1_fit(); rc_resume(); rk_seed(0x21u);
        u1 = rk_fm1_status(0); need1 = rk_fm1_fit_needed();
        rc_note_on(57, 0.8f);
        for (k = 0; k < 24; k++) rc_render(c1 + k * 512, 256, 8388607.0f * 0.7f);
        for (k = 0; k < 24 * 512; k++) { diff13 += c1[k] != c3[k]; bad3 += c3[k] > 8388607 || c3[k] < -8388608; }
        printf("16c SAW COMB's delay line: INIT uses %d B with %d voices and no comb; setting the shape asks for a fit (%d, then %d); with it %d B (+%d, %d a voice); without it the comb is skipped and the saw plays (differs from the comb in %d samples)%s\n",
               u0, v, need0, need1, u1, u1 - u0, v ? (u1 - u0) / v : 0, diff13, need0 != 1 || need1 != 0 || u1 - u0 != v * 16384 || diff13 < 1000 || bad3 ? "   FAIL" : "");
        if (need0 != 1 || need1 != 0 || u1 - u0 != v * 16384 || diff13 < 1000 || bad3)
            ++*fails;
    }
}
