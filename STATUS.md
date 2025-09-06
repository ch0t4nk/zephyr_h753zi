# Project STATUS (primary context anchor)

Generated: 2025-09-05 15:10 UTC
Owner: Joey W.  Repo: ch0t4nk/zephyr_h753zi

## Context Bootstrapping
STATUS.md is the primary context anchor for GitHub Copilot sessions; downstream orchestration depends on its integrity.

## Overview
- Target: Zephyr app `apps/stm32h753zi_stepper` for Nucleo-H753ZI + X-NUCLEO-IHM02A1 (two L6470 steppers via SPI daisy-chain)
- Workflow: west-based builds, native_sim unit tests (ztest/Twister), and on-target flashing (OpenOCD/ST-LINK)
- Models SSOT: `stepper_models.yml` -> `scripts/gen_stepper_models.py` -> `build/generated/stepper_models_autogen.h` -> runtime
- Architecture: heartbeat LED, L6470 driver (param programming + positioning + GET_STATUS), Zephyr shell, persistence (LittleFS)

## Current status: OPERATIONAL
- Build system: native_sim app, nucleo_h753zi app, and unit tests compile; Twister run is green (14/14)
- Hardware flashing: OpenOCD reliable at reduced SWD speeds; ST-LINK runner available
- L6470 driver: complete parameter programming, GET_STATUS decode, and positioning commands
- Shell: model select/list, status, run/move/goto/home/stop, persistence controls
- Persistence: LittleFS mounts with 1KB cache; settings survive reboots
- Background polling: delayable work, 32-sample ring buffer, shell toggles; disabled by default at build time

## Recent achievements
- LittleFS persistence: resolved -16 (EBUSY) by removing fstab automount; tuned 1KB cache for STM32H7 flash geometry (2025-09-05)
- STM32H7 flash geometry research: 128KB erase, 32B write; validated cache sizing independence (2025-09-05)
- End-to-end automation: `tools/flash_try.py` for flash→test→validate→reboot→verify (2025-09-05)
- West workflow: standardized docs and tasks on west (2025-09-04)
- Parameter coverage: extended model-driven ACC/DEC/MAX_SPEED/STALL_TH programming; exact encodings tested (2025-09-05)
- Positioning API: goto/home/reset_pos/get_abs_pos/stop/busy with shell integration (2025-09-04)

## Decisions
- Fixed 4-byte SPI frames for deterministic daisy-chain I/O
- Two-stage GET_STATUS (opcode, then NOPs) for robust multi-device reads
- LittleFS (manual mount) over NVS; fstab automount disabled
- West-centric workflow; VS Code tasks reflect standard commands
- LittleFS cache ~1KB for build compatibility with H7 cbprintf constraints
- Background polling behind Kconfig; runtime toggles and diagnostics when enabled

## Tasks

### Completed
- [x] Persistence infrastructure (2025-09-05) — mount, settings storage, reboot validation — Owner: Joey W.
- [x] STM32H7 flash research (2025-09-05) — geometry analysis, cache sizing — Owner: Joey W.
- [x] Flash-and-test automation (2025-09-05) — `tools/flash_try.py` — Owner: Joey W.
- [x] West workflow integration (2025-09-04) — docs + tasks — Owner: Joey W.
- [x] Positioning API implementation (2025-09-04) — Owner: Joey W.
- [x] Extended parameter programming (2025-09-04) — ACC/DEC/MAX_SPEED/STALL_TH — Owner: Joey W.
- [x] Precise parameter encoding tests (2025-09-05) — ACC/DEC/MAX_SPEED/STALL_TH encodings — Owner: Joey W.

### In progress
- [ ] Real datasheet values population — transcribe motor params into `stepper_models.yml` (Owner: Joey W., Target: 2025-09-06)

### Short-term roadmap
- [ ] Advanced movement commands — `stepper jog` and `stepper waitpos` (Owner: Joey W., Target: 2025-09-09)
- [ ] Hardware validation suite — on-target smoke (power/status/move/persist) (Owner: Joey W., Target: 2025-09-08)
- [ ] Performance optimization — memory, SPI timing, priorities (Owner: Joey W., Target: 2025-09-10)

## Milestones
- M0 Foundation — COMPLETE (2025-09-04): build system, driver core, positioning, shell (Owner: Joey W.)
- M1 Persistence — COMPLETE (2025-09-05): LittleFS, settings, reboot validation (Owner: Joey W.)
- M2 Hardware validation — IN PROGRESS (Target: 2025-09-08) (Owner: Joey W.)
- M3 Production readiness — PLANNED (Target: 2025-09-12) (Owner: Joey W.)
- M4 Advanced features — PLANNED (Target: 2025-09-15) (Owner: Joey W.)

## Known issues
- RESOLVED: LittleFS -16 (EBUSY) mount failures — removed fstab automount; manual mount (2025-09-05)
- RESOLVED: H7 flash geometry uncertainty — confirmed from DTS (2025-09-05)
- RESOLVED: OpenOCD reliability — reduced SWD to 1.8 MHz (2025-09-04)
- RESOLVED: Build inconsistencies — standardized on west (2025-09-04)
- MONITOR: cbprintf limits with large FS cache — keep ~1KB
- MONITOR: hardware access scheduling windows may affect cadence

## Quick reference

### Build & test
```bash
# Initialize environment
. zephyr/zephyr-env.sh

# Unit tests (native_sim + Twister)
west build -b native_sim apps/stm32h753zi_stepper/tests/unit -d build/native_unit -p always
$ZEPHYR_BASE/scripts/twister -T apps/stm32h753zi_stepper/tests/unit -p native_sim --inline-logs -j 4 -o twister-out -v

# Application builds
west build -b native_sim apps/stm32h753zi_stepper -d build/native -p always
west build -b nucleo_h753zi apps/stm32h753zi_stepper -d build/h753zi -p always

# Flash & automation
west flash --skip-rebuild --build-dir build/h753zi --runner openocd
python3 tools/flash_try.py
```

### Key files
- Application: `apps/stm32h753zi_stepper/`
- Device tree: `apps/stm32h753zi_stepper/nucleo_h753zi.overlay`, `apps/stm32h753zi_stepper/fstab.overlay` (disabled)
- Persistence: `apps/stm32h753zi_stepper/src/persist.c`, `apps/stm32h753zi_stepper/src/persist.h`
- L6470: `apps/stm32h753zi_stepper/src/l6470.*`
- Models & gen: `apps/stm32h753zi_stepper/stepper_models.yml`, `apps/stm32h753zi_stepper/scripts/gen_stepper_models.py`
- Tests: `apps/stm32h753zi_stepper/tests/unit/`, `apps/stm32h753zi_stepper/tests/unit/testcase.yaml`
- Tools: `tools/flash_try.py`, VS Code tasks in `.vscode/tasks.json`

---
## Change log
- 2025-09-05 — LittleFS persistence operational; automation added; west workflow standardized
- 2025-09-04 — Positioning APIs; extended parameter programming; tests and build stabilized
- 2025-09-03 — Initial structure; L6470 driver foundation; model generation framework
