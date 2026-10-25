#include <cassert>
#include <iostream>

#include "VIA_ESP32_GPIO.h"
#include "fakes/Arduino.h"

void test_input_pullup() {
  via::esp32::MatrixIOArduino io;
  io.inputPullup(5);
  assert(FakeArduino::mode[5] == INPUT_PULLUP);
}

void test_drive_low_and_release() {
  via::esp32::MatrixIOArduino io;
  io.driveLow(7);
  assert(FakeArduino::mode[7] == OUTPUT);
  assert(FakeArduino::value[7] == LOW);

  io.release(7);
  assert(FakeArduino::mode[7] == INPUT_PULLUP);
}

void test_matrix_press_and_release_polarity() {
  via::esp32::MatrixIOArduino io;
  const via::Pin rows[] = {4};
  const via::Pin columns[] = {16};
  uint32_t raw[1] = {}, candidate[1] = {}, stable[1] = {}, changed[1] = {};
  via::MatrixConfig config = {1, 1, rows, columns, via::kColToRow, 30, 5,
                              raw, candidate, stable, changed};
  via::Matrix matrix(config, io);
  assert(matrix.begin());

  FakeArduino::value[16] = HIGH;
  matrix.task(0);
  matrix.task(5);
  assert(matrix.rawRow(0) == 0);
  assert(matrix.stableRow(0) == 0);

  FakeArduino::value[16] = LOW;
  matrix.task(10);
  matrix.task(15);
  assert(matrix.stableRow(0) == 1);
  assert(matrix.changedRow(0) == 1);
  matrix.hasChanged();

  FakeArduino::value[16] = HIGH;
  matrix.task(20);
  matrix.task(25);
  assert(matrix.stableRow(0) == 0);
  assert(matrix.changedRow(0) == 1);
}

void test_delay_microseconds_returns() {
  via::esp32::MatrixIOArduino io;
  // The fake is a no-op; this call must simply return (guards against an
  // unqualified call resolving to the member and recursing forever).
  io.delayMicroseconds(100);
}

int main() {
  test_input_pullup();
  test_drive_low_and_release();
  test_matrix_press_and_release_polarity();
  test_delay_microseconds_returns();
  std::cout << "All tests passed!" << std::endl;
  return 0;
}
