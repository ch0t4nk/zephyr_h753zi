# STM32CubeExpansion_SPN2_V1.1.1 Reference Guide

This directory contains the official STMicroelectronics X-CUBE-SPN2 software package for the X-NUCLEO-IHM02A1 dual stepper motor shield (L6470 dSPIN).

## Key Assets and Where to Find Them

### 1. Documentation
- **Documentation/X-CUBE-SPN2.chm**: Main help file (Windows CHM format) with API, usage, and theory.
- **Release_Notes.html**: Package release notes and version history.

### 2. L6470 Driver Source
- **Drivers/BSP/Components/L6470/L6470.c, L6470.h**: Core L6470 driver implementation (all commands, register access, parameter setup, motion, faults, etc).
- **Drivers/BSP/X-NUCLEO-IHM02A1/xnucleoihm02a1.c, xnucleoihm02a1.h**: Board support for the shield (CS mapping, multi-device, shield abstraction).

### 3. Example Projects
- **Projects/STM32F401RE-Nucleo/Examples/MotionControl/MicrosteppingMotor/**: Complete demo for dual-motor control, including:
  - **Src/main.c**: Main application logic (init, motion, demo sequences)
  - **Src/params.c**: L6470 parameter setup (microstepping, current, speed, etc)
  - **Src/example.c**: Example routines for motion and control
  - **Inc/**: All relevant headers

### 4. HAL and Board Support
- **Drivers/BSP/STM32F4xx-Nucleo/**: Board-specific support for STM32 Nucleo boards
- **Drivers/STM32F4xx_HAL_Driver/**: STM32 HAL drivers (for reference, not needed for Zephyr)

## How to Use This Reference
- **For L6470 command details and register usage:** See `L6470.c`/`L6470.h`.
- **For shield-specific CS/motor mapping:** See `xnucleoihm02a1.c`/`.h`.
- **For parameter setup and motion examples:** See `Projects/.../MicrosteppingMotor/Src/params.c` and `main.c`.
- **For full demo flow:** Start with `main.c` in the example project.

## Quick Links
- [L6470 Driver Source](Drivers/BSP/Components/L6470/)
- [Shield Board Support](Drivers/BSP/X-NUCLEO-IHM02A1/)
- [Dual Motor Example Project](Projects/STM32F401RE-Nucleo/Examples/MotionControl/MicrosteppingMotor/Src/)
- [Documentation](Documentation/)

---
**Tip:** Use this README as a map to quickly locate code and docs for porting or reference in your Zephyr project.
