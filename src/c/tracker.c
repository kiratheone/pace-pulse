#include "tracker.h"

#include <limits.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

#define DEG_TO_RAD (3.14159265359f / 180.0f)
#define METERS_PER_DEGREE 111000.0f
#define MIN_STEP_DISTANCE_MM 20000U
#define MAX_POSITION_INTERVAL_MS 120000U
#define MAX_RUNNING_SPEED_MPS 15U
#define PACE_AVERAGING_DISTANCE_MM 200000U

static uint32_t prv_clamp_u64(uint64_t value) {
  return value > UINT32_MAX ? UINT32_MAX : (uint32_t)value;
}

static float prv_fast_sqrtf(float value) {
  union {
    float floating_point;
    uint32_t integer;
  } approximation = {value};
  approximation.integer = 0x1FBD1DF5U + (approximation.integer >> 1);
  return approximation.floating_point;
}

static float prv_euclidean_distance(float x, float y) {
  if (fabsf(x) < 0.01f) {
    return fabsf(y);
  }
  if (fabsf(y) < 0.01f) {
    return fabsf(x);
  }
  return prv_fast_sqrtf(x * x + y * y);
}

static void prv_set_baseline(Tracker *tracker, TrackerPosition position) {
  tracker->baseline = position;
  tracker->baseline_valid = true;
}

static void prv_reset_speed_buffer(Tracker *tracker) {
  memset(tracker->speed_buffer, 0, sizeof(tracker->speed_buffer));
  tracker->speed_buffer_position = 0;
  tracker->seconds_since_movement = TRACKER_PACE_STALE_SECONDS + 1;
}

static void prv_add_speed_sample(Tracker *tracker, uint32_t delta_ms,
                                 uint32_t distance_mm) {
  TrackerSpeedSample *sample =
      &tracker->speed_buffer[tracker->speed_buffer_position];
  sample->delta_ms = delta_ms;
  sample->distance_mm = distance_mm;
  sample->valid = true;
  tracker->speed_buffer_position =
      (tracker->speed_buffer_position + 1) % TRACKER_SPEED_BUFFER_SIZE;
  tracker->seconds_since_movement = 0;
}

void tracker_init(Tracker *tracker) {
  memset(tracker, 0, sizeof(*tracker));
  tracker->state = TRACKER_WAITING_FOR_GPS;
  prv_reset_speed_buffer(tracker);
}

bool tracker_position_is_valid(const TrackerPosition *position) {
  return position != NULL && position->latitude_e6 >= -90000000 &&
         position->latitude_e6 <= 90000000 &&
         position->longitude_e6 >= -180000000 &&
         position->longitude_e6 <= 180000000 && position->accuracy_dm >= 0 &&
         position->accuracy_dm <= 200;
}

uint32_t tracker_position_distance_mm(const TrackerPosition *first,
                                      const TrackerPosition *second) {
  if (first == NULL || second == NULL) {
    return 0;
  }

  int64_t longitude_delta_e6 =
      (int64_t)second->longitude_e6 - first->longitude_e6;
  if (longitude_delta_e6 > 180000000) {
    longitude_delta_e6 -= 360000000;
  } else if (longitude_delta_e6 < -180000000) {
    longitude_delta_e6 += 360000000;
  }

  int64_t latitude_delta_e6 =
      (int64_t)second->latitude_e6 - first->latitude_e6;
  float latitude_delta_m =
      latitude_delta_e6 * 1e-6f * METERS_PER_DEGREE;
  float first_latitude_rad = first->latitude_e6 * 1e-6f * DEG_TO_RAD;
  float longitude_delta_m = longitude_delta_e6 * 1e-6f *
                            METERS_PER_DEGREE * cosf(first_latitude_rad);
  float distance_m =
      prv_euclidean_distance(latitude_delta_m, longitude_delta_m);

  if (!isfinite(distance_m) || distance_m <= 0.0f) {
    return 0;
  }
  if (distance_m >= UINT32_MAX / 1000.0f) {
    return UINT32_MAX;
  }
  return (uint32_t)(distance_m * 1000.0f + 0.5f);
}

