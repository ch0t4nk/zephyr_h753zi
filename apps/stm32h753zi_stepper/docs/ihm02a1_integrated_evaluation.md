# Integrated Evaluation: X-NUCLEO-IHM02A1 on Nucleo‑H753ZI (Zephyr)

This document merges a blind, four‑phase evaluation with a code‑accurate review of the application in `apps/stm32h753zi_stepper`, focused on the X‑NUCLEO‑IHM02A1 (L6470) shield on the ST Nucleo‑H753ZI (Nucleo‑144). It includes current implementation status, traceability to the codebase, and prioritized, actionable next steps for safe bring‑up and motion.

- Repo: https://github.com/ch0t4nk/zephyr_h753zi
- App root: `apps/stm32h753zi_stepper`
- Zephyr: v4.2 (tooling reports v4.2.0-31-g663da2302b04)
- Boards in scope: `nucleo_h753zi` (target) and `native_sim` (unit tests)

---

## Phase 1 — Workspace Mapping (code‑accurate)

### 1.1 Layout and key assets

- Directories
  - `src/`: `main.c`, `l6470.c/.h`, `l6470_codec.c/.h`, `shell_stepper.c`, `hw_smoketest.c`, `persist.c/.h`, `stepper_models.c/.h`
  - `boards/shields/x_nucleo_ihm02a1/`: local shield overlay (reference wiring)
  - `tests/unit/`: ztest suites, Twister harness (`testcase.yaml`)
  - `docs/`, `reference/`, `scripts/` (model autogen, docs)
- Top‑level files
  - `CMakeLists.txt`: adds generated `build/generated/stepper_models_autogen.h`, sources in `src/`
  - `prj.conf`: logging, shell, networking, SPI, Settings + NVS, LittleFS (FMP), reboot
  - `nucleo_h753zi.overlay`: SPI1 on Arduino header and GPIO nodes for CS/FLAG/BUSY/RESET
  - `stepper_models.yml`: sample model entries (e.g., 17HS3001‑20B)

### 1.2 DeviceTree wiring (IHM02A1)

- `&arduino_spi` → SPI1 enabled; `spidev@0` with:
  - `spi-max-frequency = <20000000>;`
  - `cs-gpios = <&gpiod 14 GPIO_ACTIVE_LOW>;` (D10, PD14)
- Control GPIO nodes exposed via node labels:
  - `ihm02a1_flag` → PG14 (D2, active low, input)
  - `ihm02a1_busy` → PE13 (D3, active high, input)
  - `ihm02a1_reset` → PE14 (D4, active low, output)
- Storage: relies on board fixed partition `storage` (LittleFS via FMP)
- Note: The IHM02A1 reference shield does not provide a GPIO‑controlled VMOT switch. A DT node `ihm02a1_vmot_switch` is not defined by default.

### 1.3 Configuration highlights (`prj.conf`)

- Shell + logging (UART + RTT), GPIO, SPI enabled
- Settings subsystem (NVS backend); LittleFS enabled with `FS_LITTLEFS_FMP_DEV`
- `CONFIG_APP_LINK_WITH_FS=y` and `CONFIG_FILE_SYSTEM_MKFS=y` to allow app‑side formatting on first mount
- Networking included (not required for basic stepper bring‑up)

### 1.4 Anchors and entry points

- `main()` in `src/main.c`:
  - Initializes persistence (LittleFS with mkfs fallback and sentinel file)
  - Configures RESET/BUSY/FLAG pins (PE14/PE13/PG14) and pulses RESET low/high
  - Sets up SPI (MODE3, 5 MHz in code) and calls `l6470_init()`; polls status periodically
  - Starts unrelated network echo server (benign for stepper bring‑up)
- L6470 helper API in `src/l6470.c`:
  - `l6470_init`, `l6470_get_status_all` (two‑phase GET_STATUS for daisy chain)
  - Motion ops with safeguards: `l6470_run`, `l6470_move`, `l6470_disable_outputs`
  - Power interlock (software latch): `l6470_power_enable/disable/status`
  - Hardware RESET pulse via PE14, with board‑specific fallback if DT missing
