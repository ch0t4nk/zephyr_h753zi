#!/usr/bin/env python3
"""
Flash and on-target persistence smoke test for stm32h753zi_stepper.

Workflow:
- west flash (runner configurable)
- Open serial console and run:
  - stepper persist smoke
  - stepper model set 0 <model>
  - stepper persist dump
  - kernel reboot
  - stepper model show 0
  - stepper persist dump
- Write all output to a logfile for inspection.

Requirements:
- west available in PATH and Zephyr env loaded (or run west with absolute paths)
- pyserial installed for serial access (pip install pyserial)
- Board serial device path (e.g., /dev/ttyACM0)
"""
from __future__ import annotations
import argparse
import os
import sys
import time
import subprocess
from typing import Optional

LOG_TS_FMT = "%Y-%m-%d %H:%M:%S"


def log(logf, msg: str):
    ts = time.strftime(LOG_TS_FMT)
    line = f"[{ts}] {msg}\n"
    logf.write(line)
    logf.flush()
    print(line, end="")


class SerialSession:
    def __init__(self, port: str, baud: int, timeout: float, logf):
        try:
            import serial  # type: ignore
        except Exception as e:
            raise RuntimeError(
                "pyserial is required for serial access. Install with 'pip install pyserial'"
            ) from e
        self._serial_mod = serial
        self.port = port
        self.baud = baud
        self.timeout = timeout
        self.logf = logf
        self.ser: Optional[serial.Serial] = None  # type: ignore

    def open(self):
        self.ser = self._serial_mod.Serial(self.port, self.baud, timeout=0.1)
        # Give the port a moment to settle
        time.sleep(0.2)
        # Drain any pending data
        self._drain(0.5)

    def close(self):
        if self.ser:
            self.ser.close()
            self.ser = None

    def _drain(self, duration_s: float):
        if not self.ser:
            return ""
        end = time.time() + duration_s
        buf = bytearray()
        while time.time() < end:
            n = self.ser.in_waiting
            if n:
                data = self.ser.read(n)
                if data:
                    buf.extend(data)
                    try:
                        s = data.decode(errors="ignore")
                    except Exception:
                        s = repr(data)
                    self._log_serial(s)
            else:
                time.sleep(0.02)
        return buf.decode(errors="ignore") if buf else ""

    def _log_serial(self, s: str):
        for line in s.splitlines():
            log(self.logf, f"SER: {line}")

    @staticmethod
    def _is_prompt(line: str) -> bool:
        # Common Zephyr shell prompts: 'uart:~$ ', 'shell:~$ ', possibly just '~$ '
        return line.strip().endswith("~$") or "~$ " in line

    def wait_for_prompt(self, timeout_s: float) -> bool:
        if not self.ser:
            return False
        end = time.time() + timeout_s
        line = ""
        while time.time() < end:
            # Read chunk-wise
            n = self.ser.in_waiting
            if n:
                data = self.ser.read(n)
                try:
                    s = data.decode(errors="ignore")
                except Exception:
                    s = repr(data)
                self._log_serial(s)
                line += s
                # Split into lines and check last seen
                for ln in line.splitlines():
                    if self._is_prompt(ln):
                        return True
                # keep tail only
                line = line[-256:]
            else:
                time.sleep(0.02)
        return False

    def send_cmd(self, cmd: str, wait_prompt: float = 3.0) -> bool:
        if not self.ser:
            return False
        log(self.logf, f"CMD: {cmd}")
        self.ser.write((cmd + "\r\n").encode())
        self.ser.flush()
        return self.wait_for_prompt(wait_prompt)


