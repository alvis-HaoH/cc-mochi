from __future__ import annotations

import json
import os
import re
import select
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any
from urllib.request import Request, urlopen


@dataclass
class UsageSnapshot:
    provider: str
    primary: float | None = None
    secondary: float | None = None
    reset: str | None = None
    badge: str | None = None
    unavailable: bool = False
    stale: bool = False
    source: str = ""

    def to_display(self) -> dict[str, Any]:
        return {
            "type": "usage",
            "provider": self.provider,
            "primary": int(round(self.primary or 0)),
            "secondary": int(round(self.secondary or 0)),
            "reset": self.reset or "",
            "badge": self.badge or "",
            "unavailable": bool(self.unavailable),
            "stale": bool(self.stale),
        }


def _reset_label(epoch_value: Any) -> str:
    try:
        epoch = float(epoch_value)
    except (TypeError, ValueError):
        return ""
    delta = max(0, int(epoch - time.time()))
    hours, rem = divmod(delta, 3600)
    mins = rem // 60
    if hours:
        return f"{hours}h{mins:02d}m"
    return f"{mins}m"


def _extract_json_lines(text: str) -> list[dict[str, Any]]:
    out: list[dict[str, Any]] = []
    for line in text.splitlines():
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            out.append(json.loads(line))
        except json.JSONDecodeError:
            continue
    return out


def fetch_codex_usage(timeout: float = 8.0) -> UsageSnapshot:
    request_lines = [
        {
            "jsonrpc": "2.0",
            "id": 1,
            "method": "initialize",
            "params": {"clientInfo": {"name": "cc-mochi", "version": "0.1.0"}},
        },
        {"jsonrpc": "2.0", "method": "initialized", "params": {}},
        {"jsonrpc": "2.0", "id": 2, "method": "account/rateLimits/read", "params": {}},
    ]
    body = "\n".join(json.dumps(item) for item in request_lines) + "\n"
    proc: subprocess.Popen[str] | None = None
    try:
        proc = subprocess.Popen(
            ["codex", "-s", "read-only", "-a", "untrusted", "app-server"],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        assert proc.stdin is not None
        proc.stdin.write(body)
        proc.stdin.flush()
        deadline = time.time() + timeout
        lines: list[str] = []
        while time.time() < deadline:
            streams = [s for s in (proc.stdout, proc.stderr) if s is not None]
            ready, _, _ = select.select(streams, [], [], 0.2)
            for stream in ready:
                line = stream.readline()
                if not line:
                    continue
                lines.append(line)
                parsed = _codex_snapshot_from_lines(lines)
                if parsed is not None:
                    return parsed
        return UsageSnapshot("codex", unavailable=True, stale=True, source="codex rpc timeout")
    except Exception as exc:
        return UsageSnapshot("codex", unavailable=True, stale=True, source=f"codex rpc: {exc}")
    finally:
        if proc is not None and proc.poll() is None:
            proc.terminate()
            try:
                proc.wait(timeout=1)
            except subprocess.TimeoutExpired:
                proc.kill()


def _codex_snapshot_from_lines(lines: list[str]) -> UsageSnapshot | None:
    for item in _extract_json_lines("".join(lines)):
        if item.get("id") != 2:
            continue
        result = item.get("result") or {}
        result = result.get("rateLimits") or result
        primary = result.get("primary") or {}
        secondary = result.get("secondary") or {}
        credits = result.get("credits") or {}
        badge = str(result.get("planType") or "")
        if credits.get("hasCredits"):
            badge = "credits"
        return UsageSnapshot(
            "codex",
            primary=_num(primary.get("usedPercent")),
            secondary=_num(secondary.get("usedPercent")),
            reset=_reset_label(primary.get("resetsAt")),
            badge=badge,
            source="codex app-server",
        )
    return None


def _num(value: Any) -> float | None:
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def claude_auth_mode(env: dict[str, str] | None = None) -> str:
    env = env or claude_effective_env()
    base_url = env.get("ANTHROPIC_BASE_URL") or env.get("CLAUDE_CODE_ANTHROPIC_BASE_URL")
    token = env.get("ANTHROPIC_AUTH_TOKEN") or env.get("ANTHROPIC_API_KEY")
    if base_url and "anthropic.com" not in base_url:
        return "custom-api"
    if token:
        return "api-key"
    return "subscription"


def claude_effective_env() -> dict[str, str]:
    merged = dict(os.environ)
    settings = Path.home() / ".claude" / "settings.json"
    if settings.exists():
        try:
            data = json.loads(settings.read_text(encoding="utf-8"))
            configured = data.get("env") or {}
            if isinstance(configured, dict):
                for key, value in configured.items():
                    if isinstance(value, str):
                        merged.setdefault(key, value)
        except Exception:
            pass
    return merged


def _load_claude_oauth_token() -> str | None:
    candidates = [
        Path.home() / ".claude" / ".credentials.json",
        Path.home() / ".claude.json",
    ]
    for path in candidates:
        if not path.exists():
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except Exception:
            continue
        for key in ("accessToken", "access_token", "oauthAccessToken"):
            value = data.get(key)
            if isinstance(value, str) and value:
                return value
    return None


def fetch_claude_oauth_usage(timeout: float = 8.0) -> UsageSnapshot:
    token = _load_claude_oauth_token()
    if not token:
        return UsageSnapshot("claude", unavailable=True, stale=True, source="oauth token unavailable")
    req = Request(
        "https://api.anthropic.com/api/oauth/usage",
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/json",
            "anthropic-beta": "oauth-2025-04-20",
            "User-Agent": "cc-mochi/0.1.0",
        },
    )
    try:
        with urlopen(req, timeout=timeout) as resp:
            data = json.loads(resp.read().decode("utf-8"))
    except Exception as exc:
        return UsageSnapshot("claude", unavailable=True, stale=True, source=f"oauth: {exc}")
    five = data.get("five_hour") or {}
    week = data.get("seven_day") or {}
    model = data.get("seven_day_sonnet") or data.get("seven_day_opus") or {}
    badge = "sonnet" if data.get("seven_day_sonnet") else ("opus" if data.get("seven_day_opus") else "")
    return UsageSnapshot(
        "claude",
        primary=_num(five.get("utilization")),
        secondary=_num(week.get("utilization") or model.get("utilization")),
        reset=_iso_delta(five.get("resets_at")),
        badge=badge,
        source="claude oauth",
    )


