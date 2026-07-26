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

    def test_dashboard_places_pace_above_bpm_and_draws_heart(self):
        pace_frame = self.dashboard_source.index("s_text_pace = text_layer_create")
        heart_rate_frame = self.dashboard_source.index("s_text_heart_rate = text_layer_create")
        self.assertLess(pace_frame, heart_rate_frame)
        self.assertIn("graphics_fill_circle", self.dashboard_source)
        self.assertIn('"-- BPM"', self.dashboard_source)
        self.assertIn("heart_rate_center_y", self.dashboard_source)

    def test_pace_buffer_fits_the_largest_uint32_minute_value(self):
        self.assertIn("static char formatted_pace[16];", self.dashboard_source)

    def test_all_layers_are_destroyed(self):
        for layer in (
            "s_text_status",
            "s_text_time",
            "s_text_time_unit",
            "s_text_distance",
            "s_text_pace",
            "s_text_heart_rate",
            "s_text_distance_unit",
            "s_text_pace_unit",
            "s_heart_layer",
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
