from __future__ import annotations

import glob
import json
import os
import subprocess
import time
from pathlib import Path
from typing import Any


DEFAULT_BAUD = 115200


def discover_port() -> str | None:
    patterns = (
        "/dev/cu.usbmodem*",
        "/dev/cu.usbserial*",
        "/dev/cu.SLAB_USBtoUART*",
        "/dev/ttyACM*",
        "/dev/ttyUSB*",
    )
    for pattern in patterns:
        matches = sorted(glob.glob(pattern))
        if matches:
            return matches[0]
    return None


class SerialLink:
    def __init__(self, port: str | None = None, baud: int = DEFAULT_BAUD, dry_run: bool = False):
        self.port = port or discover_port()
        self.baud = baud
        self.dry_run = dry_run
        self._pyserial = None
        self._fh = None

    def open(self) -> None:
        if self.dry_run:
            return
        if not self.port:
            raise RuntimeError("No cc-mochi serial port found.")
        try:
            import serial  # type: ignore

            self._pyserial = serial.Serial(self.port, self.baud, timeout=0, write_timeout=0.2)
            time.sleep(1.2)
            return
        except ImportError:
            self._pyserial = None

        if os.uname().sysname == "Darwin":
            subprocess.run(["stty", "-f", self.port, str(self.baud), "raw", "-echo"], check=False)
        else:
            subprocess.run(["stty", "-F", self.port, str(self.baud), "raw", "-echo"], check=False)
        self._fh = open(self.port, "ab", buffering=0)
        time.sleep(1.2)

    def close(self) -> None:
        if self._pyserial is not None:
            self._pyserial.close()
            self._pyserial = None
        if self._fh is not None:
            self._fh.close()
            self._fh = None

    def send(self, payload: dict[str, Any]) -> None:
        line = json.dumps(payload, separators=(",", ":"), ensure_ascii=True).encode("utf-8") + b"\n"
        if self.dry_run:
            print(line.decode("utf-8"), end="")
            return
        if self._pyserial is None and self._fh is None:
            self.open()
        if self._pyserial is not None:
            self._pyserial.write(line)
            return
        if self._fh is not None:
            self._fh.write(line)


def write_state_file(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")
