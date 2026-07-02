# cc-mochi

<p align="right"><b>English</b> | <a href="README.md">中文</a></p>

`cc-mochi` is a small ESP32-C3 + ST7789 desk companion for Claude Code and Codex. It turns whatever the agent is doing into a little face in real time — thinking, reading files, writing code, running commands, waiting for your permission, compacting context, running parallel subtasks, succeeding, erroring, and more. It is intentionally **not** a numeric dashboard: **the expression is the main signal**, and usage only shows up as small cards while the device is idle.

## What this project changed

`cc-mochi` is based on [`clawd-mochi`](https://github.com/yousifamanuel/clawd-mochi) by Yousuf Amanuel. It keeps the original hardware/display foundation and makes three big changes around "being a companion for Claude Code / Codex":

1. **Redrew the whole expression set** — two distinct faces, one for Claude and one for Codex, across a dozen-plus agent states.
2. **Reworked the boot animation** — from the original logo reveal into a little mochi skit: falling asleep → being woken up → starting to type.
3. **Added the bridge stack** — a local daemon + global hooks + a USB-serial JSON Lines protocol that feed both CLIs' events to the screen, plus idle usage cards.

## Expression system (the core)

cc-mochi switches between **two faces with different personalities** depending on the event source:

| | Claude style | Codex style |
| --- | --- | --- |
| Background | Warm orange | Pure black |
| Eyes | Dark, round, large — blinks and blushes | Green pixel-block eyes, terminal/mechanical feel |
| Mouth | Soft arcs (pensive wave / big smile / frown) | Square / pixel straight lines with scanlines |
| When stuck | Half-lidded flat eyes ("listening but stuck") | X-shaped dead eyes |

Beyond changing the eyes and mouth, each state also overlays a small **prop/animation** below the face (thinking dots, a shell prompt bar, parallel-session lanes, subtask node graphs, etc.). The animations are non-blocking — blinks, scanlines and the like loop naturally while idle.

Supported states:

| State | Meaning |
| --- | --- |
| `sleeping` / `idle` | Idle standby (rotates usage cards) |
| `thinking` | Thinking (prompt submitted, session start) |
| `reading` | Reading (Read / Grep / Glob / LS) |
| `writing` | Writing (Edit / Write / MultiEdit) |
| `shell` | Running a command (Bash / Shell) |
| `permission` | Waiting for permission / notification |
| `compact` | Compacting context |
| `subagent` | A subtask is running |
| `multi` | Multiple sessions active in parallel |
| `success` | Success / done (brief flash) |
| `error` | Error |
| `blocked` | Permission denied / stuck |
| `usage_low` / `usage_critical` | High / near-exhausted usage, weary face |

When several sessions are active at once the screen enters the `multi` state and shows parallel lanes; high-priority events that need your attention (like `permission` or `error`) are bumped to the front.

## Boot animation

<p align="center"><img src="pics/cc_mochi_boot.gif" alt="cc-mochi boot animation" width="300"></p>

On power-up it plays a redesigned little skit: the mochi first sleeps with eyes closed, gets woken up and rubs its eyes, then sits down at the keyboard and starts typing, ending with `cc-mochi` printed on screen. The whole animation keeps servicing WiFi requests, so it never blocks the web controller.

> The clip above is a pixel-faithful replay of the firmware boot sequence, rendered frame by frame. For a live version, open `tools/cc_mochi_boot_preview.html` in a browser.

## How hooks work

cc-mochi learns what the agent is doing through **global hooks** in Claude Code / Codex. The path:

```
Claude Code / Codex events
      │  (hook command, one JSON per stdin)
      ▼
cc_mochi.hook  ──►  local daemon (Unix socket)
      │                    │  normalize + pick state by priority
      │                    ▼
      │              USB serial JSON Lines
      │                    ▼
      └─ if daemon not running        ESP32-C3 screen face
         events spool to
         ~/.cc-mochi/missed-hooks.jsonl
```

The hook command is deliberately tiny and non-blocking: it just forwards the event to the daemon and exits immediately. **Even if the daemon is not running, the hook still exits successfully** and the event is recorded to `~/.cc-mochi/missed-hooks.jsonl` — it never slows your CLI down.

Once the daemon receives an event it: normalizes it into one of the states above, picks the one that most deserves to be shown by severity, tracks multiple concurrent sessions, and refreshes usage cards while idle.

## Expressions at a glance

These come straight from the firmware's expression renderer. **For the same state, Claude and Codex are two completely different faces:**

**Claude style** (warm orange, dark round eyes, blinks and blushes, arc mouths)

| thinking | reading | writing | shell |
| :---: | :---: | :---: | :---: |
| ![claude thinking](pics/expressions/claude_thinking.png) | ![claude reading](pics/expressions/claude_reading.png) | ![claude writing](pics/expressions/claude_writing.png) | ![claude shell](pics/expressions/claude_shell.png) |
| **permission** | **success** | **error** | **multi** |
| ![claude permission](pics/expressions/claude_permission.png) | ![claude success](pics/expressions/claude_success.png) | ![claude error](pics/expressions/claude_error.png) | ![claude multi](pics/expressions/claude_multi.png) |

**Codex style** (pure black, neon-green pixel-block eyes, X dead-eyes, scanlines)

| thinking | reading | writing | shell |
| :---: | :---: | :---: | :---: |
| ![codex thinking](pics/expressions/codex_thinking.png) | ![codex reading](pics/expressions/codex_reading.png) | ![codex writing](pics/expressions/codex_writing.png) | ![codex shell](pics/expressions/codex_shell.png) |
| **permission** | **success** | **error** | **multi** |
| ![codex permission](pics/expressions/codex_permission.png) | ![codex success](pics/expressions/codex_success.png) | ![codex error](pics/expressions/codex_error.png) | ![codex multi](pics/expressions/codex_multi.png) |

All 14 states × 2 styles live in `pics/expressions/`. For a live, animated preview (breathing, blinks, scanlines all in motion, no hardware required), open `tools/cc_mochi_expression_preview.html` in a browser.

## Gallery

<p align="center"><img src="pics/cc_mochi_4_3.jpg" alt="cc-mochi device case" width="420"></p>

## Firmware

Open `cc_mochi.ino` in Arduino IDE and upload to an ESP32-C3 Dev Module.

Board settings:

| Setting | Value |
| --- | --- |
| Board | ESP32C3 Dev Module |
| USB CDC On Boot | Enabled |
| CPU Frequency | 160 MHz |
| Upload Speed | 921600 |

Required Arduino libraries:

- `Adafruit GFX Library`
- `Adafruit ST7735 and ST7789 Library`

The device starts a WiFi AP named `cc-mochi` with password `ccmochi1234`, and it also listens on USB serial at `115200`.

## Bridge daemon

Start the local bridge from this repository.

```bash
python3 -m cc_mochi --port /dev/cu.usbmodem101
```

If the port is omitted, the daemon auto-detects common USB serial device names:

```bash
python3 -m cc_mochi
```

`pyserial` is optional. Without it, the daemon uses the local tty directly.

Dry-run mode prints device messages without touching hardware:

```bash
python3 -m cc_mochi --dry-run
```

## Installing hooks

Install global hooks for both Claude Code and Codex in one shot:

```bash
python3 -m cc_mochi.install_hooks
```

Install just one:

```bash
python3 -m cc_mochi.install_hooks --claude
python3 -m cc_mochi.install_hooks --codex
```

The installer backs up existing config before writing:

- Claude Code: `~/.claude/settings.json`
- Codex: `~/.codex/hooks.json`

## Usage cards

While idle, the daemon rotates Codex and Claude usage cards (ring + bar + reset time + short badge). When usage runs high the face shifts into the weary `usage_low` / `usage_critical` states.

- **Codex** usage comes from the local `codex app-server` rate-limit RPC when available.
- **Claude Code** usage prefers Claude subscription sources; if a custom API-key / base-url setup is detected, it is marked as local / API mode instead of being misreported as subscription quota.

## Serial protocol

The daemon sends one JSON object per line.

State:

```json
{"type":"state","source":"codex","state":"shell","label":"Bash","active":1}
```

Usage:

```json
{"type":"usage","provider":"codex","primary":23,"secondary":29,"reset":"1h20m","badge":"plus","unavailable":false,"stale":false}
```

Ping:

```json
{"type":"ping"}
```

The firmware responds with short JSON ACK lines.

## HTTP debug routes

The same new UI can be driven over WiFi:

```text
/cc/state?source=codex&state=thinking&label=prompt&active=1
/cc/usage?provider=codex&primary=42&secondary=10&reset=1h20m&badge=plus
```

Legacy routes are kept for the original controller:

```text
/cmd?k=w    /cmd?k=s    /cmd?k=d
/char?c=x
/canvas?on=1
/draw/clear    /draw/stroke
/backlight?on=1
/state
```

## Usage source policy

Codex usage priority:

1. `codex -s read-only -a untrusted app-server` JSON-RPC `account/rateLimits/read`
2. Future OAuth/backend provider
3. Future `/status` PTY parser
4. Local hook activity stats only as activity, not quota

Claude Code usage priority:

1. Claude OAuth usage API when local OAuth credentials are available
2. Future opt-in Claude Web API cookie provider
3. CLI `/usage` parser
4. Admin API only for organization/API usage and cost
5. Local stats only as activity, not subscription quota

If `ANTHROPIC_BASE_URL` points to a non-Anthropic endpoint, or an API-key mode is detected, cc-mochi displays Claude subscription usage as unavailable/local.

## Project layout

```text
cc_mochi.ino          ESP32-C3 firmware
cc_mochi/             bridge daemon, hooks, usage providers, serial protocol
tests/                unit tests for protocol and usage parsing
tools/                browser-based serial tester and expression preview
models/               printable case/model files
pics/                 README and project images
```

`tools/cc_mochi_expression_preview.html` previews every expression right in the browser; `tools/cc_mochi_tester.html` drives a real device over serial.

## Development

Run tests:

```bash
python3 -m pytest
```

The unit tests require no hardware.

## Hardware

Printable model files live in `models/`. The `cc_mochi` case is provided as `.3mf` and `.stl`; additional inherited model variants are kept as-is so their filenames continue to match the upstream asset names.

## Attribution & license

Based on [`clawd-mochi`](https://github.com/yousifamanuel/clawd-mochi) by Yousuf Amanuel.

Code is MIT licensed. Existing 3D models and media assets retain their original CC BY-NC-SA 4.0 licensing note where applicable.
