#include "dashboard.h"

#include <limits.h>
#include <stdio.h>

static TextLayer *s_text_status;
static TextLayer *s_text_time;
static TextLayer *s_text_distance;
static TextLayer *s_text_pace;
static TextLayer *s_text_heart_rate;
static Layer *s_heart_layer;
static Layer *s_heart_zone_layer;
static GPath *s_heart_path;
static HeartRateZone s_heart_rate_zone;

static GPoint s_heart_points[] = {{2, 9}, {22, 9}, {12, 23}};
static const GPathInfo s_heart_path_info = {
  .num_points = 3,
  .points = s_heart_points,
};

static void prv_format_text_layer(TextLayer *layer) {
  text_layer_set_text_alignment(layer, GTextAlignmentRight);
  text_layer_set_text_color(layer, GColorWhite);
  text_layer_set_background_color(layer, GColorClear);
}

static GFont prv_primary_metric_font(void) {
  return fonts_get_system_font(
    PBL_PLATFORM_SWITCH(
      PBL_PLATFORM_TYPE_CURRENT,
      /* default */ FONT_KEY_BITHAM_42_BOLD,
      /* basalt  */ FONT_KEY_BITHAM_42_BOLD,
      /* chalk   */ FONT_KEY_BITHAM_42_BOLD,
      /* diorite */ FONT_KEY_BITHAM_42_BOLD,
      /* emery   */ FONT_KEY_ROBOTO_BOLD_SUBSET_49,
      /* flint   */ FONT_KEY_BITHAM_42_BOLD,
      /* gabbro  */ FONT_KEY_ROBOTO_BOLD_SUBSET_49
    )
  );
}

static void prv_heart_layer_update_proc(Layer *layer, GContext *ctx) {
  if (s_heart_path == NULL) {
    return;
  }
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_fill_circle(ctx, GPoint(7, 7), 7);
  graphics_fill_circle(ctx, GPoint(17, 7), 7);
  gpath_draw_filled(ctx, s_heart_path);
}

static void prv_heart_zone_layer_update_proc(Layer *layer, GContext *ctx) {
  const int16_t bar_width = 20;
  const int16_t bar_gap = 7;
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);

  for (int16_t index = 0; index < HEART_RATE_ZONE_PERFORMANCE; index++) {
    GRect bar = GRect(index * (bar_width + bar_gap), 1, bar_width, 6);
    if (index < s_heart_rate_zone) {
      graphics_fill_rect(ctx, bar, 0, GCornerNone);
    } else {
      graphics_draw_rect(ctx, bar);
    }
  }
}

bool dashboard_create(Window *window) {
  if (window == NULL) {
    return false;
  }

  Layer *window_layer = window_get_root_layer(window);
  if (window_layer == NULL) {
    return false;
  }
  GRect bounds = layer_get_bounds(window_layer);
  int16_t horizontal_inset = PBL_IF_ROUND_ELSE(12, 4);
  int16_t status_y = PBL_IF_ROUND_ELSE(10, 0);
  int16_t status_height = 18;
  int16_t summary_y = status_y + status_height;
  int16_t summary_height = 28;
  int16_t metric_y = summary_y + summary_height;
  int16_t metric_height =
      (bounds.size.h - metric_y - PBL_IF_ROUND_ELSE(16, 2)) / 2;
  int16_t content_width = bounds.size.w - horizontal_inset * 2;
  int16_t half_width = content_width / 2;
  int16_t heart_rate_y = metric_y + metric_height;
  int16_t heart_group_x = (bounds.size.w - 122) / 2;

  s_text_status = text_layer_create(
      GRect(horizontal_inset, status_y, content_width, status_height));
  if (s_text_status == NULL) {
    goto fail;
  }
  prv_format_text_layer(s_text_status);
  text_layer_set_font(s_text_status,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(s_text_status, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_text_status));

  s_text_time = text_layer_create(
      GRect(horizontal_inset, summary_y, half_width, summary_height));
  if (s_text_time == NULL) {
    goto fail;
  }
  prv_format_text_layer(s_text_time);
  text_layer_set_font(s_text_time,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_text_time, GTextAlignmentLeft);
  layer_add_child(window_layer, text_layer_get_layer(s_text_time));

  s_text_distance = text_layer_create(
      GRect(horizontal_inset + half_width, summary_y, half_width,
            summary_height));
  if (s_text_distance == NULL) {
    goto fail;
  }
  prv_format_text_layer(s_text_distance);
  text_layer_set_font(s_text_distance,
                      fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(s_text_distance));

  s_text_pace = text_layer_create(
      GRect(horizontal_inset, metric_y, content_width, metric_height));
  if (s_text_pace == NULL) {
    goto fail;
  }
  prv_format_text_layer(s_text_pace);
  text_layer_set_font(s_text_pace, prv_primary_metric_font());
  text_layer_set_text_alignment(s_text_pace, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_text_pace));

  s_heart_layer = layer_create(
      GRect(heart_group_x, heart_rate_y + 2, 24, 24));
  if (s_heart_layer == NULL) {
    goto fail;
  }
  s_heart_path = gpath_create(&s_heart_path_info);
  if (s_heart_path == NULL) {
    goto fail;
  }
  layer_set_update_proc(s_heart_layer, prv_heart_layer_update_proc);
  layer_add_child(window_layer, s_heart_layer);

  s_text_heart_rate = text_layer_create(
      GRect(heart_group_x + 28, heart_rate_y, 94, 30));
  if (s_text_heart_rate == NULL) {
    goto fail;
  }
  prv_format_text_layer(s_text_heart_rate);
  text_layer_set_font(s_text_heart_rate,
                      fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_text_heart_rate, GTextAlignmentCenter);
  layer_add_child(window_layer, text_layer_get_layer(s_text_heart_rate));

  s_heart_zone_layer = layer_create(
      GRect((bounds.size.w - 74) / 2, heart_rate_y + 32, 74, 8));
  if (s_heart_zone_layer == NULL) {
    goto fail;
  }
  layer_set_update_proc(s_heart_zone_layer,
                        prv_heart_zone_layer_update_proc);
  layer_add_child(window_layer, s_heart_zone_layer);

  dashboard_set_status("Waiting for GPS");
  dashboard_update_elapsed(0);
  dashboard_update_distance(0);
  dashboard_update_pace(false, 0);
  dashboard_update_heart_rate(false, 0, HEART_RATE_ZONE_NONE);
  return true;

