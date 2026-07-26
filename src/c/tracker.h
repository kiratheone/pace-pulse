#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TRACKER_SPEED_BUFFER_SIZE 64
#define TRACKER_PACE_STALE_SECONDS 15

typedef enum {
  TRACKER_WAITING_FOR_GPS = 0,
  TRACKER_READY,
  TRACKER_RUNNING,
  TRACKER_PAUSED,
} TrackerState;

typedef enum {
  TRACKER_POSITION_REJECTED = 0,
  TRACKER_POSITION_BASELINE,
  TRACKER_POSITION_COMMITTED,
} TrackerPositionResult;

typedef struct {
  int32_t latitude_e6;
  int32_t longitude_e6;
  int32_t accuracy_dm;
  uint32_t timestamp_ms;
} TrackerPosition;

typedef struct {
  uint32_t delta_ms;
  uint32_t distance_mm;
  bool valid;
} TrackerSpeedSample;

typedef struct {
  TrackerState state;
  uint32_t elapsed_seconds;
  uint32_t seconds_since_movement;
  uint64_t distance_mm;
  TrackerPosition baseline;
  bool baseline_valid;
  uint16_t speed_buffer_position;
  TrackerSpeedSample speed_buffer[TRACKER_SPEED_BUFFER_SIZE];
} Tracker;

void tracker_init(Tracker *tracker);
bool tracker_position_is_valid(const TrackerPosition *position);
uint32_t tracker_position_distance_mm(const TrackerPosition *first,
                                      const TrackerPosition *second);
TrackerPositionResult tracker_accept_position(Tracker *tracker,
                                              TrackerPosition position);
bool tracker_start(Tracker *tracker);
void tracker_pause(Tracker *tracker);
bool tracker_resume(Tracker *tracker);
void tracker_tick(Tracker *tracker);
TrackerState tracker_state(const Tracker *tracker);
uint32_t tracker_elapsed_seconds(const Tracker *tracker);
uint64_t tracker_distance_mm(const Tracker *tracker);
bool tracker_current_pace(const Tracker *tracker, uint32_t *pace_seconds_per_km);
bool tracker_overall_pace(const Tracker *tracker, uint32_t *pace_seconds_per_km);
