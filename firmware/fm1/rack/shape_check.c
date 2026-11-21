/* SPDX-License-Identifier: GPL-3.0 */
/* FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md. */
/* rack_host check 16 (FM1 Phase 2, update 61): the Braids shapes and Rings models the firmware no longer offers.
 * 16a the 14 withdrawn Braids shapes read back, once set, as the kept shape that is their nearest (the table below, written out here on purpose, not read from the engine),
 *     the 33 kept ones as themselves; Rings: MODAL, SYMPATH and SYM QNT read back as STRING, the other three as themselves;
 * 16b a withdrawn shape plays sample for sample as the shape it maps to (INIT, one held note, the first 9 blocks, 53 ms: the noise sources of Braids' own keep their state from one
 *     render to the next, so a shape that uses one differs from about the tenth block whichever shape it is compared with). */
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
        for (k = 0; k < 9 * 512 && a[k] == b[k]; k++) ;
        if (k < 9 * 512) { bad++; printf("16b Braids shape %d does not play as %d (sample %d)\n", GONE[j][0], GONE[j][1], k); }
        else same++;
    }
    printf("16 withdrawn shapes: %d of 47 Braids shapes kept, the 14 withdrawn read back as their nearest and %d of 14 play sample for sample as it; Rings MODAL, SYMPATH, SYM QNT read STRING: %d wrong%s\n", kept, same, bad, bad ? "   FAIL" : "");
    if (bad || kept != 33)
        ++*fails;
}
