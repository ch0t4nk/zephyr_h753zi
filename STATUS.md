# Project STATUS (primary context anchor)

Generated: 2025-09-04 00:00 UTC
Owner: Joey W.  Repo: $HOME/zephyr-dev

Context Bootstrapping
- STATUS.md is the primary context anchor for GitHub Copilot sessions; downstream orchestration depends on its integrity.

## Overview
- Zephyr app: `apps/stm32h753zi_stepper` for Nucleo‑H753ZI + X‑NUCLEO‑IHM02A1 (two L6470s, SPI daisy‑chain).
- Workflow: native host tests (native_sim via ztest/Twister) + on‑target build/flash (OpenOCD).
- Models SSOT: `stepper_models.yml` → `scripts/gen_stepper_models.py` → `build/generated/stepper_models_autogen.h` → `src/stepper_models.c`.

## Current status
- Builds: native_sim app and nucleo_h753zi app both link successfully; unit build compiles (ctest intentionally finds none; Twister is the runner).
- Status decode: on‑demand GET_STATUS works with human‑readable decode; periodic polling thread not yet enabled.
- Parameter programming: model‑driven apply covers STEP_MODE, KVAL_* (hold/run/acc/dec), OCD_TH, ACC/DEC, MAX_SPEED, STALL_TH; applied on power‑on.
- Positioning (fast wins): added goto/goto_dir/home/reset_pos, ABS_POS read, soft/hard stop, BUSY read/wait. Shell: `stepper goto|goto_dir|home|zero|pos|stop|busy|waitbusy`.
- Persistence: LittleFS sentinel present; NVS backend planning for model selection durability.
- Tests: ztest suites build on native_sim; new unit harness for parameter apply; more assertions planned.

## Key changes (recent)
- Extended model schema (acc_sps2/dec_sps2) and generator; unit test build now generates the autogen header.
- Expanded `l6470_apply_model_params()` to program ACC/DEC/MAX_SPEED/STALL_TH and existing STEP_MODE/KVAL/OCD_TH.
- Added positioning and control APIs (goto/home/reset_pos/get_abs_pos/stop/busy) and wired shell commands.
- Twister task guidance: prefer `$ZEPHYR_BASE/scripts/twister` or `twister` after sourcing `zephyr-env.sh` (avoid hard‑coded absolute paths).

## Decisions
- Fixed 4‑byte frames per device for simple, deterministic SPI packing across the daisy chain.
- Two‑phase GET_STATUS (opcode broadcast then NOP clock‑out) for reliable per‑device status reads.
- Stick with Zephyr Twister + native_sim for unit tests; avoid external simulators unless hardware‑specific modeling is required.

## Tasks (short‑term)
- [ ] Populate `stepper_models.yml` with real datasheet values (current, accel/dec, speed, stall). Owner: Joey W. Target: 2025‑09‑06.
- [ ] Add precise unit tests that assert encoded ACC/DEC/MAX_SPEED/STALL_TH bytes. Owner: Joey W. Target: 2025‑09‑07.
- [ ] Add periodic status poll worker (e.g., 200 ms) gated by Kconfig, with a ring buffer (last 32 samples) and CLI toggle. Owner: Joey W. Target: 2025‑09‑09.
- [ ] Add `stepper jog <dev> <±steps>` and `stepper waitpos <dev> <abs> [tol] [timeout_ms]`. Owner: Joey W. Target: 2025‑09‑09.
- [ ] Ensure Twister task uses correct path; run Twister and publish artifacts. Owner: Joey W. Target: 2025‑09‑05.
- [ ] On‑target smoke: power, status ‑v, goto/home, stop, pos. Owner: Joey W. Target: 2025‑09‑08.
- [ ] Persistence on hardware: enable NVS settings backend and verify model restore after reboot. Owner: Joey W. Target: 2025‑09‑12.

## Milestones
- M0 Positioning primitives: goto/home/reset_pos/pos/stop/busy wired; Status: COMPLETE (2025‑09‑04). Owner: Joey W.
- M1 Bring‑up: On‑target VMOT/power + GET_STATUS sanity and shell smoke; Target: 2025‑09‑08. Owner: Joey W.
- M2 Persistence: NVS partitioning + verified model restore; Target: 2025‑09‑12. Owner: Joey W.
- M3 Protocol coverage: strict SPI frame tests (RUN/MOVE/STOP + SET_PARAM encodings); Target: 2025‑09‑15. Owner: Joey W.

## Blockers/Risks
- Datasheet transcription for real models takes time; keep conservative defaults until verified on hardware.
- On‑target settings/NVS partitioning details for H753ZI need confirmation.
- Occasional Twister path issues from absolute paths; standardize on `$ZEPHYR_BASE/scripts/twister`.
- Hardware access windows may limit on‑target testing cadence.

## How to run (quick refs)
- Init env: `. zephyr/zephyr-env.sh` (sets ZEPHYR_BASE, etc.)
- Unit build (native_sim): `west build -b native_sim apps/stm32h753zi_stepper/tests/unit -d build/native_unit -p always`
- Run tests (Twister): `twister -T apps/stm32h753zi_stepper/tests/unit -p native_sim --inline-logs --outdir build/twister-out`
- App build (native_sim): `west build -b native_sim apps/stm32h753zi_stepper -d build/native -p always`
- App build/flash (H753ZI): `west build -b nucleo_h753zi apps/stm32h753zi_stepper -d build/h753zi -p always && west flash --skip-rebuild --build-dir build/h753zi --runner openocd`

## Pointers
- App: `apps/stm32h753zi_stepper/`
- Overlay: `apps/stm32h753zi_stepper/nucleo_h753zi.overlay`
- Persistence: `apps/stm32h753zi_stepper/src/persist.c`
- L6470 driver: `apps/stm32h753zi_stepper/src/l6470.*`
- Models: `apps/stm32h753zi_stepper/stepper_models.yml`, generator in `apps/stm32h753zi_stepper/scripts/`
- Tests: `apps/stm32h753zi_stepper/tests/unit/` and `tests/unit/testcase.yaml`

---
Change log
- 2025‑09‑04: Added positioning APIs and shell commands; expanded model‑driven parameter programming (ACC/DEC/MAX_SPEED/STALL_TH); unit/native/H753ZI builds green; Twister path guidance; normalized STATUS formatting and tasks.
