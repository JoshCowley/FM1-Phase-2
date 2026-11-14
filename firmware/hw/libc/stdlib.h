// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
/* Freestanding <stdlib.h> for fm1-base builds (see README.md): abs inline, the rest declared only (not linked) */
#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
static inline int abs(int x) { return x < 0 ? -x : x; }
int rand(void); void *malloc(size_t); void free(void *);
#ifdef __cplusplus
}
#endif
