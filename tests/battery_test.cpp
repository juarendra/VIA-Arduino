#include "VIA_Battery.h"
int main() {
  via::BatteryMgr battery;
  battery.setVref(5000);
  battery.setAdcBits(12);
  battery.setCalibration(3200, 4200);
  uint16_t r = battery.rawFromMv(4200);
  if (r == 0) return 1;
  if (battery.percentage() != 0) return 2;
  battery.update(r, 0);
  if (battery.percentage() != 100) return 3;
  battery.update(battery.rawFromMv(3200), 1);
  if (battery.percentage() != 0) return 4;
  battery.update(battery.rawFromMv(3700), 2);
  if (battery.percentage() != 50) return 5;
  battery.update(battery.rawFromMv(3000), 3);
  if (battery.percentage() != 0) return 6;
  battery.charging(true);
  if (!battery.charging()) return 7;
  battery.charging(false);
  if (battery.charging()) return 8;
  battery.setAverageSamples(4);
  for (int i = 0; i < 4; ++i)
    battery.update(battery.rawFromMv(3600), 10u + i);
  if (battery.percentage() != 40) return 9;
  return 0;
}
