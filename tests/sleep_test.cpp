#include "VIA_SleepMgr.h"
int main() {
  via::SleepMgr sleep;
  sleep.configure(5000);
  if (sleep.update(true, 0)) return 1;
  if (sleep.update(false, 4000)) return 2;
  if (!sleep.update(false, 5100)) return 3;
  if (sleep.update(false, 5101)) return 4;
  sleep.update(true, 10000);
  if (sleep.update(false, 14000)) return 5;
  sleep.configure(1000);
  if (sleep.update(true, 0xFFFFFFFF - 500)) return 6;
  if (!sleep.update(false, 600)) return 7;
  return 0;
}