def run_subprocess(cmd: list[str], logf, cwd: Optional[str] = None) -> int:
    log(logf, f"RUN: {' '.join(cmd)}")
    proc = subprocess.Popen(cmd, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    assert proc.stdout
    for line in proc.stdout:
        log(logf, f"OUT: {line.rstrip()} ")
    proc.wait()
    log(logf, f"RC: {proc.returncode}")
    return proc.returncode


def find_default_serial() -> Optional[str]:
    # Try common ST-LINK VCP device names
    candidates = [
        "/dev/ttyACM0",
        "/dev/ttyACM1",
        "/dev/ttyUSB0",
        "/dev/ttyUSB1",
    ]
    for c in candidates:
        if os.path.exists(c):
            return c
    return None


def main():
    ap = argparse.ArgumentParser(description="Flash and run on-target persistence check")
    ap.add_argument("--build-dir", default=os.path.expanduser("~/zephyr-dev/build/h753zi"), help="Path to west build directory")
    ap.add_argument("--runner", default="openocd", choices=["openocd", "stlink"], help="west flash runner")
    ap.add_argument("--openocd-arg", action="append", default=[], help="Extra -c arg for openocd, e.g. adapter speed 400")
    ap.add_argument("--no-flash", action="store_true", help="Skip flashing; only run shell steps")
    ap.add_argument("--serial", default=None, help="Serial device for Zephyr shell (default: auto-detect)")
    ap.add_argument("--baud", type=int, default=115200, help="Serial baud rate")
    ap.add_argument("--prompt-timeout", type=float, default=8.0, help="Seconds to wait for shell prompt after connect/reboot")
    ap.add_argument("--model", default="17HS3001-20B", help="Model name to set on axis 0 for the check")
    ap.add_argument("--logfile", default="flash_try.log", help="Path to write combined log")
    args = ap.parse_args()

    # Open log file
    with open(args.logfile, "w", buffering=1) as logf:
        log(logf, "Flash+Try starting")

        # Flash
        if not args.no_flash:
            cmd = ["west", "flash", "--skip-rebuild", "--build-dir", args.build_dir, "-r", args.runner]
            if args.runner == "openocd" and args.openocd_arg:
                cmd += ["--"]
                for a in args.openocd_arg:
                    cmd += ["-c", a]
            rc = run_subprocess(cmd, logf)
            if rc != 0:
                # Some OpenOCD versions return non-zero after reset even when programming succeeded.
                # Continue to serial stage to verify runtime behavior.
                log(logf, "WARN: west flash returned non-zero; continuing to serial anyway")
        else:
            log(logf, "Skipping flash per --no-flash")

        # Serial connect
        port = args.serial or find_default_serial()
        if not port:
            log(logf, "ERROR: Could not auto-detect serial device. Use --serial /dev/ttyACM0")
            return 2

        sess = SerialSession(port, args.baud, args.prompt_timeout, logf)
        try:
            log(logf, f"Opening serial {port} @ {args.baud}")
            sess.open()
            if not sess.wait_for_prompt(args.prompt_timeout):
                log(logf, "WARN: No prompt detected after connect; continuing")

            # Kernel diagnostics first: stacks and threads
            sess.send_cmd("kernel stacks", wait_prompt=args.prompt_timeout)
            sess.send_cmd("kernel threads", wait_prompt=args.prompt_timeout)

            # 1) Smoke (mount+file+active)
            sess.send_cmd("stepper persist smoke", wait_prompt=args.prompt_timeout)

            # 2) Set model and dump settings
            sess.send_cmd(f"stepper model set 0 {args.model}", wait_prompt=args.prompt_timeout)
            sess.send_cmd("stepper persist dump", wait_prompt=args.prompt_timeout)

            # 3) Reboot and verify
            # Try standard Zephyr shell reboot command
            reboot_ok = sess.send_cmd("kernel reboot", wait_prompt=1.0)
            if not reboot_ok:
                # Fallback command name (older shells): 'reboot'
                sess.send_cmd("reboot", wait_prompt=1.0)
            # After reboot, the prompt will disappear briefly; wait and re-sync
            time.sleep(1.5)
            if not sess.wait_for_prompt(args.prompt_timeout):
                log(logf, "WARN: No prompt after reboot; waiting a bit more")
                time.sleep(2.0)
                sess.wait_for_prompt(args.prompt_timeout)

            sess.send_cmd("stepper model show 0", wait_prompt=args.prompt_timeout)
            sess.send_cmd("stepper persist dump", wait_prompt=args.prompt_timeout)

            log(logf, "SUCCESS: Flash+Try sequence completed")
            return 0
        finally:
            sess.close()


if __name__ == "__main__":
    sys.exit(main())
