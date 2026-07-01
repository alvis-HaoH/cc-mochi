from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from .daemon import RUNTIME_DIR, send_to_daemon


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="cc-mochi hook forwarder")
    parser.add_argument("--source", required=True, choices=("claude", "codex"))
    parser.add_argument("--event", default="")
    args = parser.parse_args(argv)

    raw = sys.stdin.read()
    try:
        payload: dict[str, Any] = json.loads(raw) if raw.strip() else {}
    except json.JSONDecodeError:
        payload = {"raw": raw[:200]}
    if args.event and "hook_event_name" not in payload and "event" not in payload:
        payload["hook_event_name"] = args.event

    message = {"kind": "hook", "source": args.source, "payload": payload}
    if not send_to_daemon(message):
        RUNTIME_DIR.mkdir(parents=True, exist_ok=True)
        spool = RUNTIME_DIR / "missed-hooks.jsonl"
        with spool.open("a", encoding="utf-8") as fh:
            fh.write(json.dumps(message, separators=(",", ":"), ensure_ascii=True) + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
