#include "VIA_Battery.h"
int main() {
  via::BatteryMgr battery;
  battery.setVref(5000);
  battery.setCalibration(3200, 4200);
  battery.setAverageSamples(1);
  uint16_t adc4200 = battery.rawFromMv(4200);
  battery.update(adc4200, 0);
  int p = battery.percentage();
  if (p < 99 || p > 100) return 1;
  uint16_t adc3200 = battery.rawFromMv(3200);
  battery.update(adc3200, 1);
  p = battery.percentage();
  if (p < 0 || p > 1) return 2;
  uint16_t adc3700 = battery.rawFromMv(3700);
  battery.update(adc3700, 2);
  p = battery.percentage();
  if (p < 49 || p > 51) return 3;
  battery.update(battery.rawFromMv(3000), 3);
  if (battery.percentage() != 0) return 4;
  battery.charging(true);
  if (!battery.charging()) return 5;
  battery.charging(false);
  if (battery.charging()) return 6;
  battery.setAverageSamples(4);
  for (int i = 0; i < 4; ++i)
    battery.update(battery.rawFromMv(3600), 10u + i);
  p = battery.percentage();
  if (p < 39 || p > 41) return 7;
  return 0;
}
