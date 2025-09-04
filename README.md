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
west build -b native_sim apps/stm32h753zi_stepper/tests/unit -d build/native_unit -p always
ctest --test-dir build/native_unit -V
```

Notes
- The repository previously contained a vendored Zephyr tree; it was removed and preserved as a backup during migration (backup removed).
- A GitHub Actions workflow builds native_sim and runs ctest on pushes to `main`.
- The application exposes a shell command `vmot` for safe VMOT control (requires `ihm02a1-vmot-switch` device-tree node on your board to be present and wired to a switching FET).

## Paths and environment

- Do not hardcode usernames or absolute paths. Prefer `$HOME` or `~`.
- This workspace lives at `$HOME/zephyr-dev` and Zephyr SDK is at `$HOME/zephyr-sdk-0.17.2` (adjust if you installed elsewhere).
- Zephyr is a submodule at `$HOME/zephyr-dev/zephyr`.

Key locations:
- Topdir: `$HOME/zephyr-dev`
- App: `$HOME/zephyr-dev/apps/stm32h753zi_stepper`
- Zephyr env script: `$HOME/zephyr-dev/zephyr/zephyr-env.sh`
- Native unit build dir: `$HOME/zephyr-dev/build/native_unit`

Environment tips:
- West manages ZEPHYR_BASE automatically; you usually do not need to export it.
- If a build ever fails with a path like `.../zephyr/zephyr/cmake/pristine.cmake`, clear any override and use the portable pristine option:

```bash
west config -d zephyr.base   # remove any pinned zephyr.base
west build -p always ...     # instead of --pristine
```

Board builds (examples):

```bash
# From $HOME/zephyr-dev
. ./zephyr/zephyr-env.sh
west build -b nucleo_h753zi apps/stm32h753zi_stepper -d build/h753zi -p always
west flash --skip-rebuild --build-dir build/h753zi --runner openocd
```
