from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
from typing import Any


class Severity(IntEnum):
    idle = 0
    active = 10
    tool = 20
    subagent = 30
    success = 40
    permission = 50
    compact = 60
    warning = 80
    error = 90


STATE_SEVERITY = {
    "idle": Severity.idle,
    "sleeping": Severity.idle,
    "thinking": Severity.active,
    "reading": Severity.tool,
    "writing": Severity.tool,
    "shell": Severity.tool,
    "tool": Severity.tool,
    "subagent": Severity.subagent,
    "permission": Severity.permission,
    "compact": Severity.compact,
    "success": Severity.success,
    "usage_low": Severity.warning,
    "usage_critical": Severity.error,
    "blocked": Severity.error,
    "error": Severity.error,
}


CLAUDE_EVENT_STATES = {
    "SessionStart": "thinking",
    "UserPromptSubmit": "thinking",
    "UserPromptExpansion": "thinking",
    "PreToolUse": "tool",
    "PermissionRequest": "permission",
    "PermissionDenied": "blocked",
    "PostToolUse": "success",
    "PostToolUseFailure": "error",
    "PostToolBatch": "tool",
    "SubagentStart": "subagent",
    "SubagentStop": "success",
    "TaskCreated": "subagent",
    "TaskCompleted": "success",
    "MessageDisplay": "thinking",
    "Stop": "success",
    "StopFailure": "error",
    "PreCompact": "compact",
    "PostCompact": "success",
    "Notification": "permission",
    "SessionEnd": "idle",
}

CODEX_EVENT_STATES = {
    "SessionStart": "thinking",
    "UserPromptSubmit": "thinking",
    "PreToolUse": "tool",
    "PermissionRequest": "permission",
    "PostToolUse": "success",
    "SubagentStart": "subagent",
    "SubagentStop": "success",
    "Stop": "success",
    "PreCompact": "compact",
    "PostCompact": "success",
}


TOOL_STATE_HINTS = {
    "Bash": "shell",
    "Shell": "shell",
    "Read": "reading",
    "Glob": "reading",
    "Grep": "reading",
    "LS": "reading",
    "Edit": "writing",
    "Write": "writing",
    "MultiEdit": "writing",
    "NotebookEdit": "writing",
}


def sanitize_short(value: Any, *, default: str = "", limit: int = 20) -> str:
    if value is None:
        return default
    text = str(value).replace("\n", " ").replace("\r", " ").strip()
    text = "".join(ch for ch in text if 32 <= ord(ch) <= 126)
    if not text:
        return default
    return text[:limit]


def session_id_for(payload: dict[str, Any]) -> str:
    for key in ("session_id", "sessionId", "thread_id", "threadId", "conversation_id"):
        if payload.get(key):
            return sanitize_short(payload[key], limit=48)
    cwd = sanitize_short(payload.get("cwd"), default="global", limit=48)
    return cwd or "global"


@dataclass(frozen=True)
class NormalizedEvent:
    source: str
    session_id: str
    state: str
    label: str
    event: str

    @property
    def severity(self) -> Severity:
        return STATE_SEVERITY.get(self.state, Severity.active)


def normalize_event(source: str, payload: dict[str, Any]) -> NormalizedEvent:
    clean_source = sanitize_short(source.lower(), default="agent", limit=12)
    event = sanitize_short(
        payload.get("hook_event_name") or payload.get("event") or payload.get("event_name"),
        default="event",
        limit=32,
    )
    tool = sanitize_short(payload.get("tool_name") or payload.get("tool"), limit=20)

    table = CLAUDE_EVENT_STATES if clean_source == "claude" else CODEX_EVENT_STATES
    state = table.get(event, "thinking")
    if state == "tool" and tool:
        state = TOOL_STATE_HINTS.get(tool, "tool")
    if event in {"StopFailure", "PostToolUseFailure"}:
        state = "error"

    label = tool or event
    if state == "idle":
        label = "idle"
    elif state == "permission":
        label = "permission"
    elif state == "compact":
        label = "compact"

    return NormalizedEvent(
        source=clean_source,
        session_id=session_id_for(payload),
        state=state,
        label=sanitize_short(label, default=state, limit=18),
        event=event,
    )


def choose_display_event(events: list[NormalizedEvent]) -> NormalizedEvent | None:
    if not events:
        return None
    return max(events, key=lambda item: (item.severity, item.event != "Stop"))
