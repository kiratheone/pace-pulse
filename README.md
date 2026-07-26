# PacePulse

PacePulse is a simple Pebble run and walk tracker with phone GPS pace and live
heart rate on compatible watches.

## Overview

PacePulse keeps essential training data visible without a companion app UI. The
watch gets location from the connected phone and reads heart rate directly from
the watch sensor when available. Activity history is not stored or uploaded.

## Features

- Wall clock, elapsed time, and distance in kilometers.
- Current pace while running and average pace while paused.
- A fresh GPS baseline after start and resume, so paused movement is excluded.
- Invalid, delayed, or implausibly fast GPS updates are ignored safely.
- Current pace changes to `--:--` after 15 seconds without accepted movement.
- Live raw heart rate below the pace reading.
- `-- BPM` when heart rate is unsupported, unavailable, invalid, or stale for
  more than 15 seconds during a run.
- One-second heart-rate sampling only while a run is active.
- Start, pause, and resume from the Select button.
- Black-and-white layout for rectangular and round Pebble displays.

## Compatibility

Run tracking requires a connected phone for GPS on every platform. Heart rate
requires watch hardware with a compatible sensor and the Health permission.

| Platform | Run tracking | Heart rate |
| --- | --- | --- |
| Aplite | Yes | No |
| Basalt | Yes | Hardware dependent |
| Chalk | Yes | No |
| Diorite | Yes | Pebble 2 non-SE only |
| Emery | Yes | Hardware dependent |
| Flint | Yes | Hardware dependent |
| Gabbro | Yes | Hardware dependent |

## Controls

| Button | Action |
| --- | --- |
| Select | Start, pause, or resume a run |
| Back | Exit PacePulse |

## Permissions

- **Location** obtains GPS positions from the connected phone.
- **Health** reads live heart-rate data from supported watches.

PacePulse does not store or upload activity, location, or heart-rate history.

## Known Limitations

- Distance is available in kilometers only; miles are not yet supported.
- GPS accuracy depends on the phone and its current location lock.
- The emulator cannot produce live heart-rate sensor readings.
- A physical Pebble 2 non-SE is required to validate live BPM updates.

## GPS Accuracy

Pebble watches do not have their own GPS receiver. PacePulse uses the phone's
current position, but the watch cannot force a precise GPS lock. For best
results, run a phone activity app such as Strava or Google Fit alongside
PacePulse so the phone requests precise location data.

## Development

Install Node.js, libpng, the Pebble command-line tool, and a current Pebble
SDK on macOS:

```bash
brew install node libpng
uv tool install pebble-tool
pebble sdk install latest
```

PacePulse builds with Pebble SDK 4 or later, while its app metadata must retain
`sdkVersion: "3"` for toolchain compatibility.

Build all configured platforms:

```bash
pebble clean
pebble build
```

Run the Pebble 2 emulator:

```bash
pebble install --emulator diorite
pebble logs --emulator diorite
```

For a physical Pebble 2 non-SE, enable Dev Connect in the Pebble mobile app,
run `pebble login`, then install with `pebble install --cloudpebble`. Grant
both Location and Health permissions.

### Project Structure

```text
src/
+-- c/
|   +-- main.c          Pebble lifecycle and service wiring
|   +-- tracker.c/.h    GPS validation, run state, distance, and pace
|   +-- heart_rate.c/.h BPM availability and freshness
|   +-- dashboard.c/.h  Layer ownership, layout, and rendering
+-- pkjs/
    +-- index.js        Phone geolocation and queued AppMessages
tests/
+-- c/                  Executable tests for portable C domain logic
+-- js/                 Executable PebbleKit JS queue tests
+-- test_*.py           Host-test runners and Pebble integration contracts
```

The tracker and heart-rate modules do not depend on Pebble APIs, so their
arithmetic and state transitions can run as native C tests. `main.c` is kept
small and only translates Pebble events into domain operations and dashboard
updates. `dashboard.c` owns every UI allocation and can safely unwind partial
initialization on constrained devices.

## Credits

PacePulse is based on and inspired by
[Marian Kleineberg's Pebble Run Tracker](https://github.com/marian42/pebble-run-tracker).
The original project is available under the MIT License; its copyright and
license notice are retained in `LICENSE.md`.

## License

PacePulse is available under the [MIT License](LICENSE.md).
