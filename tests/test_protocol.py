from cc_mochi.protocol import choose_display_event, normalize_event


def test_claude_bash_maps_to_shell():
    event = normalize_event(
        "claude",
        {"hook_event_name": "PreToolUse", "tool_name": "Bash", "session_id": "abc"},
    )
    assert event.source == "claude"
    assert event.session_id == "abc"
    assert event.state == "shell"
    assert event.label == "Bash"


def test_permission_beats_recent_success():
    permission = normalize_event("codex", {"hook_event_name": "PermissionRequest", "session_id": "a"})
    success = normalize_event("claude", {"hook_event_name": "Stop", "session_id": "b"})
    assert choose_display_event([success, permission]) == permission


def test_prompt_is_not_forwarded_as_label():
    event = normalize_event(
        "claude",
        {"hook_event_name": "UserPromptSubmit", "prompt": "secret prompt", "session_id": "abc"},
    )
    assert event.state == "thinking"
    assert "secret" not in event.label
