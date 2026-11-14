// SPDX-License-Identifier: GPL-3.0
// FM1 Phase 2: licensed GPL-3.0, see LICENSE; third-party code and its licences: NOTICE.md.
#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_
#include "stmlib/stmlib.h"
namespace plaits {
class UserData {
 public:
  inline const uint8_t* ptr(int slot) const { return 0; }
};
}
#endif
