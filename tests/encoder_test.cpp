#include "VIA_Encoder.h"
int main() {
  via::Encoder enc;
  enc.setDebounceUs(0);
  // CCW: 00→01→11→10→00 = 4 edges = count -1
  enc.update(0,0,0); enc.update(0,1,2); enc.update(1,1,4); enc.update(1,0,6); enc.update(0,0,8);
  // CW: 00→10→11→01→00 = 4 edges = count +1 → back to 0
  enc.update(0,0,10); enc.update(1,0,12); enc.update(1,1,14); enc.update(0,1,16); enc.update(0,0,18);
  // 2nd CCW = -1 again
  enc.update(0,0,20); enc.update(0,1,22); enc.update(1,1,24); enc.update(1,0,26); enc.update(0,0,28);
  if (enc.count() != -1) return 1;
  if (enc.consume() != -1) return 2;
  if (enc.count() != 0) return 3;
  // noise
  enc.update(0,0,30); enc.update(0,0,31);
  if (enc.count() != 0) return 4;
  // debounce rejects fast transitions
  enc.setDebounceUs(2000);
  enc.update(1,0,100); enc.update(1,1,101);
  if (enc.count() != 0) return 5;
  enc.setDebounceUs(0);
  // 2 cycles CCW = -2
  enc.update(0,0,200); enc.update(0,1,202); enc.update(1,1,204); enc.update(1,0,206); enc.update(0,0,208);
  enc.update(0,0,300); enc.update(0,1,302); enc.update(1,1,304); enc.update(1,0,306); enc.update(0,0,308);
  if (enc.count() != -2) return 6;
  return 0;
}
