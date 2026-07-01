from cc_mochi.usage import UsageSnapshot, claude_auth_mode, strip_ansi
from cc_mochi.daemon import MochiDaemon


def test_custom_claude_base_url_disables_subscription_usage():
    env = {"ANTHROPIC_BASE_URL": "https://api.moonshot.cn/anthropic"}
    assert claude_auth_mode(env) == "custom-api"


def test_api_key_mode_disables_subscription_usage():
    env = {"ANTHROPIC_API_KEY": "sk-ant-api03-test"}
    assert claude_auth_mode(env) == "api-key"


def test_usage_snapshot_display_is_compact():
    payload = UsageSnapshot("codex", primary=23.4, secondary=88.8, reset="1h20m", badge="plus").to_display()
    assert payload["type"] == "usage"
    assert payload["primary"] == 23
    assert payload["secondary"] == 89
    assert payload["reset"] == "1h20m"


def test_strip_ansi():
    assert strip_ansi("\x1b[31mred\x1b[0m") == "red"


def test_usage_alert_for_critical_window():
    alert = MochiDaemon._usage_alert([UsageSnapshot("codex", primary=97, reset="12m")])
    assert alert is not None
    assert alert["state"] == "usage_critical"
    assert alert["label"] == "reset 12m"
