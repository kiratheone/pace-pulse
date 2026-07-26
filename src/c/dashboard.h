#pragma once

#include <pebble.h>

#include <stdbool.h>
#include <stdint.h>

bool dashboard_create(Window *window);
void dashboard_destroy(void);
void dashboard_set_status(const char *status);
void dashboard_update_wall_time(const struct tm *time_value);
void dashboard_update_elapsed(uint32_t elapsed_seconds);
void dashboard_update_distance(uint64_t distance_mm);
void dashboard_update_pace(bool available, uint32_t pace_seconds_per_km);
void dashboard_update_heart_rate(bool available, int32_t bpm);