TrackerPositionResult tracker_accept_position(Tracker *tracker,
                                              TrackerPosition position) {
  if (!tracker_position_is_valid(&position)) {
    return TRACKER_POSITION_REJECTED;
  }

  if (tracker->state == TRACKER_WAITING_FOR_GPS) {
    tracker->state = TRACKER_READY;
    prv_set_baseline(tracker, position);
    return TRACKER_POSITION_BASELINE;
  }

  if (tracker->state != TRACKER_RUNNING || !tracker->baseline_valid) {
    prv_set_baseline(tracker, position);
    return TRACKER_POSITION_BASELINE;
  }

  uint32_t delta_ms = position.timestamp_ms - tracker->baseline.timestamp_ms;
  uint32_t distance_mm =
      tracker_position_distance_mm(&tracker->baseline, &position);
  if (delta_ms == 0 || delta_ms > MAX_POSITION_INTERVAL_MS ||
      (uint64_t)distance_mm > (uint64_t)delta_ms * MAX_RUNNING_SPEED_MPS) {
    prv_set_baseline(tracker, position);
    return TRACKER_POSITION_BASELINE;
  }
  if (distance_mm <= MIN_STEP_DISTANCE_MM) {
    return TRACKER_POSITION_REJECTED;
  }

  tracker->distance_mm += distance_mm;
  prv_add_speed_sample(tracker, delta_ms, distance_mm);
  prv_set_baseline(tracker, position);
  return TRACKER_POSITION_COMMITTED;
}

bool tracker_start(Tracker *tracker) {
  if (tracker->state != TRACKER_READY) {
    return false;
  }
  tracker->state = TRACKER_RUNNING;
  tracker->elapsed_seconds = 0;
  tracker->distance_mm = 0;
  tracker->baseline_valid = false;
  prv_reset_speed_buffer(tracker);
  return true;
}

void tracker_pause(Tracker *tracker) {
  if (tracker->state == TRACKER_RUNNING) {
    tracker->state = TRACKER_PAUSED;
    tracker->baseline_valid = false;
  }
}

bool tracker_resume(Tracker *tracker) {
  if (tracker->state != TRACKER_PAUSED) {
    return false;
  }
  tracker->state = TRACKER_RUNNING;
  tracker->baseline_valid = false;
  tracker->seconds_since_movement = TRACKER_PACE_STALE_SECONDS + 1;
  return true;
}

void tracker_tick(Tracker *tracker) {
  if (tracker->state != TRACKER_RUNNING) {
    return;
  }
  if (tracker->elapsed_seconds < UINT32_MAX) {
    tracker->elapsed_seconds++;
  }
  if (tracker->seconds_since_movement < UINT32_MAX) {
    tracker->seconds_since_movement++;
  }
}

TrackerState tracker_state(const Tracker *tracker) {
  return tracker->state;
}

uint32_t tracker_elapsed_seconds(const Tracker *tracker) {
  return tracker->elapsed_seconds;
}

uint64_t tracker_distance_mm(const Tracker *tracker) {
  return tracker->distance_mm;
}

bool tracker_current_pace(const Tracker *tracker, uint32_t *pace_seconds_per_km) {
  if (pace_seconds_per_km == NULL ||
      tracker->seconds_since_movement > TRACKER_PACE_STALE_SECONDS) {
    return false;
  }

  uint32_t total_time_ms = 0;
  uint32_t total_distance_mm = 0;
  for (uint16_t offset = 0; offset < TRACKER_SPEED_BUFFER_SIZE; offset++) {
    uint16_t index =
        (tracker->speed_buffer_position + TRACKER_SPEED_BUFFER_SIZE - 1 - offset) %
        TRACKER_SPEED_BUFFER_SIZE;
    const TrackerSpeedSample *sample = &tracker->speed_buffer[index];
    if (!sample->valid) {
      break;
    }
    if (sample->delta_ms > UINT32_MAX - total_time_ms ||
        sample->distance_mm > UINT32_MAX - total_distance_mm) {
      return false;
    }
    total_time_ms += sample->delta_ms;
    total_distance_mm += sample->distance_mm;
    if (total_distance_mm >= PACE_AVERAGING_DISTANCE_MM) {
      break;
    }
  }
  if (total_distance_mm == 0 || total_time_ms > UINT32_MAX / 1000U) {
    return false;
  }

  *pace_seconds_per_km = total_time_ms * 1000U / total_distance_mm;
  return true;
}

bool tracker_overall_pace(const Tracker *tracker, uint32_t *pace_seconds_per_km) {
  if (pace_seconds_per_km == NULL || tracker->distance_mm == 0) {
    return false;
  }
  *pace_seconds_per_km = prv_clamp_u64(
      (uint64_t)tracker->elapsed_seconds * 1000000ULL / tracker->distance_mm);
  return true;
}