- Shell interfaces
  - `stepper ...` command group (`src/shell_stepper.c`): status, probe, reset, run, move, disable, limits, power, model list/show/set
  - `vmot ...` (`src/hw_smoketest.c`): compiled, but no‑op unless a DT node `ihm02a1_vmot_switch` is provided
- Persistence: `persist.c` mounts `/lfs` (FMP on `storage`), creates `/.mounted` sentinel, and read/write helpers
- Models: `stepper_models.yml` + generator; settings handler persists “stepper/active” per axis

---

## Phase 2 — Algorithm and behavior identification

### 2.1 SPI protocol and frames

- L6470 opcodes and framing implemented; 4‑byte frames per device for simplicity
- Two‑device daisy chain supported; `l6470_get_status_all()` performs two‑phase GET_STATUS and parses per‑device 16‑bit status words
- Helpers in `l6470_codec.*` pack RUN/MOVE and status frames deterministically

### 2.2 Motion and safety gates

- Motion commands refuse to run when software power latch is disabled
- Basic fault screen: checks status bits (e.g., OCD, step loss) and blocks RUN/MOVE
- Limits enforced:
  - By active model when available (speed)
  - By Kconfig for max speed, max steps, current, microstep getters
- Outputs can be forced to Hi‑Z (SOFT/HARD)

### 2.3 Missing driver parameter init (intentional gap)

- No L6470 register programming (KVALs, OCD/STALL thresholds, microstepping, MAX_SPEED/ACC/DEC) is applied yet
- Real hardware should not be driven until a conservative, model‑driven parameter set is written at init

---

## Phase 3 — Traceability, tests, and gaps

### 3.1 Tests

- Native simulation unit tests (`native_sim`) with Twister:
  - Current run: 1/1 configuration passed; 9/9 test cases passed
  - Artifacts generated under `twister-out/` (xUnit XML, JSON, summary)
- Tests focus on frame packing/decoding, status parsing, and model selection behavior

### 3.2 Traceability matrix (key slices)

- SPI wiring → DT overlay node labels (`nucleo_h753zi.overlay`) → used by `main.c` and `l6470.c`
- Status polling/logs → `main.c` periodic thread, `shell_stepper.c status`
- Power interlock → `l6470_*power*()` guarded; shell command exposes state; no real VMOT switching without added hardware
- Persistence → `persist.c` mount + sentinel + R/W; `stepper_models.c` settings handler stores active model

### 3.3 Known gaps / risks

- Parameter init absent: driving motors without programming L6470 registers is unsafe
- VMOT switching not present on shield: software “power” is only an interlock, not physical VMOT control
- No IRQ/FLAG handling yet: status is polled; faults not latched via interrupt
- Hardcoded GPIOE string usage in `l6470_power_*` (should be DT‑backed or removed if not wired)
- Networking code increases footprint; can be disabled for motion‑only builds

---

## Phase 4 — Readiness and summary dashboard

| Area                      | Status                 | Notes |
|---------------------------|------------------------|-------|
| DT wiring (SPI/GPIO)      | Implemented            | SPI1 + CS (PD14), FLAG (PG14), BUSY (PE13), RESET (PE14) |
| L6470 SPI protocol        | Implemented            | RUN/MOVE/HIZ/status; two‑phase GET_STATUS |
| Safety interlocks         | Basic                  | Software power latch, fault screen before motion |
| Parameter programming     | Missing                | Must set KVAL/OCD/STALL/microstep/speeds before motion |
| VMOT control              | Not present            | Add external switch + DT, or manual power |
| Persistence (LittleFS)    | Implemented            | FMP on `storage`; mkfs fallback + sentinel |
| Shell UX                  | Implemented            | stepper and vmot (vmot no‑op without DT node) |
| Tests (native_sim)        | Passing                | 1/1 config, 9/9 cases |
| On‑target smoke           | Partial                | Status/reset; motion blocked by safety gate if power off |

