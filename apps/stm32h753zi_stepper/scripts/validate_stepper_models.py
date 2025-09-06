#!/usr/bin/env python3
"""
Validate stepper_models.yml for required fields, types, and safe ranges.
Exits non-zero on any validation error so CMake fails early.
"""
from __future__ import annotations
import sys
import math
import yaml
from pathlib import Path


REQ_FIELDS = {
    "name": str,
    "steps_per_rev": int,
    "max_speed": int,           # steps/sec
    "max_current": int,         # mA
    "max_microstep": int,
    "rated_voltage": (int, float),
    "holding_torque": (int, float),
    # L6470 related (all optional but validated if present)
    # use_microstep, kval_*, ocd_thresh_ma, stall_thresh_ma, acc_sps2, dec_sps2
}


def err(msg: str) -> None:
    print(f"[stepper-models] ERROR: {msg}", file=sys.stderr)


def warn(msg: str) -> None:
    print(f"[stepper-models] WARN: {msg}")


def validate_model(idx: int, m: dict) -> list[str]:
    errors: list[str] = []

    # Required fields and types
    for k, typ in REQ_FIELDS.items():
        if k not in m:
            errors.append(f"model[{idx}] missing required field '{k}'")
            continue
        if not isinstance(m[k], typ):
            errors.append(f"model[{idx}].{k} has wrong type {type(m[k]).__name__}, expected {typ}")

    name = m.get("name", f"#{idx}")

    # Basic value ranges
    spr = m.get("steps_per_rev", 200)
    if spr not in (48, 96, 100, 200, 400, 800):
        # Allow any positive, but nudge to common values
        if not (1 <= spr <= 20000):
            errors.append(f"{name}: steps_per_rev must be within 1..20000 (got {spr})")

    max_speed = m.get("max_speed", 1000)
    if not (1 <= max_speed <= 100000):
        errors.append(f"{name}: max_speed (steps/s) out of range: {max_speed}")

    max_current = m.get("max_current", 1000)
    if not (100 <= max_current <= 6000):
        errors.append(f"{name}: max_current (mA) should be 100..6000 (got {max_current})")

    max_micro = m.get("max_microstep", 16)
    if max_micro not in (1, 2, 4, 8, 16, 32, 64, 128):
        errors.append(f"{name}: max_microstep must be power-of-two up to 128 (got {max_micro})")

    use_micro = m.get("use_microstep", max_micro)
    if use_micro not in (1, 2, 4, 8, 16, 32, 64, 128):
        errors.append(f"{name}: use_microstep must be power-of-two up to 128 (got {use_micro})")
    if use_micro > max_micro:
        errors.append(f"{name}: use_microstep {use_micro} exceeds max_microstep {max_micro}")

    rated_v = m.get("rated_voltage", 0.0)
    if not (0.0 <= float(rated_v) <= 100.0):
        errors.append(f"{name}: rated_voltage out of range 0..100 V (got {rated_v})")

    hold_t = m.get("holding_torque", 0.0)
    if not (0.0 <= float(hold_t) <= 20.0):
        errors.append(f"{name}: holding_torque out of range 0..20 Nm (got {hold_t})")

    # Optional L6470-related fields and sanity ranges
    for kval_key in ("kval_run", "kval_acc", "kval_dec", "kval_hold"):
        if kval_key in m:
            kval = int(m[kval_key])
            if not (0 <= kval <= 255):
                errors.append(f"{name}: {kval_key} must be 0..255 (got {kval})")

    if "ocd_thresh_ma" in m:
        ocd = int(m["ocd_thresh_ma"])
        if not (375 <= ocd <= 6000):
            errors.append(f"{name}: ocd_thresh_ma should be 375..6000 (got {ocd})")

    if "stall_thresh_ma" in m:
        sth = int(m["stall_thresh_ma"])
        if not (0 <= sth <= 4000):
            errors.append(f"{name}: stall_thresh_ma should be 0..4000 (got {sth})")

    for a_key in ("acc_sps2", "dec_sps2"):
        if a_key in m:
            val = int(m[a_key])
            if not (10 <= val <= 200000):
                errors.append(f"{name}: {a_key} (steps/s^2) out of range 10..200000 (got {val})")

    # L6470 register theoretical bounds checks (non-fatal warnings)
    # MAX_SPEED reg encodes speed step per s with scale ~0.238 steps/s per LSB
    # Upper bound of register is 0x3FF -> ~976.5 steps/s (per full-step); but effective speed also
    # depends on microstepping; enforce a conservative cap when microstepping is high.
    # We warn if max_speed is far above typical L6470 practical range.
    if max_speed > 100000:
        warn(f"{name}: max_speed {max_speed} steps/s is unusually high for L6470")

    return errors


def main() -> int:
    if len(sys.argv) != 2:
        print("Usage: validate_stepper_models.py <stepper_models.yml>")
        return 2
    yml = Path(sys.argv[1])
    if not yml.exists():
        err(f"YAML file not found: {yml}")
        return 2
    try:
        data = yaml.safe_load(yml.read_text())
    except Exception as e:
        err(f"Failed to parse YAML: {e}")
        return 2

    if not isinstance(data, list) or not data:
        err("Top-level YAML must be a non-empty list of model objects")
        return 2

    all_errors: list[str] = []
    names = set()
    for i, m in enumerate(data):
        if not isinstance(m, dict):
            all_errors.append(f"model[{i}] is not a mapping")
            continue
        errs = validate_model(i, m)
        all_errors.extend(errs)
        nm = m.get("name")
        if isinstance(nm, str):
            if nm in names:
                all_errors.append(f"Duplicate model name: {nm}")
            names.add(nm)

    if all_errors:
        for e in all_errors:
            err(e)
        print(f"[stepper-models] Validation failed with {len(all_errors)} error(s)")
        return 1

    print(f"[stepper-models] Validation OK: {len(data)} model(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
