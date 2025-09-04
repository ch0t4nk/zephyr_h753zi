# Project STATUS (primary context anchor)

Generated: 2025-09-04 00:00 UTC
Owner: Joey W.  Repo: $HOME/zephyr-dev

Context Bootstrapping
- STATUS.md is the primary context anchor for GitHub Copilot sessions; downstream orchestration depends on its integrity.

## Overview
- Zephyr app: apps/stm32h753zi_stepper for Nucleo-H753ZI + X-NUCLEO-IHM02A1 (two L6470 drivers, daisy-chained SPI).
- Dev workflow: native host tests (native_sim via ztest/Twister) + on-target build/flash (OpenOCD).
- SSOT for stepper models: stepper_models.yml → scripts/gen_stepper_models.py → generated/stepper_models_autogen.h → src/stepper_models.c.

## Current status
- native_sim: builds clean (prior warnings addressed). Twister installed and wired; ctest intentionally finds none (ztest-only suites).
- nucleo_h753zi: board build passes.
- Smoke shell: vmot on|off|toggle|status (DT-gated GPIO; prints -ENOTSUP if node missing).

## Key changes
- Persistence: LittleFS at /lfs with sentinel file /.mounted (src/persist.c). Formats storage on first mount failure when enabled (CONFIG_APP_LINK_WITH_FS=y and CONFIG_FILE_SYSTEM_MKFS=y).
- Overlay: Shield wiring inlined in nucleo_h753zi.overlay (SPI1 and control GPIOs), avoiding separate shield DTS.
- L6470: Reset GPIO fallback to PE14 when overlay node absent; test hooks for SPI/power.
- Tests: Twister discovery enabled via apps/stm32h753zi_stepper/tests/unit/testcase.yaml; native_sim settings warning fixed with CONFIG_SETTINGS_NONE=y; weak-symbol guard removes strict mock address warning.
- Tooling: VS Code task added to run Twister; settings/tasks JSON cleaned.

## Decisions
- Daisy-chain SPI and fixed 4-byte frames per device for deterministic packing.
- Two-phase GET_STATUS (broadcast + NOP shift-out) to read both devices reliably.
- Prefer Zephyr-native testing (Twister + ztest + native_sim) instead of external simulators unless ST-specific electrical modeling is required.

## Tasks (short-term)
- [ ] On-target smoke: flash and exercise vmot shell; confirm logs and GPIO behavior.
- [ ] Persistence check on hardware: set model, reboot, verify restore (after finalizing NVS backend/partitioning).
- [ ] Twister reports: enable XML/HTML artifacts for CI (optional).

## Milestones
- M1 Bring-up: On-target VMOT smoke + GET_STATUS sanity; Target 2025-09-08; Owner Joey W.
- M2 Persistence: NVS partitioning + verified model restore; Target 2025-09-12; Owner Joey W.
- M3 Protocol: Expand strict SPI frame tests (RUN/MOVE/STOP); Target 2025-09-15; Owner Joey W.

## Blockers/Risks
- Finalize on-target settings backend and partitioning; LittleFS path exists but NVS layout for H753ZI needs confirmation.
- Hardware access windows may limit on-target testing cadence.

## How to run (quick refs)
- Build native unit: west build -b native_sim apps/stm32h753zi_stepper/tests/unit -d build/native_unit -p always
- Run tests: python zephyr/scripts/twister -T apps/stm32h753zi_stepper/tests/unit -p native_sim --inline-logs
- Board build/flash: west build -b nucleo_h753zi apps/stm32h753zi_stepper -d build/h753zi -p always && west flash --skip-rebuild --build-dir build/h753zi --runner openocd

## Pointers
- App: apps/stm32h753zi_stepper/
- Overlay: apps/stm32h753zi_stepper/nucleo_h753zi.overlay
- Persistence: apps/stm32h753zi_stepper/src/persist.c
- L6470: apps/stm32h753zi_stepper/src/l6470.*
- Tests: apps/stm32h753zi_stepper/tests/unit/ and tests/unit/testcase.yaml

---
Change log
- 2025-09-04: Added root STATUS.md anchor; cleaned warnings (SYS_INIT prototype, weak mock check), added Twister task; documented LittleFS sentinel and mkfs enablement.