---

## Actionable next steps (prioritized)

1) Implement conservative L6470 parameter init
- Add a startup routine that programs, at minimum, per‑device: microstepping, KVAL_RUN/ACC/DEC/HOLD, OCD_TH, STALL_TH, MAX_SPEED, ACC/DEC.
- Derive reasonable defaults from `stepper_models.yml` and clamp to Kconfig safety limits.
- Gate `stepper power on` so it runs the init once and verifies status; refuse motion if init fails.

2) Decide on VMOT control strategy
- If you want software VMOT control: add a high‑side (preferred) switch and define a DT node `ihm02a1_vmot_switch` with a GPIO; wire `hw_smoketest.c` and optionally `stepper power on/off` to it.
- Otherwise, document manual VMOT control steps in the README and keep the software power latch purely as a command interlock.

3) Replace ad‑hoc GPIO strings with DT‑backed specs
- In `l6470_power_*`, switch to a `gpio_dt_spec` sourced from a DT node (e.g., `ihm02a1_power_en`) and fall back cleanly if absent.

4) Add FLAG/BUSY interrupt handling (optional but recommended)
- Configure GPIO interrupt on `ihm02a1_flag` (active low) to latch faults promptly.
- Debounce or re‑read status on ISR; expose fault reason via `stepper status -v`.

5) Expand shell diagnostics and configuration
- Add `stepper config apply/show` to program/read key L6470 registers (read‑back verification).
- Add `stepper clearfaults` helper (sequence: read status until clear, or device reset if required).

6) Model‑driven parameter mapping
- Extend `stepper_models.yml` schema with fields for KVALs and thresholds; generate a struct and apply at init.
- Unit test: verify generated values program the expected register writes (mock SPI).

7) Tighten on‑target smoke procedure
- Script a bring‑up flow: reset → parameter init → status check → enable outputs → tiny move → disable outputs.
- Log persistence status (`/lfs/.mounted`) and capture a short status poll trace.

8) CI and reporting
- Add a CI job to run Twister on `native_sim` and publish xUnit + `summary.txt` as artifacts.
- Optionally add coverage for unit logic (host coverage) and a style/lint stage.

9) Streamline builds
- Consider a minimal `prj.conf` (no networking) for motion‑only firmware to reduce footprint during early bring‑up.

10) Documentation
- Add a “Safe power‑on checklist” (see below) to README and this doc; link to L6470 datasheet sections referenced by register init.

---

## Safe power‑on checklist (before attaching motors)

- Flash app with shield connected, no motors attached.
- Verify status path with no faults:
  - `stepper reset` → `stepper status -v` (both devices Hi‑Z; no OCD/thermal/UVLO flags)
- Keep software power off until parameter init is in place:
  - `stepper power status` should be DISABLED after boot
- Apply parameter init (from step 1) and re‑read status
- Power VMOT (manually or via DT‑controlled switch) and attach motors
- Start with very low KVALs and speeds; try a tiny move: `stepper move 0 1 50`
- Monitor `stepper status -v` for any faults; disable outputs if anything latches

---

## Appendix — Reconciliation notes vs. “blind” template

- No HAL/Cube direct init is used; Zephyr device APIs are used for SPI/GPIO. There is no `SystemClock_Config` in this app.
- No Zephyr “stepper” driver API is used; control is implemented in the local `l6470.*` helpers.
- No ISR handlers presently; status is polled. Mapping for FLAG/BUSY is present via DT nodes.
- The directory structure differs from the generic template (no `include/` or `kconfig/` subdirs; overlays live at app root and under `boards/shields/`).
- Tests use Zephyr Twister with `native_sim`; latest run: 1/1 configuration, 9/9 cases passed.

---

Last updated: based on repo state with Twister passing (native_sim) and overlays/configs as of v4.2.
