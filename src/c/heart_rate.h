#pragma once

#include <stdbool.h>
#include <stdint.h>

#define HEART_RATE_STALE_SECONDS 15

typedef enum {
  HEART_RATE_ZONE_NONE = 0,
  HEART_RATE_ZONE_FAT_BURN = 1,
  HEART_RATE_ZONE_ENDURANCE = 2,
  HEART_RATE_ZONE_PERFORMANCE = 3,
} HeartRateZone;

typedef struct {
  int32_t bpm;
  uint32_t age_seconds;
  bool available;
} HeartRateState;

void heart_rate_init(HeartRateState *state);
bool heart_rate_update(HeartRateState *state, int32_t bpm);
void heart_rate_tick(HeartRateState *state, bool running);
bool heart_rate_current(const HeartRateState *state, int32_t *bpm);
void heart_rate_reset(HeartRateState *state);
HeartRateZone heart_rate_zone(int32_t bpm);
