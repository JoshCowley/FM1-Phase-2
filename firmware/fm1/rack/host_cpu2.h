// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* The second core on a computer (update 9), for rack_host.c's checks: the platform's worker calls (plat_cpu2_run, _wait, _sync, _on) standing
 * in for the FM-1's. Two ways to run a job:
 *   HC2_DEFER   the job runs on the calling thread when it is waited for (so the random numbers are drawn in the order one core draws
 *               them: the output can be compared sample for sample with one core's)
 *   HC2_NOW     the job runs at once, when handed over (the platform's own host simulator does: plat_host.c), so a job runs where one core
 *               would have run the same work and draws its random numbers in the one-core order
 *   HC2_THREAD  the job runs on a real second thread, at the same time as the caller (what the FM-1 does: for ThreadSanitizer, and
 *               for the races a deferred job cannot show)
 * and a way to fail, as the platform can (fm1-base device/cpu2.c): hc2_norun_pct: plat_cpu2_run says 0 (it can't take the job: do it
 * yourself); hc2_giveup_pct: it takes the job, then plat_cpu2_wait says -1 (given up, the job dropped; the platform's "misread" job
 * never ran, and its "late" one is retired: do it yourself). Both from a seeded generator, per call. */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { HC2_DEFER, HC2_THREAD, HC2_NOW };
static int hc2_mode = HC2_DEFER, hc2_on = 1;
static int hc2_norun_pct = 0, hc2_giveup_pct = 0;
static uint32_t hc2_seed = 1, hc2_runs = 0, hc2_norun = 0, hc2_giveups = 0, hc2_syncs = 0;
static void (*hc2_job)(uint32_t);
static uint32_t hc2_arg;
static int hc2_dropped, hc2_out;           /* a job is out and was given up; a job is out */
static pthread_t hc2_th;
#define HC2_STACK 65536
static uint32_t hc2_depth;                 /* the deepest a job has gone below the thread's own frame, bytes */
static unsigned char *hc2_stk;             /* the thread's stack, painted, to see how deep a job goes (the FM-1's: 2816 B; this host's frames are larger) */
static pthread_mutex_t hc2_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hc2_cv = PTHREAD_COND_INITIALIZER;
static int hc2_pending, hc2_done, hc2_quit, hc2_started;

static int hc2_roll(int pct)
{
    hc2_seed = hc2_seed * 1664525u + 1013904223u;
    return (int)((hc2_seed >> 16) % 100u) < pct;
}
static void *hc2_main(void *x)
{
    (void)x;
    pthread_mutex_lock(&hc2_mu);
    for (;;) {
        void (*fn)(uint32_t);
        uint32_t arg;
        while (!hc2_pending && !hc2_quit)
            pthread_cond_wait(&hc2_cv, &hc2_mu);
        if (hc2_quit)
            break;
        fn = hc2_job; arg = hc2_arg; hc2_pending = 0;
        pthread_mutex_unlock(&hc2_mu);
        {   /* paint everything below this frame, run the job, see how far down it went */
            unsigned char here, *top = &here - 256, *p;
            uint32_t used = 0;
            if (hc2_stk) memset(hc2_stk, 0xA5, (size_t)(top - hc2_stk));
            fn(arg);                               /* the job itself, outside the lock */
            if (hc2_stk) {
                for (p = hc2_stk; p < top && *p == 0xA5; p++) ;
                used = (uint32_t)(&here - p);
                if (used > hc2_depth) hc2_depth = used;
            }
        }
        pthread_mutex_lock(&hc2_mu);
        hc2_done = 1;
        pthread_cond_broadcast(&hc2_cv);
    }
    pthread_mutex_unlock(&hc2_mu);
    return 0;
}
static int hc2_cpu_run(void (*fn)(uint32_t), uint32_t arg)
{
    if (!hc2_on || hc2_out)
        return 0;
    hc2_runs++;
    if (hc2_roll(hc2_norun_pct)) { hc2_norun++; return 0; }
    hc2_out = 1;
    hc2_dropped = hc2_roll(hc2_giveup_pct);
    if (hc2_dropped) { hc2_giveups++; return 1; }  /* taken, then given up at the wait: it never runs */
    hc2_job = fn; hc2_arg = arg;
    if (hc2_mode == HC2_NOW) { hc2_job = 0; fn(arg); }
    else if (hc2_mode == HC2_THREAD) {
        if (!hc2_started) {
            pthread_attr_t at;
            hc2_stk = (unsigned char *)malloc(HC2_STACK);
            memset(hc2_stk, 0xA5, HC2_STACK);
            pthread_attr_init(&at);
            pthread_attr_setstack(&at, hc2_stk, HC2_STACK);
            pthread_create(&hc2_th, &at, hc2_main, 0);
            hc2_started = 1;
        }
        pthread_mutex_lock(&hc2_mu);
        hc2_done = 0; hc2_pending = 1;
        pthread_cond_broadcast(&hc2_cv);
        pthread_mutex_unlock(&hc2_mu);
    }
    return 1;
}
static int hc2_cpu_wait(void)
{
    hc2_out = 0;
    if (hc2_dropped) { hc2_dropped = 0; return -1; }
    if (hc2_mode == HC2_THREAD) {
        pthread_mutex_lock(&hc2_mu);
        while (!hc2_done)
            pthread_cond_wait(&hc2_cv, &hc2_mu);
        pthread_mutex_unlock(&hc2_mu);
    } else if (hc2_job) {
        void (*fn)(uint32_t) = hc2_job;
        hc2_job = 0;
        fn(hc2_arg);
    }
    return 0;
}
static void hc2_cpu_sync(void) { hc2_syncs++; __sync_synchronize(); }
static int hc2_cpu_on(void) { return hc2_on; }
/* the deepest a job has gone below the thread's frame, bytes (read after hc2_stop: the thread has been joined) */
static uint32_t hc2_stack_used(void) { return hc2_depth; }
static void hc2_stop(void)
{
    if (hc2_started) {
        pthread_mutex_lock(&hc2_mu);
        hc2_quit = 1;
        pthread_cond_broadcast(&hc2_cv);
        pthread_mutex_unlock(&hc2_mu);
        pthread_join(hc2_th, 0);
        hc2_started = 0; hc2_quit = 0;
    }
}
