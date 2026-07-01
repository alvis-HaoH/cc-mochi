from __future__ import annotations

import argparse
import json
import os
import signal
import socket
import threading
import time
from pathlib import Path
from typing import Any

from .protocol import NormalizedEvent, Severity, choose_display_event, normalize_event
from .serial_link import SerialLink, write_state_file
from .usage import UsageSnapshot, fetch_claude_usage, fetch_codex_usage


RUNTIME_DIR = Path.home() / ".cc-mochi"
SOCKET_PATH = RUNTIME_DIR / "daemon.sock"
STATE_PATH = RUNTIME_DIR / "state.json"

SESSION_TTL = 25.0
SUCCESS_TTL = 3.0
USAGE_TTL = 180.0


class MochiDaemon:
    def __init__(self, serial_link: SerialLink):
        self.serial = serial_link
        self.sessions: dict[tuple[str, str], tuple[NormalizedEvent, float]] = {}
        self.usage: dict[str, UsageSnapshot] = {}
        self.last_usage_refresh = 0.0
        self.lock = threading.RLock()
        self.running = True

    def start(self) -> None:
        RUNTIME_DIR.mkdir(parents=True, exist_ok=True)
        if SOCKET_PATH.exists():
            SOCKET_PATH.unlink()
        self.serial.open()
        self._send_idle()
        server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        server.bind(str(SOCKET_PATH))
        server.listen(12)
        SOCKET_PATH.chmod(0o600)
        threading.Thread(target=self._usage_loop, daemon=True).start()
        while self.running:
            try:
                conn, _ = server.accept()
            except OSError:
                break
            threading.Thread(target=self._handle_client, args=(conn,), daemon=True).start()

    def stop(self) -> None:
        self.running = False
        self.serial.close()
        try:
            SOCKET_PATH.unlink()
        except FileNotFoundError:
            pass

    def _handle_client(self, conn: socket.socket) -> None:
        with conn:
            raw = conn.recv(65536)
            if not raw:
                return
            try:
                payload = json.loads(raw.decode("utf-8"))
            except json.JSONDecodeError:
                return
            kind = payload.get("kind") or payload.get("type")
            if kind == "refresh_usage":
                self.refresh_usage(force=True)
            else:
                self.handle_hook(payload)
            try:
                conn.sendall(b'{"ok":true}\n')
            except OSError:
                pass

    def handle_hook(self, payload: dict[str, Any]) -> None:
        event = normalize_event(str(payload.get("source") or "agent"), payload.get("payload") or payload)
        now = time.time()
        with self.lock:
            key = (event.source, event.session_id)
            if event.state == "idle":
                self.sessions.pop(key, None)
            else:
                self.sessions[key] = (event, now)
            self._expire_locked(now)
            self._render_locked()
        if event.event in {"Stop", "SessionStart", "SessionEnd"}:
            threading.Thread(target=self.refresh_usage, kwargs={"force": False}, daemon=True).start()

    def _expire_locked(self, now: float) -> None:
        for key, (event, ts) in list(self.sessions.items()):
            ttl = SUCCESS_TTL if event.severity == Severity.success else SESSION_TTL
            if now - ts > ttl:
                self.sessions.pop(key, None)

    def _render_locked(self) -> None:
        events = [value[0] for value in self.sessions.values()]
        selected = choose_display_event(events)
        if selected is None:
            self._send_idle()
            return
        active = len(events)
        if active > 1 and selected.severity < Severity.permission:
            state = "multi"
            label = f"{active} active"
        else:
            state = selected.state
            label = selected.label
        self._send(
            {
                "type": "state",
                "source": selected.source,
                "state": state,
                "label": label,
                "active": active,
            }
        )

    def _send_idle(self) -> None:
        self._send({"type": "state", "source": "cc", "state": "sleeping", "label": "idle", "active": 0})

    def _send(self, payload: dict[str, Any]) -> None:
        try:
            self.serial.send(payload)
            write_state_file(STATE_PATH, payload)
        except Exception as exc:
            write_state_file(STATE_PATH, {"type": "error", "message": str(exc), "payload": payload})

    def _usage_loop(self) -> None:
        while self.running:
            time.sleep(30)
            with self.lock:
                idle = not self.sessions
            if idle:
                self.refresh_usage(force=False)

    def refresh_usage(self, force: bool = False) -> None:
        now = time.time()
        if not force and now - self.last_usage_refresh < USAGE_TTL:
            return
        self.last_usage_refresh = now
        snapshots = [fetch_codex_usage(), fetch_claude_usage()]
        with self.lock:
            for snapshot in snapshots:
                self.usage[snapshot.provider] = snapshot
                if not self.sessions:
                    self._send(snapshot.to_display())
                    time.sleep(1.0)
            if not self.sessions:
                alert = self._usage_alert(snapshots)
                if alert is not None:
                    self._send(alert)
                    time.sleep(1.0)
                self._send_idle()

    @staticmethod
    def _usage_alert(snapshots: list[UsageSnapshot]) -> dict[str, Any] | None:
        available = [item for item in snapshots if not item.unavailable and item.primary is not None]
        if not available:
            return None
        worst = max(available, key=lambda item: item.primary or 0)
        primary = worst.primary or 0
        if primary >= 95:
            state = "usage_critical"
        elif primary >= 80:
            state = "usage_low"
        else:
            return None
        return {
            "type": "state",
            "source": worst.provider,
            "state": state,
            "label": f"reset {worst.reset or '--'}",
            "active": 0,
        }


def send_to_daemon(payload: dict[str, Any], timeout: float = 0.2) -> bool:
    try:
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        client.settimeout(timeout)
        client.connect(str(SOCKET_PATH))
        client.sendall(json.dumps(payload, separators=(",", ":")).encode("utf-8"))
        client.close()
        return True
    except OSError:
        return False


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="cc-mochi bridge daemon")
    parser.add_argument("--port", default=None, help="Serial port, defaults to auto-discovery.")
    parser.add_argument("--baud", default=115200, type=int)
    parser.add_argument("--dry-run", action="store_true", help="Print serial JSON Lines instead of writing a device.")
    args = parser.parse_args(argv)

    daemon = MochiDaemon(SerialLink(args.port, args.baud, dry_run=args.dry_run))

    def _stop(_signum: int, _frame: Any) -> None:
        daemon.stop()

    signal.signal(signal.SIGINT, _stop)
    signal.signal(signal.SIGTERM, _stop)
    daemon.start()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
