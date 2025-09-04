# zephyr_h753zi (stm32h753zi_stepper)

This repository contains the `stm32h753zi_stepper` Zephyr application (Nucleo-H753ZI + X-NUCLEO-IHM02A1).

Important: Zephyr is tracked as a git submodule at `zephyr/`.

## Current status

- native_sim: builds with warnings only (unit tests pass via ctest)
- nucleo_h753zi: build passes

## Key changes

- LittleFS persistence with sentinel in `apps/stm32h753zi_stepper/src/persist.c`
	- Mounts fixed partition `storage` at `/lfs`
	- Creates `/lfs/.mounted` sentinel; formats with `fs_mkfs()` on first use when enabled
- Filesystem mkfs enabled via `CONFIG_APP_LINK_WITH_FS=y` and `CONFIG_FILE_SYSTEM_MKFS=y`
- Shield wiring inlined in `apps/stm32h753zi_stepper/nucleo_h753zi.overlay` (no separate shield DTS needed)
- Reset GPIO fallback in `apps/stm32h753zi_stepper/src/l6470.c` (uses PE14 on Nucleo-H753ZI if overlay node absent)

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

Alternatively, in VS Code use Tasks:
- Build native_sim unit tests
- Rebuild native_sim unit tests (with ctest)

## Flashing (Nucleo-H753ZI via OpenOCD)

- VS Code task: "Zephyr: Flash stm32h753zi_stepper (OpenOCD)"
- Or from the repository root after a board build:

```bash
west flash --skip-rebuild --build-dir build/h753zi --runner openocd
```

## Smoke CLI and logs

- Zephyr shell provides a simple VMOT control under `vmot`:
	- `vmot on | off | toggle | status`
- Logging is enabled on UART and RTT; view via your serial terminal or RTT viewer.
- VMOT control requires `ihm02a1_vmot_switch` node present in the overlay; otherwise commands print a helpful message and return `-ENOTSUP`.

## Tests and Twister

- Today: build native_sim tests and run with `ctest` (see above or VS Code tasks).
- Next: wire ztest/Twister (add `testcase.yaml` under `apps/stm32h753zi_stepper/tests/` and a Twister invocation) so CI and local `twister` runs cover suites automatically.

## Known warnings

- On native_sim, enabling Settings or FS can emit warnings due to missing flash/partitions; these are harmless for host tests. On-target builds provide the required flash map and partition via overlay.

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
