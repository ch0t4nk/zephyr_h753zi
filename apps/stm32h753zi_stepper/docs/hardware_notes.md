Hardware Notes - X-NUCLEO-IHM02A1 and Nucleo-H753ZI

1) Motor power (PE11 / Arduino D5)

- Observation: The application currently controls motor power using a GPIO bound at runtime to "GPIOE" pin 11 (PE11), exposed on the Nucleo Arduino header as Arduino D5.
- Reason: The X-NUCLEO-IHM02A1 expansion board does not include a VMOT power switch tied to an Arduino header by default. The chosen pin (PE11) is convenient on the Nucleo board but is not declared in a device-tree node in the provided shield overlay.
- Recommendation: For better traceability and portability, add a DT node to the shield overlay, e.g.:

  / {
    ihm02a1_vmot_switch: ihm02a1-vmot-switch {
      gpios = <&gpioe 11 GPIO_ACTIVE_HIGH>; /* or GPIO_ACTIVE_LOW depending on wiring */
      status = "okay";
    };
  };

  Then in the driver use GPIO_DT_SPEC_GET(IHM02A1_VMOT_NODE, gpios) and avoid hard-coded device_get_binding("GPIOE").

- Schematic: See `docs/x-nucleo-ihm02a1_schematic.pdf` for the shield motor supply wiring and lack of onboard VMOT switch.

2) SPI frequency (overlay vs runtime)

- Overlay: `boards/shields/x_nucleo_ihm02a1/x_nucleo_ihm02a1.overlay` sets `spi-max-frequency = <20000000>` (20 MHz) on the `&arduino_spi` node.
- Runtime: The application configures `struct spi_config.frequency = 5_000_000` (5 MHz) in `src/main.c` via `L6470_SPI_FREQ_HZ`.
- Notes:
  - L6470 datasheet's "Digital interface" section lists recommended maximum clock rates for reliable operation; many user examples use 5..10 MHz.
  - The STM32H7's SPI peripheral can run at much higher frequencies, but board wiring, level shifters, and the shield's trace lengths may limit reliable speed.
  - The overlay value (20 MHz) documents the controller's capability but does not force the application to use that speed. Keep them aligned when increasing runtime speed.

- Recommendation:
  - Keep the conservative 5 MHz runtime default for robustness.
  - If you plan to raise speed, perform signal integrity checks (scope MOSI/MISO, check rise/fall times) and test for communication errors at the target speed. Update the overlay `spi-max-frequency` to match the tested runtime frequency.

3) Spec links and references

- L6470: See the STMicroelectronics L6470 datasheet; focus on the "Serial Interface" / "Digital Interface" sections for opcode timing and maximum SCLK frequency.
- Nucleo-H753ZI: See UM2407 and the board schematics for Arduino header pin mapping (PE11 = Arduino D5). The repo contains a local excerpt of UM2407 under `docs/`.

4) Quick action items

- Add DT node for `ihm02a1_vmot_switch` if you need explicit VMOT control from software.
- Add a line in the README referencing `docs/x-nucleo-ihm02a1_schematic.pdf` and this hardware_notes.md.

