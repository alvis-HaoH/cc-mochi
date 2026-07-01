from __future__ import annotations

import argparse
import json
import shutil
import sys
import time
from pathlib import Path
from typing import Any


ROOT = Path(__file__).resolve().parents[1]
PY = sys.executable


CLAUDE_EVENTS = [
    "SessionStart",
    "UserPromptSubmit",
    "UserPromptExpansion",
    "PreToolUse",
    "PermissionRequest",
    "PermissionDenied",
    "PostToolUse",
    "PostToolUseFailure",
    "SubagentStart",
    "SubagentStop",
    "TaskCreated",
    "TaskCompleted",
    "MessageDisplay",
    "Stop",
    "StopFailure",
    "PreCompact",
    "PostCompact",
    "Notification",
    "SessionEnd",
]

CODEX_EVENTS = [
    "SessionStart",
    "UserPromptSubmit",
    "PreToolUse",
    "PermissionRequest",
    "PostToolUse",
    "SubagentStart",
    "SubagentStop",
    "Stop",
    "PreCompact",
    "PostCompact",
]


def _load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    text = path.read_text(encoding="utf-8").strip()
    if not text:
        return {}
    return json.loads(text)


def _backup(path: Path) -> None:
    if path.exists():
        shutil.copy2(path, path.with_suffix(path.suffix + f".cc-mochi-bak-{int(time.time())}"))


def _command(source: str, event: str) -> str:
    return f'PYTHONPATH="{ROOT}" "{PY}" -m cc_mochi.hook --source {source} --event {event}'


def _append_unique(items: list[Any], item: dict[str, Any], command: str) -> list[Any]:
    for existing in items:
        if command in json.dumps(existing, ensure_ascii=False):
            return items
    return items + [item]


def install_claude() -> Path:
    path = Path.home() / ".claude" / "settings.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    data = _load_json(path)
    hooks = data.setdefault("hooks", {})
    for event in CLAUDE_EVENTS:
        command = _command("claude", event)
        item = {"matcher": "*", "hooks": [{"type": "command", "command": command}]}
        hooks[event] = _append_unique(list(hooks.get(event) or []), item, command)
    _backup(path)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return path


def install_codex() -> Path:
    path = Path.home() / ".codex" / "hooks.json"
    path.parent.mkdir(parents=True, exist_ok=True)
    data = _load_json(path)
    hooks = data.setdefault("hooks", {})
    for event in CODEX_EVENTS:
        command = _command("codex", event)
        item = {"command": command}
        hooks[event] = _append_unique(list(hooks.get(event) or []), item, command)
    _backup(path)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return path


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="Install cc-mochi global hooks.")
    parser.add_argument("--claude", action="store_true")
    parser.add_argument("--codex", action="store_true")
    args = parser.parse_args(argv)
    if not args.claude and not args.codex:
        args.claude = args.codex = True
    changed = []
    if args.claude:
        changed.append(install_claude())
    if args.codex:
        changed.append(install_codex())
    for path in changed:
        print(path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
