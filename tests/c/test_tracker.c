#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "tracker.h"

static TrackerPosition position(int32_t latitude_e6, int32_t longitude_e6,
                                int32_t accuracy_dm, uint32_t timestamp_ms) {
  TrackerPosition value = {
    .latitude_e6 = latitude_e6,
    .longitude_e6 = longitude_e6,
    .accuracy_dm = accuracy_dm,
    .timestamp_ms = timestamp_ms,
  };
  return value;
}

static void test_position_validation(void) {
  TrackerPosition valid = position(0, 0, 0, 0);
  TrackerPosition invalid_latitude = position(90000001, 0, 0, 0);
  TrackerPosition invalid_longitude = position(0, 180000001, 0, 0);
  TrackerPosition negative_accuracy = position(0, 0, -1, 0);
  TrackerPosition inaccurate = position(0, 0, 201, 0);
  assert(tracker_position_is_valid(&valid));
  assert(!tracker_position_is_valid(&invalid_latitude));
  assert(!tracker_position_is_valid(&invalid_longitude));
  assert(!tracker_position_is_valid(&negative_accuracy));
  assert(!tracker_position_is_valid(&inaccurate));
}

static void test_distance_is_non_negative_and_wraps_antimeridian(void) {
  TrackerPosition origin = position(0, 0, 0, 0);
  TrackerPosition west = position(0, -1000, 0, 1000);
  TrackerPosition south = position(-1000, 0, 0, 1000);
  uint32_t west_distance = tracker_position_distance_mm(&origin, &west);
  uint32_t south_distance = tracker_position_distance_mm(&origin, &south);
  assert(west_distance > 110000 && west_distance < 112500);
  assert(south_distance > 110000 && south_distance < 112500);

  TrackerPosition east_edge = position(0, 179999000, 0, 0);
  TrackerPosition west_edge = position(0, -179999000, 0, 1000);
  uint32_t wrapped_distance = tracker_position_distance_mm(&east_edge, &west_edge);
  assert(wrapped_distance > 200000 && wrapped_distance < 225000);
}

static void test_start_and_resume_require_fresh_gps_baselines(void) {
  Tracker tracker;
  tracker_init(&tracker);
  assert(tracker_accept_position(&tracker, position(0, 0, 0, 1000)) ==
         TRACKER_POSITION_BASELINE);
  assert(tracker_start(&tracker));
  assert(tracker_accept_position(&tracker, position(0, 1000, 0, 2000)) ==
         TRACKER_POSITION_BASELINE);
  assert(tracker_distance_mm(&tracker) == 0);
  assert(tracker_accept_position(&tracker, position(0, 2000, 0, 12000)) ==
         TRACKER_POSITION_COMMITTED);

  tracker_pause(&tracker);
  uint64_t distance_before_pause = tracker_distance_mm(&tracker);
  assert(tracker_resume(&tracker));
  uint32_t pace = 0;
  assert(!tracker_current_pace(&tracker, &pace));
  assert(tracker_accept_position(&tracker, position(0, 10000, 0, 60000)) ==
         TRACKER_POSITION_BASELINE);
  assert(tracker_distance_mm(&tracker) == distance_before_pause);
}

static void test_implausible_and_delayed_positions_rebase_without_distance(void) {
  Tracker tracker;
  tracker_init(&tracker);
  tracker_accept_position(&tracker, position(0, 0, 0, 1000));
  tracker_start(&tracker);
  tracker_accept_position(&tracker, position(0, 0, 0, 2000));

  assert(tracker_accept_position(&tracker, position(0, 1000000, 0, 3000)) ==
         TRACKER_POSITION_BASELINE);
  assert(tracker_distance_mm(&tracker) == 0);
  assert(tracker_accept_position(&tracker, position(0, 1001000, 0, 200000)) ==
         TRACKER_POSITION_BASELINE);
  assert(tracker_distance_mm(&tracker) == 0);
}

static void test_long_run_pace_and_elapsed_time_do_not_overflow(void) {
  Tracker tracker;
  tracker_init(&tracker);
  tracker_accept_position(&tracker, position(0, 0, 0, 0));
  tracker_start(&tracker);
  tracker_accept_position(&tracker, position(0, 0, 0, 1000));

  for (uint32_t i = 0; i < 9 * 60 * 60; i++) {
    tracker_tick(&tracker);
  }
  assert(tracker_elapsed_seconds(&tracker) == 9U * 60U * 60U);

  assert(tracker_accept_position(&tracker, position(0, 10000000, 0, 100000)) ==
         TRACKER_POSITION_BASELINE);
  assert(tracker_accept_position(&tracker, position(0, 10001000, 0, 110000)) ==
         TRACKER_POSITION_COMMITTED);

  uint32_t overall_pace = 0;
  assert(tracker_overall_pace(&tracker, &overall_pace));
  assert(overall_pace > 0);
}

static void test_current_pace_expires_without_movement(void) {
  Tracker tracker;
  tracker_init(&tracker);
  tracker_accept_position(&tracker, position(0, 0, 0, 0));
  tracker_start(&tracker);
  tracker_accept_position(&tracker, position(0, 0, 0, 1000));
  tracker_accept_position(&tracker, position(0, 1000, 0, 11000));

  uint32_t pace = 0;
  assert(tracker_current_pace(&tracker, &pace));
  for (uint32_t i = 0; i <= TRACKER_PACE_STALE_SECONDS; i++) {
    tracker_tick(&tracker);
  }
  assert(!tracker_current_pace(&tracker, &pace));
}

static void test_current_pace_rejects_accumulator_overflow(void) {
  Tracker tracker;
  tracker_init(&tracker);
  tracker.seconds_since_movement = 0;
  tracker.speed_buffer_position = 2;
  tracker.speed_buffer[0] = (TrackerSpeedSample) {
    .delta_ms = UINT32_MAX,
    .distance_mm = 100000,
    .valid = true,
  };
  tracker.speed_buffer[1] = (TrackerSpeedSample) {
    .delta_ms = 1,
    .distance_mm = 100000,
    .valid = true,
  };

  uint32_t pace = 0;
  assert(!tracker_current_pace(&tracker, &pace));
}

int main(void) {
  test_position_validation();
  test_distance_is_non_negative_and_wraps_antimeridian();
  test_start_and_resume_require_fresh_gps_baselines();
  test_implausible_and_delayed_positions_rebase_without_distance();
  test_long_run_pace_and_elapsed_time_do_not_overflow();
  test_current_pace_expires_without_movement();
  test_current_pace_rejects_accumulator_overflow();
  return 0;
}
