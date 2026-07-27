#include <pebble.h>

#include <limits.h>

#include "dashboard.h"
#include "heart_rate.h"
#include "tracker.h"

static Window *s_window;
static Tracker s_tracker;
static HeartRateState s_heart_rate;
static int16_t s_status_seconds;
static bool s_dashboard_ready;
static bool s_tick_subscribed;
static bool s_app_message_registered;
static bool s_phone_link_failed;
#if !defined(PBL_PLATFORM_APLITE)
static bool s_health_subscribed;
#endif

static uint32_t prv_current_time_ms(void) {
  time_t seconds;
  uint16_t milliseconds;
  time_ms(&seconds, &milliseconds);
  return (uint32_t)((uint64_t)(uint32_t)seconds * 1000ULL + milliseconds);
}

static void prv_show_temporary_status(const char *status) {
  dashboard_set_status(status);
  s_status_seconds = 4;
}

static void prv_update_pace(void) {
  uint32_t pace = 0;
  bool available = false;
  if (tracker_state(&s_tracker) == TRACKER_RUNNING) {
    available = tracker_current_pace(&s_tracker, &pace);
  } else if (tracker_state(&s_tracker) == TRACKER_PAUSED) {
    available = tracker_overall_pace(&s_tracker, &pace);
  }
  dashboard_update_pace(available, pace);
}

static void prv_update_heart_rate(void) {
  int32_t bpm = 0;
  bool available = heart_rate_current(&s_heart_rate, &bpm);
  HeartRateZone zone = available ? heart_rate_zone(bpm) : HEART_RATE_ZONE_NONE;
  dashboard_update_heart_rate(available, bpm, zone);
}

static void prv_set_heart_rate_sampling(bool running) {
#if !defined(PBL_PLATFORM_APLITE)
  if (!s_health_subscribed) {
    return;
  }
  health_service_set_heart_rate_sample_period(running ? 1 : 0);
#endif
}

#if !defined(PBL_PLATFORM_APLITE)
static void prv_health_event_handler(HealthEventType event, void *context) {
  if (event != HealthEventHeartRateUpdate ||
      tracker_state(&s_tracker) != TRACKER_RUNNING) {
    return;
  }
  HealthValue bpm =
      health_service_peek_current_value(HealthMetricHeartRateRawBPM);
  heart_rate_update(&s_heart_rate, bpm);
  prv_update_heart_rate();
}
#endif

static bool prv_tuple_to_int32(const Tuple *tuple, int32_t *value) {
  if (tuple == NULL || value == NULL) {
    return false;
  }
  TupleType type = tuple->type;
  if (type == TUPLE_INT) {
    switch (tuple->length) {
      case sizeof(int8_t):
        *value = tuple->value->int8;
        return true;
      case sizeof(int16_t):
        *value = tuple->value->int16;
        return true;
      case sizeof(int32_t):
        *value = tuple->value->int32;
        return true;
    }
  } else if (type == TUPLE_UINT) {
    uint32_t unsigned_value;
    switch (tuple->length) {
      case sizeof(uint8_t):
        unsigned_value = tuple->value->uint8;
        break;
      case sizeof(uint16_t):
        unsigned_value = tuple->value->uint16;
        break;
      case sizeof(uint32_t):
        unsigned_value = tuple->value->uint32;
        break;
      default:
        return false;
    }
    if (unsigned_value <= INT32_MAX) {
      *value = (int32_t)unsigned_value;
      return true;
    }
  }
  return false;
}

static void prv_inbox_received_handler(DictionaryIterator *iterator,
                                       void *context) {
  Tuple *gps_error = dict_find(iterator, MESSAGE_KEY_gpsError);
  if (gps_error != NULL) {
    prv_show_temporary_status("GPS unavailable");
    return;
  }

  Tuple *latitude = dict_find(iterator, MESSAGE_KEY_latitude);
  Tuple *longitude = dict_find(iterator, MESSAGE_KEY_longitude);
  Tuple *accuracy = dict_find(iterator, MESSAGE_KEY_accuracy);
  int32_t latitude_e6;
  int32_t longitude_e6;
  int32_t accuracy_dm;
  if (!prv_tuple_to_int32(latitude, &latitude_e6) ||
      !prv_tuple_to_int32(longitude, &longitude_e6) ||
      !prv_tuple_to_int32(accuracy, &accuracy_dm)) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Invalid GPS message");
    return;
  }

  TrackerPosition position = {
    .latitude_e6 = latitude_e6,
    .longitude_e6 = longitude_e6,
    .accuracy_dm = accuracy_dm,
    .timestamp_ms = prv_current_time_ms(),
  };
  TrackerState previous_state = tracker_state(&s_tracker);
  TrackerPositionResult result =
      tracker_accept_position(&s_tracker, position);
  if (result == TRACKER_POSITION_REJECTED) {
    return;
  }
  if (previous_state == TRACKER_WAITING_FOR_GPS &&
      tracker_state(&s_tracker) == TRACKER_READY) {
    prv_show_temporary_status("GPS ready");
  }
  if (result == TRACKER_POSITION_COMMITTED) {
    dashboard_update_distance(tracker_distance_mm(&s_tracker));
    prv_update_pace();
  }
}

