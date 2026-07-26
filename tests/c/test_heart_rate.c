#include <assert.h>
#include <stdint.h>

#include "heart_rate.h"

int main(void) {
  HeartRateState heart_rate;
  heart_rate_init(&heart_rate);

  int32_t bpm = 0;
  assert(!heart_rate_current(&heart_rate, &bpm));
  assert(!heart_rate_update(&heart_rate, 0));
  assert(!heart_rate_update(&heart_rate, -1));

  assert(heart_rate_update(&heart_rate, 154));
  assert(heart_rate_current(&heart_rate, &bpm));
  assert(bpm == 154);
  assert(!heart_rate_update(&heart_rate, 0));
  assert(!heart_rate_current(&heart_rate, &bpm));
  assert(heart_rate_update(&heart_rate, 154));

  for (uint32_t i = 0; i < HEART_RATE_STALE_SECONDS; i++) {
    heart_rate_tick(&heart_rate, true);
  }
  assert(heart_rate_current(&heart_rate, &bpm));

  heart_rate_tick(&heart_rate, false);
  assert(heart_rate_current(&heart_rate, &bpm));

  heart_rate_tick(&heart_rate, true);
  assert(!heart_rate_current(&heart_rate, &bpm));

  assert(heart_rate_update(&heart_rate, 120));
  heart_rate_reset(&heart_rate);
  assert(!heart_rate_current(&heart_rate, &bpm));
  return 0;
}
