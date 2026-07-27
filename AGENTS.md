# PacePulse Agent Guide

## Project Overview

PacePulse is a Pebble watchapp for run and walk tracking. It receives GPS
positions from the connected phone and reads heart-rate samples from compatible
watch hardware. It does not store or upload activity history.

## Repository Structure

- `src/c/main.c`: Pebble lifecycle, services, buttons, AppMessage decoding, and
  coordination between domain modules and the dashboard.
- `src/c/tracker.c` and `tracker.h`: portable run state, GPS validation,
  distance, elapsed time, and pace calculations.
- `src/c/heart_rate.c` and `heart_rate.h`: portable BPM freshness and zone
  classification.
- `src/c/dashboard.c` and `dashboard.h`: all Pebble UI allocation, layout,
  rendering, updates, and cleanup.
- `src/pkjs/index.js`: phone geolocation and queued AppMessages.
- `tests/c/`: native C tests for portable domain logic.
- `tests/js/`: executable PebbleKit JavaScript tests.
- `tests/test_*.py`: host runners and Pebble integration contracts.

Keep these boundaries intact. Domain modules must remain independent of Pebble
APIs. Do not move business rules into `main.c` or rendering code.

## Platform Constraints

- Build with Pebble SDK 4 or later, currently verified with SDK 4.17.
- Keep `pebble.sdkVersion` set to `"3"` in `package.json` for tool compatibility.
- Preserve all targets: `aplite`, `basalt`, `chalk`, `diorite`, `emery`,
  `flint`, and `gabbro`.
- Preserve the `location` and `health` capabilities.
- Use C99 supported by the Pebble toolchain; avoid compiler-specific features.
- Treat RAM and layer allocations as constrained, especially on Aplite.
- Guard HealthService APIs where unsupported and clean up every subscription,
  callback, layer, and path created by the app.

## Behavioral Contracts

- GPS is supplied by the phone; the watch has no GPS receiver.
- Start and resume require a fresh GPS baseline. Movement while paused must not
  increase distance.
- Reject invalid coordinates, poor accuracy, delayed positions, and
  implausibly fast movement.
- Current pace becomes unavailable after 15 seconds without accepted movement.
- Heart-rate sampling runs once per second only while tracking is active.
- The last BPM remains visible while paused and becomes stale after 15 seconds
  without an update during an active run.
- Heart-rate zones are cumulative bars: below 132 BPM shows zero bars, 132-150
  shows one, 151-165 shows two, and 166 or above shows three.
- An unavailable or stale heart rate displays `-- BPM` with no active zone bar.

## UI Contracts

- Support rectangular and round displays without clipping.
- Keep elapsed time and distance in the summary row.
- Distance includes its lowercase unit, for example `3.42 km`.
- Do not restore separate `TIME` or `PACE` labels.
- Pace remains the primary centered metric.
- Heart-rate text must stay compact enough for three-digit BPM values and the
  three horizontal zone bars on 144-pixel-wide displays.
- `dashboard.c` owns all UI objects and must safely unwind partial allocation.

## Code Style

- Prefer the smallest correct change and follow existing naming patterns.
- Use `prv_` for file-local Pebble callback and helper functions.
- Keep ownership explicit and reset destroyed static pointers to `NULL`.
- Use fixed-width integer types for persisted measurements and arithmetic.
- Check overflow before narrowing or multiplying values.
- Add comments only where intent is not apparent from the code.
- Do not add dependencies unless the task clearly requires one.

## Testing Workflow

Use test-driven development for behavior changes: add a failing regression test,
confirm the expected failure, implement the minimal fix, then rerun all tests.

Run the full host suite:

```bash
python3 -m unittest discover -s tests -v
```

Validate PebbleKit JavaScript:

```bash
node --check src/pkjs/index.js
node tests/js/test_pkjs.js
```

Build every configured watch platform:

```bash
pebble clean
pebble build
```

Before reporting completion, also run `git diff --check`. Do not claim physical
heart-rate behavior was validated unless it was tested on compatible hardware.

## Documentation

- Keep `README.md` in English and user-focused.
- Use public watch names in compatibility documentation, not platform codenames.
- Keep upstream attribution and the existing MIT license notice.
- Do not add banners, screenshots, speculative plans, or internal planning docs
  unless explicitly requested.

## Git Safety

- `main` is published. Never amend, rebase, reset, or force-push its history.
- Make follow-up work as new commits only when the user explicitly requests a
  commit.
- Never revert unrelated user changes in a dirty worktree.
- Do not commit `build/`, Waf lock files, Python caches, or generated artifacts.
- Do not push, create a pull request, or change remotes unless explicitly asked.