static void prv_select_click_handler(ClickRecognizerRef recognizer,
                                     void *context) {
  switch (tracker_state(&s_tracker)) {
    case TRACKER_WAITING_FOR_GPS:
      return;
    case TRACKER_READY:
      if (tracker_start(&s_tracker)) {
        dashboard_update_elapsed(0);
        dashboard_update_distance(0);
        dashboard_update_pace(false, 0);
        prv_show_temporary_status("Run started");
        prv_set_heart_rate_sampling(true);
      }
      return;
    case TRACKER_RUNNING:
      tracker_pause(&s_tracker);
      dashboard_update_elapsed(tracker_elapsed_seconds(&s_tracker));
      prv_update_pace();
      prv_show_temporary_status("Run paused");
      prv_set_heart_rate_sampling(false);
      return;
    case TRACKER_PAUSED:
      if (tracker_resume(&s_tracker)) {
        prv_update_pace();
        prv_show_temporary_status("Run resumed");
        prv_set_heart_rate_sampling(true);
      }
      return;
  }
}

static void prv_click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_select_click_handler);
}

static void prv_tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  bool running = tracker_state(&s_tracker) == TRACKER_RUNNING;
  tracker_tick(&s_tracker);
  heart_rate_tick(&s_heart_rate, running);

  if (s_phone_link_failed) {
    dashboard_set_status("Phone link failed");
  } else if (s_status_seconds > 0) {
    s_status_seconds--;
  } else if (tracker_state(&s_tracker) == TRACKER_WAITING_FOR_GPS) {
    dashboard_set_status("Waiting for GPS");
  } else {
    dashboard_update_wall_time(tick_time);
  }

  dashboard_update_elapsed(tracker_elapsed_seconds(&s_tracker));
  dashboard_update_distance(tracker_distance_mm(&s_tracker));
  prv_update_pace();
  prv_update_heart_rate();
}

static void prv_window_load(Window *window) {
  s_dashboard_ready = dashboard_create(window);
  if (!s_dashboard_ready) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Dashboard allocation failed");
  }
}

static void prv_window_unload(Window *window) {
  dashboard_destroy();
  s_dashboard_ready = false;
}

static bool prv_init(void) {
  tracker_init(&s_tracker);
  heart_rate_init(&s_heart_rate);

  s_window = window_create();
  if (s_window == NULL) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "Window allocation failed");
    return false;
  }
  window_set_background_color(s_window, GColorBlack);
  window_set_click_config_provider(s_window, prv_click_config_provider);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = prv_window_load,
    .unload = prv_window_unload,
  });
  window_stack_push(s_window, true);
  if (!s_dashboard_ready) {
    return false;
  }

  app_message_register_inbox_received(prv_inbox_received_handler);
  s_app_message_registered = true;
  AppMessageResult app_message_result = app_message_open(128, 128);
  if (app_message_result != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "AppMessage open failed: %d",
            app_message_result);
    s_phone_link_failed = true;
    dashboard_set_status("Phone link failed");
  }

  tick_timer_service_subscribe(SECOND_UNIT, prv_tick_handler);
  s_tick_subscribed = true;
#if !defined(PBL_PLATFORM_APLITE)
  s_health_subscribed =
      health_service_events_subscribe(prv_health_event_handler, NULL);
  if (!s_health_subscribed) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "HealthService unavailable");
  }
#endif
  return true;
}

static void prv_deinit(void) {
#if !defined(PBL_PLATFORM_APLITE)
  prv_set_heart_rate_sampling(false);
  if (s_health_subscribed) {
    health_service_events_unsubscribe();
    s_health_subscribed = false;
  }
#endif
  if (s_tick_subscribed) {
    tick_timer_service_unsubscribe();
    s_tick_subscribed = false;
  }
  if (s_app_message_registered) {
    app_message_deregister_callbacks();
    s_app_message_registered = false;
  }
  if (s_window != NULL) {
    window_destroy(s_window);
    s_window = NULL;
  }
}

int main(void) {
  if (prv_init()) {
    app_event_loop();
  }
  prv_deinit();
}
