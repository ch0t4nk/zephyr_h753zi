# Project STATUS (primary context anchor)

Generated: 2025-09-05 14:10 UTC
Owner: Joey W.  Repo: $HOME/zephyr-dev

## Context Bootstrapping
STATUS.md is the primary context anchor for GitHub Copilot sessions; downstream orchestration depends on its integrity.

## Overview
- **Target**: Zephyr app `apps/stm32h753zi_stepper` for Nucleo‑H753ZI + X‑NUCLEO‑IHM02A1 (two L6470 stepper drivers, SPI daisy‑chain)
- **Workflow**: West-based build system with native_sim unit tests (ztest/Twister) + on‑target deployment (OpenOCD/ST-LINK)
- **Models SSOT**: `stepper_models.yml` → `scripts/gen_stepper_models.py` → `build/generated/stepper_models_autogen.h` → runtime configuration
- **Architecture**: Multi-threaded with heartbeat LED, background L6470 polling, TCP echo server, shell interface, persistent settings

## Current Status ✅ OPERATIONAL
- **Build System**: All targets compile successfully (native_sim app, nucleo_h753zi app, unit tests via Twister)
- **Hardware Integration**: STM32H753ZI flash operations stable with OpenOCD runner at reduced SWD speeds (1.8MHz)
- **L6470 Driver**: Complete SPI frame encoding/decoding with GET_STATUS decode, parameter programming, positioning commands
- **Shell Interface**: Full command set operational - model selection, status inspection, movement control, persistence management
- **Persistence Infrastructure**: **RESOLVED** - LittleFS successfully mounting with optimized 1KB cache, settings survive reboots
- **Test Framework**: Native_sim unit tests with ztest, automated flash-and-test script (`tools/flash_try.py`) working
- **Memory Management**: Optimized stack sizes for LittleFS operations, heap monitoring enabled

## Key Achievements (Recent)
- **LittleFS Persistence Resolution** (2025-09-05): Resolved -16 (EBUSY) mount failures by removing conflicting fstab automount, implemented research-driven 1KB cache configuration for STM32H7 128KB erase blocks
- **STM32H7 Flash Geometry Research** (2025-09-05): Confirmed erase-block-size=128KB, write-block-size=32 bytes from device tree, validated LittleFS cache_size independence from erase-block-size
- **End-to-End Automation** (2025-09-05): `tools/flash_try.py` script providing automated flash→test→validate→reboot→verify workflow with comprehensive logging
- **Settings Persistence Validation** (2025-09-05): Confirmed stepper model configurations survive reboot cycles, settings file operations working on-target
- **West Workflow Standardization** (2025-09-04): Aligned all documentation and VS Code tasks around west commands, deprecated direct tool invocations
- **Expanded L6470 Parameter Programming**: Extended model-driven parameter application to cover ACC/DEC/MAX_SPEED/STALL_TH beyond initial STEP_MODE/KVAL/OCD_TH
- **Positioning API Implementation**: Complete goto/home/reset_pos/get_abs_pos/stop/busy command set with shell integration
- **Test Infrastructure**: Ztest unit test framework with Twister runner, proper native_sim builds generating autogen headers

## Architecture Decisions
- **Fixed 4-byte SPI frames**: Deterministic daisy-chain communication with consistent per-device addressing
- **Two-phase GET_STATUS protocol**: Opcode broadcast followed by NOP clock-out for reliable multi-device status reads
- **LittleFS over NVS**: File-based persistence with custom configuration optimized for STM32H7 flash geometry
- **Manual filesystem mounting**: Removed device tree fstab automount to prevent resource conflicts, explicit mount control in application
- **West-centric workflow**: Standardized on west commands for build/flash/test operations, VS Code task integration
- **Moderate cache sizing**: Research-validated 1KB LittleFS cache size independent of 128KB erase blocks for compilation compatibility

## Tasks

### Completed ✅
- [x] **Persistence Infrastructure** (2025-09-05): LittleFS mounting, settings storage/retrieval, reboot survival validation - Joey W.
- [x] **STM32H7 Flash Research** (2025-09-05): Device tree geometry analysis, cache sizing optimization - Joey W.
- [x] **End-to-End Testing Automation** (2025-09-05): Flash-and-test script with comprehensive validation - Joey W.
- [x] **West Workflow Integration** (2025-09-04): Documentation alignment, VS Code tasks, build system standardization - Joey W.
- [x] **Positioning API Implementation** (2025-09-04): Complete movement control command set with shell interface - Joey W.
- [x] **Extended Parameter Programming** (2025-09-04): Model-driven ACC/DEC/MAX_SPEED/STALL_TH configuration - Joey W.

### In Progress 🔄
- [ ] **Real Datasheet Values Population**: Transcribe authentic motor parameters from manufacturer datasheets into `stepper_models.yml`. Owner: Joey W. Target: 2025-09-06.
- [ ] **Background Status Polling**: Implement configurable periodic GET_STATUS worker (200ms default) with ring buffer (32 samples) and CLI controls. Owner: Joey W. Target: 2025-09-07.

