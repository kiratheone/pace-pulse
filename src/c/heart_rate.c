#include "heart_rate.h"

#include <limits.h>
#include <stddef.h>

void heart_rate_init(HeartRateState *state) {
  state->bpm = 0;
  state->age_seconds = 0;
  state->available = false;
}

bool heart_rate_update(HeartRateState *state, int32_t bpm) {
  if (bpm <= 0) {
    heart_rate_reset(state);
    return false;
  }
  state->bpm = bpm;
  state->age_seconds = 0;
  state->available = true;
  return true;
}

void heart_rate_tick(HeartRateState *state, bool running) {
  if (!running || !state->available) {
    return;
  }
  if (state->age_seconds < UINT32_MAX) {
    state->age_seconds++;
  }
  if (state->age_seconds > HEART_RATE_STALE_SECONDS) {
    state->available = false;
  }
}

bool heart_rate_current(const HeartRateState *state, int32_t *bpm) {
  if (!state->available || bpm == NULL) {
    return false;
  }
  *bpm = state->bpm;
  return true;
}

void heart_rate_reset(HeartRateState *state) {
  state->bpm = 0;
  state->age_seconds = 0;
  state->available = false;
}

HeartRateZone heart_rate_zone(int32_t bpm) {
  if (bpm < 132) {
    return HEART_RATE_ZONE_NONE;
  }
  if (bpm <= 150) {
    return HEART_RATE_ZONE_FAT_BURN;
  }
  if (bpm <= 165) {
    return HEART_RATE_ZONE_ENDURANCE;
  }
  return HEART_RATE_ZONE_PERFORMANCE;
}
