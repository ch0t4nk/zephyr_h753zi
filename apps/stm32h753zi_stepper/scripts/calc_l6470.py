#!/usr/bin/env python3
"""
Compute L6470 register encodings from a stepper model in stepper_models.yml.
This is a helper for traceability and bench tuning. It does not affect builds.

Usage:
  python3 calc_l6470.py [--yaml apps/stm32h753zi_stepper/stepper_models.yml] [--model 0|NAME]

Prints: step_mode nibble, KVAL_RUN, OCD_TH steps, ACC/DEC reg values, MAX_SPEED reg, STALL_TH steps.
"""
from __future__ import annotations
import argparse
import math
import sys
from pathlib import Path
import yaml

DEFAULT_YAML = Path(__file__).resolve().parents[1] / 'stepper_models.yml'

MICROSTEP_TO_NIBBLE = {
    1: 0x00,
    2: 0x01,
    4: 0x02,
    8: 0x03,
    16: 0x04,
    32: 0x05,
    64: 0x06,
    128: 0x07,
}

# L6470 unit scales
ACC_DEC_LSB_SPS2 = 14.55      # steps/s^2 per LSB (12-bit register)
MAX_SPEED_LSB_SPS = 0.238     # steps/s per LSB (10-bit register)
STALL_TH_LSB_MA   = 31.25     # mA per LSB (7-bit register)
OCD_TH_LSB_MA     = 375       # mA per LSB (4-bit value 1..15)


def load_models(yaml_path: Path):
    with open(yaml_path, 'r', encoding='utf-8') as f:
        return yaml.safe_load(f)


def pick_model(models, selector: str|int):
    if isinstance(selector, int):
        return models[selector]
    # try by name field exact
    for m in models:
        if m.get('name') == selector or m.get('sku') == selector:
            return m
    # try case-insensitive name contains
    for m in models:
        if selector.lower() in str(m.get('name', '')).lower():
            return m
    raise KeyError(f"Model '{selector}' not found")


def encode_acc_dec_sps2(sps2: float) -> int:
    raw = int(round(sps2 / ACC_DEC_LSB_SPS2))
    return max(0, min(0x0FFF, raw))


def encode_max_speed_sps(sps: float) -> int:
    raw = int(math.floor(sps / MAX_SPEED_LSB_SPS))
    return max(0, min(0x03FF, raw))


def encode_stall_th_ma(ma: float) -> int:
    # Round to nearest step, clamp 1..0x7F per datasheet
    raw = int(round(ma / STALL_TH_LSB_MA))
    raw = max(1, min(0x7F, raw))
    return raw


def encode_ocd_th_ma(ma: float) -> int:
    raw = int(round(ma / OCD_TH_LSB_MA))
    raw = max(1, min(0x0F, raw))
    return raw


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument('--yaml', type=Path, default=DEFAULT_YAML)
    ap.add_argument('--model', default='0', help='Model index (0..) or name/sku')
    args = ap.parse_args(argv)

    models = load_models(args.yaml)
    selector = int(args.model) if args.model.isdigit() else args.model
    m = pick_model(models, selector)

    ms = int(m.get('use_microstep', m.get('max_microstep', 16)))
    step_mode_nibble = MICROSTEP_TO_NIBBLE.get(ms, 0x00)

    kval_run = int(m.get('kval_run', 0))
    ocd_steps = encode_ocd_th_ma(float(m.get('ocd_thresh_ma', 375)))
    acc_reg = encode_acc_dec_sps2(float(m.get('acc_sps2', 0)))
    dec_reg = encode_acc_dec_sps2(float(m.get('dec_sps2', 0)))
    maxs_reg = encode_max_speed_sps(float(m.get('max_speed', 0)))
    stall_steps = encode_stall_th_ma(float(m.get('stall_thresh_ma', 31.25)))

    print(f"Model: {m.get('name')} (sku={m.get('sku')})")
    print(f"  step_mode_nibble: {step_mode_nibble} (0x{step_mode_nibble:02X}) for 1/{ms}")
    print(f"  kval_run: {kval_run}")
    print(f"  ocd_th_steps: {ocd_steps}")
    print(f"  acc_reg: {acc_reg} (0x{acc_reg:04X})")
    print(f"  dec_reg: {dec_reg} (0x{dec_reg:04X})")
    print(f"  max_speed_reg: {maxs_reg} (0x{maxs_reg:04X})")
    print(f"  stall_th_steps: {stall_steps} (0x{stall_steps:02X})")

    # Optional: derived coil time constant if data present
    r = m.get('phase_resistance_ohm')
    l_mh = m.get('phase_inductance_mH')
    if r is not None and l_mh is not None:
        tau_s = (float(l_mh) / 1000.0) / float(r)
        print(f"  tau (L/R): {tau_s*1000:.2f} ms")

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