### Short-term Roadmap 📋
- [ ] **Precise Parameter Encoding Tests**: Unit tests asserting correct ACC/DEC/MAX_SPEED/STALL_TH byte encoding. Owner: Joey W. Target: 2025-09-07.
- [ ] **Advanced Movement Commands**: Add `stepper jog <dev> <±steps>` and `stepper waitpos <dev> <abs> [tol] [timeout_ms]`. Owner: Joey W. Target: 2025-09-09.
- [ ] **Hardware Validation Suite**: On-target smoke testing (power, status -v, movement, persistence) with real motors. Owner: Joey W. Target: 2025-09-08.
- [ ] **Performance Optimization**: Memory usage analysis, SPI timing optimization, thread priority tuning. Owner: Joey W. Target: 2025-09-10.

## Milestones
- **M0 Foundation** ✅ COMPLETE (2025-09-04): Build system, L6470 driver core, positioning primitives, shell interface. Owner: Joey W.
- **M1 Persistence** ✅ COMPLETE (2025-09-05): LittleFS integration, settings storage, reboot validation, research-driven optimization. Owner: Joey W.  
- **M2 Hardware Validation** 🎯 IN PROGRESS (Target: 2025-09-08): On-target testing with real motors, VMOT power validation, movement verification. Owner: Joey W.
- **M3 Production Readiness** 📅 PLANNED (Target: 2025-09-12): Performance optimization, comprehensive test coverage, documentation completion. Owner: Joey W.
- **M4 Advanced Features** 📅 PLANNED (Target: 2025-09-15): Background polling, advanced movement patterns, telemetry collection. Owner: Joey W.

## Known Issues & Resolutions
- **✅ RESOLVED: LittleFS -16 (EBUSY) mounting failures** (2025-09-05): Removed conflicting device tree fstab automount, implemented manual mounting with optimized configuration
- **✅ RESOLVED: STM32H7 flash geometry uncertainty** (2025-09-05): Confirmed 128KB erase blocks, 32-byte write blocks from device tree analysis  
- **✅ RESOLVED: OpenOCD reliability issues** (2025-09-04): Reduced SWD clock speeds to 1.8MHz for stable flash operations
- **✅ RESOLVED: Build system inconsistencies** (2025-09-04): Standardized on west-centric workflow, updated all documentation and tasks
- **⚠️ MONITORING: cbprintf limitations with large cache sizes**: Compilation fails with >4KB LittleFS cache, resolved with 1KB configuration
- **⚠️ MONITORING: Hardware access scheduling**: On-target testing windows may impact development cadence

## Quick Reference

### Build & Test Commands
```bash
# Initialize environment
. zephyr/zephyr-env.sh

# Unit tests (native_sim with Twister)
west build -b native_sim apps/stm32h753zi_stepper/tests/unit -d build/native_unit -p always
twister -T apps/stm32h753zi_stepper/tests/unit -p native_sim --inline-logs --outdir build/twister-out

# Application builds
west build -b native_sim apps/stm32h753zi_stepper -d build/native -p always          # Native simulation
west build -b nucleo_h753zi apps/stm32h753zi_stepper -d build/h753zi -p always      # STM32H753ZI target

# Flash and automated testing
west flash --skip-rebuild --build-dir build/h753zi --runner openocd                 # Manual flash
python3 tools/flash_try.py                                                          # Automated flash + test + validation
```

### Key Directories & Files
- **Application**: `apps/stm32h753zi_stepper/`
- **Device Tree**: `apps/stm32h753zi_stepper/nucleo_h753zi.overlay`, `apps/stm32h753zi_stepper/fstab.overlay` (disabled)
- **Persistence**: `apps/stm32h753zi_stepper/src/persist.c`, `apps/stm32h753zi_stepper/src/persist.h`
- **L6470 Driver**: `apps/stm32h753zi_stepper/src/l6470.*`
- **Models & Generation**: `apps/stm32h753zi_stepper/stepper_models.yml`, `apps/stm32h753zi_stepper/scripts/gen_stepper_models.py`
- **Tests**: `apps/stm32h753zi_stepper/tests/unit/`, `apps/stm32h753zi_stepper/tests/unit/testcase.yaml`
- **Tools**: `tools/flash_try.py` (automated testing), VS Code tasks in `.vscode/tasks.json`

### Configuration Files
- **Project**: `apps/stm32h753zi_stepper/prj.conf` (Zephyr config, LittleFS settings)
- **CMake**: `apps/stm32h753zi_stepper/CMakeLists.txt` (build configuration, model generation)
- **Flash Layout**: Custom `app_storage` partition at 0x80000 (512KB on Bank 0)

---
## Change Log
- **2025-09-05**: **MAJOR MILESTONE** - LittleFS persistence fully operational. Resolved -16 mounting conflicts through fstab removal and research-driven STM32H7 optimization (1KB cache vs 128KB erase blocks). End-to-end automation working. Settings survive reboots. West workflow standardized.
- **2025-09-04**: Positioning APIs implemented, extended parameter programming, shell command integration, test framework established, build system stabilized.
- **2025-09-03**: Initial project structure, L6470 driver foundation, basic SPI communication, model generation framework.
