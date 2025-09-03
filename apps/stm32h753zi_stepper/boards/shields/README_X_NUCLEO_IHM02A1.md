X-NUCLEO-IHM02A1 (template) — integration notes

Files added as a minimal template in this app to help enable the IHM02A1 shield.

Files:
- shield.yml — descriptor name: x_nucleo_ihm02a1
- Kconfig.shield — Kconfig option: SHIELD_X_NUCLEO_IHM02A1
- x_nucleo_ihm02a1.overlay — device-tree overlay template (edit pins/compat)

How to use (recommended order):
1. Edit x_nucleo_ihm02a1.overlay to match the real pin labels and compatible strings
   for the IHM02A1 and the Nucleo-H753ZI (check ST docs and board device tree).
2. Configure the build to use the shield before connecting hardware:
   - Build with: west build -b nucleo_h753zi apps/stm32h753zi_stepper -- -DSHIELD=x_nucleo_ihm02a1
   - Or set SHIELD in CMake args or prj.conf as needed.
3. Power off the Nucleo board.
4. Stack/attach the X-NUCLEO-IHM02A1 shield physically.
5. Power on, flash, and test.

When to power off and install:
- Always power off the Nucleo board before stacking shields. Configure and build
  the firmware (with correct overlay) before attaching the shield, so the firmware
  initializes hardware correctly on first boot.

Notes:
- This is a template. Verify L6474 compatible string, pin mappings (enable/standby/fault),
  and whether the shield uses SPI, I2C, or direct GPIOs for control. Update overlay
  accordingly.

Manufacturer specs and recommended safe defaults
------------------------------------------------
The X-NUCLEO-IHM02A1 shield is based on ST's L6470 stepper driver. Key manufacturer
specifications (see ST product pages and datasheet) include:

- Operating voltage (VMOT): 8 V to 45 V DC
- Peak output current: up to 7 A (peak) per driver; ~3 A RMS continuous (board thermal-limited)
- Microstepping: up to 1/128 microstepping supported by the L6470

Kconfig defaults applied in `Kconfig.shield` (these match conservative, manufacturer-driven
recommendations and protect typical motors/drivers):

- `L6470_MAX_SPEED_STEPS_PER_SEC` = 20000 (steps/s) — conservative maximum step rate for
   many hobby/industrial steppers when microstepping is used. Adjust to your motor's
   mechanical limits and the microstep setting.
- `L6470_MAX_CURRENT_MA` = 3000 (mA) — a safe continuous current limit (3 A RMS). The
   shield and L6470 support higher peak currents but continuous current should be set
   to the motor and PCB thermal limits.
- `L6470_MAX_STEPS_PER_COMMAND` = 8,388,607 — keeps within the L6470's 22-bit capable
   ranges used by the driver implementation.
- `L6470_MAX_MICROSTEP` = 128 — matches the device capability (1/128 microstepping).

References:
- L6470 product page and datasheet: https://www.st.com/en/motor-drivers/l6470.html
- X-NUCLEO-IHM02A1 product page / databrief: https://www.st.com/en/evaluation-tools/x-nucleo-ihm02a1.html

When to change these values
---------------------------
- If you use a motor with lower thermal/current capability, reduce `L6470_MAX_CURRENT_MA`.
- If your mechanical system cannot tolerate high step rates (resonance, missed steps), reduce
   `L6470_MAX_SPEED_STEPS_PER_SEC` accordingly.
- If you use an alternate driver board or a different revision of the IHM02A1 with a different
   power stage, update these defaults to match the hardware's documented safe limits.