def _iso_delta(value: Any) -> str:
    if not isinstance(value, str) or not value:
        return ""
    try:
        from datetime import datetime, timezone

        dt = datetime.fromisoformat(value.replace("Z", "+00:00"))
        return _reset_label(dt.astimezone(timezone.utc).timestamp())
    except Exception:
        return ""


def fetch_claude_cli_usage(timeout: float = 18.0) -> UsageSnapshot:
    try:
        proc = subprocess.run(
            ["claude", "/usage"],
            input="\n",
            text=True,
            capture_output=True,
            timeout=timeout,
            check=False,
        )
    except Exception as exc:
        return UsageSnapshot("claude", unavailable=True, stale=True, source=f"cli: {exc}")
    text = strip_ansi(proc.stdout + "\n" + proc.stderr)
    session_left = _percent_after_label(text, "Current session")
    weekly_left = _percent_after_label(text, "Current week")
    if session_left is None:
        return UsageSnapshot("claude", unavailable=True, stale=True, source="cli parse failed")
    return UsageSnapshot(
        "claude",
        primary=max(0, min(100, 100 - session_left)),
        secondary=None if weekly_left is None else max(0, min(100, 100 - weekly_left)),
        reset=_reset_text_after_label(text, "Current session"),
        badge="cli",
        source="claude /usage",
    )


def strip_ansi(text: str) -> str:
    return re.sub(r"\x1b\[[0-9;?]*[ -/]*[@-~]", "", text)


def _percent_after_label(text: str, label: str) -> float | None:
    lines = text.splitlines()
    for idx, line in enumerate(lines):
        if label.lower() not in line.lower():
            continue
        window = "\n".join(lines[idx : idx + 10])
        match = re.search(r"([0-9]{1,3}(?:\.[0-9]+)?)\s*%", window)
        if match:
            return float(match.group(1))
    return None


def _reset_text_after_label(text: str, label: str) -> str:
    lines = text.splitlines()
    for idx, line in enumerate(lines):
        if label.lower() not in line.lower():
            continue
        window = " ".join(lines[idx : idx + 6])
        match = re.search(r"reset[s]?\s+(?:at|in)?\s*([A-Za-z0-9: ]{2,12})", window, re.I)
        if match:
            return match.group(1).strip()[:12]
    return ""


def fetch_claude_usage(env: dict[str, str] | None = None) -> UsageSnapshot:
    env = env or claude_effective_env()
    mode = claude_auth_mode(env)
    if mode != "subscription":
        return UsageSnapshot("claude", unavailable=True, stale=False, badge="local", source=mode)
    oauth = fetch_claude_oauth_usage()
    if not oauth.unavailable:
        return oauth
    return fetch_claude_cli_usage()
