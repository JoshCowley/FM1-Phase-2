/* SPDX-License-Identifier: MPL-2.0 */
/* FM1 Rack firmware: JAM's page (update 28), part of app.c (menus.c includes it): the settings of jam.c on a fixed page
 * of their own (as SEL, ARP, GLO: the values live in jam_v, not in the patch, and are not kept across a power-up).
 * Seven pages: JAM (ON, SPLIT), then two moves to a page, each its AMOUNT and its key (HOLD or LATCH). */
#define JAM_OPT_BASE 0xFF00u                  /* a list option from JAM_OPT, not UI_OPT (menus.c fmt) */
static const char *const JAM_OPT[] = {"OFF", "ON", "HOLD", "LATCH"};
#define JO_OFFON (JAM_OPT_BASE + 0)
#define JO_KEY (JAM_OPT_BASE + 2)
static const char *const JAM_SHORT[JAM_NM] = {"GRAIN", "STUT 8", "STUT 16", "STUT 32", "SCRUB", "TAPE", "WASH", "FREEZE", "GLITCH", "DIVE", "RISE", "CRUSH"};
static const char *const JAM_PAGE[7] = {"JAM MODE", "GRAIN + STUT 8", "STUT 16 + 32", "SCRUB + TAPE", "WASH + FREEZE", "GLITCH + DIVE", "RISE + CRUSH"};
static const uint8_t UG_JAM[] = {2, 4, 4, 4, 4, 4, 4};
#define JP(l) {l, 0, 0, 0, 1, 0, 0.8f}, {"KEY", 2, 0, 0, 1, JO_KEY, 0.0f}
static const ui_param_t UP_JAM[] = {
    {"JAM", 2, 0, 0, 1, JO_OFFON, 0.0f}, {"SPLIT KEY", 3, 0, 1, 26, 0, 15.0f},
    JP("GRAIN"), JP("STUT 8"), JP("STUT 16"), JP("STUT 32"), JP("SCRUB"), JP("TAPE"),
    JP("WASH"), JP("FREEZE"), JP("GLITCH"), JP("DIVE"), JP("RISE"), JP("CRUSH"),
};
static const ui_mod_t UI_JAM = {"JAM", "PERFORM", 0, 7, 7, 26, UG_JAM, UP_JAM, 0, {0}, 0};
