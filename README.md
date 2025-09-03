# zephyr_h753zi (stm32h753zi_stepper)

This repository contains the `stm32h753zi_stepper` Zephyr application (Nucleo-H753ZI + X-NUCLEO-IHM02A1).

Important: Zephyr is tracked as a git submodule at `zephyr/`.

Quick start
1. Clone the repository with submodules:

```bash
git clone --recurse-submodules https://github.com/ch0t4nk/zephyr_h753zi.git
```

If you already cloned without submodules, initialize them:

```bash
git submodule update --init --recursive
```

Building locally (native_sim unit tests)

1. Install Zephyr SDK and Python tools as described in Zephyr docs.
2. Source Zephyr environment (adjust path depending on your current working directory):

From the repository root:

```bash
. ./zephyr/zephyr-env.sh
```

If you are inside the app folder (`apps/stm32h753zi_stepper`) use the relative path:

```bash
. ../../zephyr/zephyr-env.sh
```

Or source the absolute path to the script if you prefer:

```bash
. /full/path/to/zephyr/zephyr-env.sh
```

3. Build & run unit tests (native_sim):

```bash
west build -b native_sim apps/stm32h753zi_stepper/tests/unit -d build/native_unit --pristine
ctest --test-dir build/native_unit -V
```

Notes
- The repository previously contained a vendored Zephyr tree; it was removed and preserved as a backup during migration (backup removed).
- A GitHub Actions workflow builds native_sim and runs ctest on pushes to `main`.
- The application exposes a shell command `vmot` for safe VMOT control (requires `ihm02a1-vmot-switch` device-tree node on your board to be present and wired to a switching FET).
