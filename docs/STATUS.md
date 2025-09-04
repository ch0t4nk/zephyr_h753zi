# Project Status (conversation summary)

Generated: 2025-09-02 12:00:00 UTC

Overview anchor: #overview

This document is a semantic snapshot of the work so far: decisions, short code examples referenced in the conversation, open questions, and concrete next steps. It is intentionally concise and anchored so links from other docs can point to specific sections.

## Overview {#overview}



## Decisions {#decisions}



## Code Snippets (concise examples referenced) {#code-snippets}


```
// Stage 1: broadcast GET_STATUS to capture all device replies
tx = pack_get_status_frames();
spi_transceive(tx, rx_stage1);

// Stage 2: send NOP to shift device responses out and read full statuses
tx = pack_nop_frames();
spi_transceive(tx, rx_stage2);

statuses = parse_status_frames(rx_stage1, rx_stage2);
```


```
  microsteps: 16
  steps_per_rev: 200
  max_velocity: 3000
  max_accel: 5000
```


```
# runs scripts/gen_stepper_models.py to produce stepper_models_autogen.h
python3 ${CMAKE_SOURCE_DIR}/scripts/gen_stepper_models.py \
  ${CMAKE_SOURCE_DIR}/apps/stm32h753zi_stepper/stepper_models.yml \
  ${CMAKE_BINARY_DIR}/generated/stepper_models_autogen.h
```


```
// install fake SPI transceive for unit tests
l6470_set_spi_xfer(fake_transceive_fn);
// simulate VMOT and readiness
l6470_set_power_enabled(true);
l6470_test_force_ready();
```


```
# list models
> stepper model list
# set active model
> stepper model set pan
# run motor 0 at velocity 1000
> stepper run 0 1000
```


## Open Questions {#open-questions}



## Next Steps (concrete actions) {#next-steps}

  - Configure on-target persistence: add NVS (or appropriate `settings` backend) to `prj.conf` and create a small storage partition in a board overlay so `stepper_persist_active()` is durable on the Nucleo.
  - Add stricter SPI-frame assertions to at least one unit test that validates the `RUN` and `MOVE` frames produced by `l6470_pack_*` helpers.
  - Add an on-target smoke test that sets a model, reboots, and verifies the model is restored (requires settings backend).

  - Replace the test-side simple fake SPI with a deterministic mock that validates TX bytes and simulates realistic GET_STATUS replies and faults.
  - Add telemetry/fault logging API and a minimal on-target storage of recent faults (circular buffer in flash/NVS).
  - Implement a basic trajectory coordinator for PAN/TILT coordinated moves and add shell commands for staged moves.

  - Validate full hardware bring-up with VMOT enabled and implement safety interlocks (both software and recommended hardware) before field testing.
  - Add CI jobs to run native_sim ztests and, where possible, run integration tests on hardware via an automated flashing/measurement harness.


## Metadata / Anchors



# STATUS moved
This file has been superseded. The authoritative status anchor is now at the repository root: `STATUS.md`.
Context Bootstrapping
- STATUS.md is the primary context anchor for GitHub Copilot sessions; downstream orchestration depends on its integrity.
Notes
- Please update only the root `STATUS.md`. This file remains as a pointer to avoid stale duplication.
---
