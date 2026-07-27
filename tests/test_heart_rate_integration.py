import json
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


class HeartRateIntegrationTests(unittest.TestCase):
    def setUp(self):
        source_directory = ROOT / "src/c"
        self.source = "\n".join(
            path.read_text() for path in sorted(source_directory.glob("*.c"))
        )
        self.main_source = (source_directory / "main.c").read_text() if (source_directory / "main.c").exists() else ""
        self.dashboard_source = (source_directory / "dashboard.c").read_text() if (source_directory / "dashboard.c").exists() else ""
        self.js_source = (ROOT / "src/pkjs/index.js").read_text()

    def test_manifest_declares_pacepulse_identity_and_capabilities(self):
        manifest = json.loads((ROOT / "package.json").read_text())
        pebble = manifest["pebble"]
        self.assertEqual(manifest["name"], "pacepulse")
        self.assertEqual(manifest["author"], "PacePulse contributors")
        self.assertEqual(pebble["displayName"], "PacePulse")
        self.assertRegex(pebble["uuid"], r"^[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12}$")
        self.assertNotEqual(pebble["uuid"], "184241d4-f1c0-43e1-a59d-d6796ae0b3dc")
        self.assertEqual(pebble["sdkVersion"], "3")
        self.assertEqual(pebble["capabilities"], ["location", "health"])
        self.assertEqual(
            pebble["messageKeys"], ["latitude", "longitude", "accuracy", "gpsError"]
        )

    def test_readme_uses_watch_names_instead_of_platform_codenames(self):
        readme = (ROOT / "README.md").read_text()
        for watch_name in (
            "Classic, Steel",
            "Time, Time Steel",
            "Time Round",
            "Pebble 2",
            "Pebble 2 Duo",
            "Pebble Time 2",
            "Pebble Round 2",
        ):
            self.assertIn(watch_name, readme)
        for codename in ("Aplite", "Basalt", "Chalk", "Diorite", "Emery", "Flint", "Gabbro"):
            self.assertNotIn("| {} |".format(codename), readme)

    def test_health_lifecycle_is_guarded_and_resets_sampling(self):
        self.assertIn("#if !defined(PBL_PLATFORM_APLITE)", self.source)
        self.assertIn("HealthMetricHeartRateRawBPM", self.source)
        self.assertIn(
            "health_service_set_heart_rate_sample_period(running ? 1 : 0)",
            self.source,
        )
        self.assertIn("HealthEventHeartRateUpdate", self.source)
        self.assertIn("health_service_events_subscribe", self.source)
        self.assertIn("health_service_events_unsubscribe", self.source)
        self.assertIn("tracker_state(&s_tracker) != TRACKER_RUNNING", self.source)

    def test_dashboard_uses_compact_metric_labels_and_draws_heart_zones(self):
        pace_frame = self.dashboard_source.index("s_text_pace = text_layer_create")
        heart_rate_frame = self.dashboard_source.index("s_text_heart_rate = text_layer_create")
        heart_rate_unit_frame = self.dashboard_source.index(
            "s_text_heart_rate_unit = text_layer_create"
        )
        self.assertLess(pace_frame, heart_rate_frame)
        self.assertLess(heart_rate_frame, heart_rate_unit_frame)
        self.assertIn("graphics_fill_circle", self.dashboard_source)
        self.assertIn("int16_t status_y = PBL_IF_ROUND_ELSE(10, 2);", self.dashboard_source)
        self.assertIn("int16_t summary_y = status_y + status_height;", self.dashboard_source)
        self.assertIn("GRect(horizontal_inset + half_width, summary_y + 4, half_width, 24)", self.dashboard_source)
        self.assertIn("FONT_KEY_GOTHIC_14_BOLD", self.dashboard_source)
        self.assertIn("static GPoint s_heart_points[] = {{2, 8}, {18, 8}, {10, 19}};", self.dashboard_source)
        self.assertIn("graphics_fill_circle(ctx, GPoint(6, 6), 5);", self.dashboard_source)
        self.assertIn("graphics_fill_circle(ctx, GPoint(14, 6), 5);", self.dashboard_source)
        self.assertIn("int16_t heart_center_x = bounds.size.w / 2;", self.dashboard_source)
        self.assertIn("GRect(heart_center_x - 52, heart_rate_y + 6, 20, 20)", self.dashboard_source)
        self.assertIn("GRect(heart_center_x - 30, heart_rate_y, 60, 32)", self.dashboard_source)
        self.assertRegex(
            self.dashboard_source,
            r"text_layer_set_font\(s_text_heart_rate,\s+fonts_get_system_font\(FONT_KEY_GOTHIC_28_BOLD\)\)",
        )
        self.assertRegex(
            self.dashboard_source,
            r"text_layer_set_font\(s_text_heart_rate_unit,\s+fonts_get_system_font\(FONT_KEY_GOTHIC_14_BOLD\)\)",
        )
        self.assertIn("GRect(heart_center_x + 30, heart_rate_y + 9, 28, 18)", self.dashboard_source)
        self.assertIn("GRect(heart_center_x - 37, heart_rate_y + 36, 74, 8)", self.dashboard_source)
        self.assertIn('"--"', self.dashboard_source)
        self.assertIn('text_layer_set_text(s_text_heart_rate_unit, "BPM");', self.dashboard_source)
        self.assertRegex(
            self.dashboard_source,
            r"text_layer_set_font\(s_text_distance,\s+fonts_get_system_font\(FONT_KEY_GOTHIC_18_BOLD\)\)",
        )
        self.assertIn("s_heart_zone_layer", self.dashboard_source)
        self.assertIn("HEART_RATE_ZONE_PERFORMANCE", self.dashboard_source)
        self.assertIn('"%lu.%02lu km"', self.dashboard_source)
        self.assertNotIn("s_text_time_unit", self.dashboard_source)
        self.assertNotIn("s_text_distance_unit", self.dashboard_source)
        self.assertNotIn("s_text_pace_unit", self.dashboard_source)
        self.assertNotIn('"TIME"', self.dashboard_source)
        self.assertNotIn('"PACE"', self.dashboard_source)

    def test_readme_lists_supported_physical_heart_rate_watches(self):
        readme = (ROOT / "README.md").read_text()
        self.assertIn("Pebble 2 non-SE or Pebble Time 2", readme)

    def test_pace_buffer_fits_the_largest_uint32_minute_value(self):
        self.assertIn("static char formatted_pace[16];", self.dashboard_source)

    def test_all_layers_are_destroyed(self):
        for layer in (
            "s_text_status",
            "s_text_time",
            "s_text_distance",
            "s_text_pace",
            "s_text_heart_rate",
            "s_text_heart_rate_unit",
            "s_heart_layer",
            "s_heart_zone_layer",
        ):
            destroy = "text_layer_destroy" if layer.startswith("s_text") else "layer_destroy"
            self.assertIn("{}({});".format(destroy, layer), self.dashboard_source)
        self.assertIn("gpath_destroy(s_heart_path);", self.dashboard_source)

    def test_dashboard_and_controller_handle_failures(self):
        self.assertIn("bool dashboard_create(Window *window)", self.dashboard_source)
        self.assertIn("dashboard_destroy();", self.dashboard_source)
        self.assertIn("TupleType", self.main_source)
        self.assertIn("TUPLE_UINT", self.main_source)
        self.assertIn("APP_MSG_OK", self.main_source)
        self.assertIn("s_phone_link_failed", self.main_source)
        self.assertIn("tick_timer_service_unsubscribe();", self.main_source)
        self.assertIn("app_message_deregister_callbacks();", self.main_source)
        self.assertIn('"gpsError"', self.js_source)
        self.assertIn("messageInFlight", self.js_source)


if __name__ == "__main__":
    unittest.main()