fail:
  dashboard_destroy();
  return false;
}

void dashboard_destroy(void) {
  if (s_text_status != NULL) {
    text_layer_destroy(s_text_status);
    s_text_status = NULL;
  }
  if (s_text_time != NULL) {
    text_layer_destroy(s_text_time);
    s_text_time = NULL;
  }
  if (s_text_distance != NULL) {
    text_layer_destroy(s_text_distance);
    s_text_distance = NULL;
  }
  if (s_text_pace != NULL) {
    text_layer_destroy(s_text_pace);
    s_text_pace = NULL;
  }
  if (s_text_heart_rate != NULL) {
    text_layer_destroy(s_text_heart_rate);
    s_text_heart_rate = NULL;
  }
  if (s_heart_layer != NULL) {
    layer_destroy(s_heart_layer);
    s_heart_layer = NULL;
  }
  if (s_heart_zone_layer != NULL) {
    layer_destroy(s_heart_zone_layer);
    s_heart_zone_layer = NULL;
  }
  if (s_heart_path != NULL) {
    gpath_destroy(s_heart_path);
    s_heart_path = NULL;
  }
}

void dashboard_set_status(const char *status) {
  if (s_text_status != NULL) {
    text_layer_set_text(s_text_status, status);
  }
}

void dashboard_update_wall_time(const struct tm *time_value) {
  static char formatted_wall_time[10];
  if (time_value == NULL) {
    return;
  }
  snprintf(formatted_wall_time, sizeof(formatted_wall_time), "%d:%02d:%02d",
           time_value->tm_hour, time_value->tm_min, time_value->tm_sec);
  dashboard_set_status(formatted_wall_time);
}

void dashboard_update_elapsed(uint32_t elapsed_seconds) {
  static char formatted_time[16];
  snprintf(formatted_time, sizeof(formatted_time), "%lu:%02lu",
           (unsigned long)(elapsed_seconds / 60),
           (unsigned long)(elapsed_seconds % 60));
  if (s_text_time != NULL) {
    text_layer_set_text(s_text_time, formatted_time);
  }
}

void dashboard_update_distance(uint64_t distance_mm) {
  static char formatted_distance[20];
  uint64_t kilometers = distance_mm / 1000000ULL;
  if (kilometers > ULONG_MAX) {
    kilometers = ULONG_MAX;
  }
  snprintf(formatted_distance, sizeof(formatted_distance), "%lu.%02lu km",
           (unsigned long)kilometers,
           (unsigned long)((distance_mm / 10000ULL) % 100ULL));
  if (s_text_distance != NULL) {
    text_layer_set_text(s_text_distance, formatted_distance);
  }
}

void dashboard_update_pace(bool available, uint32_t pace_seconds_per_km) {
  static char formatted_pace[16];
  if (available) {
    snprintf(formatted_pace, sizeof(formatted_pace), "%lu:%02lu",
             (unsigned long)(pace_seconds_per_km / 60),
             (unsigned long)(pace_seconds_per_km % 60));
  } else {
    snprintf(formatted_pace, sizeof(formatted_pace), "--:--");
  }
  if (s_text_pace != NULL) {
    text_layer_set_text(s_text_pace, formatted_pace);
  }
}

void dashboard_update_heart_rate(bool available, int32_t bpm,
                                 HeartRateZone zone) {
  static char formatted_heart_rate[16];
  if (available) {
    snprintf(formatted_heart_rate, sizeof(formatted_heart_rate), "%ld BPM",
             (long)bpm);
  } else {
    snprintf(formatted_heart_rate, sizeof(formatted_heart_rate), "-- BPM");
  }
  if (s_text_heart_rate != NULL) {
    text_layer_set_text(s_text_heart_rate, formatted_heart_rate);
  }
  s_heart_rate_zone = available ? zone : HEART_RATE_ZONE_NONE;
  if (s_heart_zone_layer != NULL) {
    layer_mark_dirty(s_heart_zone_layer);
  }
}
