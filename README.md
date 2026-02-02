# Claupper

A one-handed Flipper Zero remote for [Claude Code](https://claude.ai/code) + offline Claude Code manual & quiz.

Built by [Wetware Labs](https://WetwareOfficial.com).

## Why Claupper?

Claude Code is a terminal-based AI coding assistant. It asks questions, proposes changes, and waits for your input — but you're stuck at the keyboard. Claupper lets you **control the entire conversation from a Flipper Zero**, one-handed, from across the room.

Approve changes while you're reading docs on your phone. Decline from the couch. Dictate instructions by voice without touching your computer. Switch between terminal windows with Cmd+`. Nuke an entire input line instantly when Claude goes off-track.

Five buttons. No keyboard required.

---

## ⚠ Two Apps, One Codebase — Read This First

Claupper ships as **two separate `.fap` files** because Flipper Zero has two completely different HID stacks depending on your firmware:

| | **Claupper BLE** | **Claupper USB** |
|---|---|---|
| **File** | `claude_remote_ble.fap` | `claude_remote_usb.fap` |
| **Firmware** | Momentum or Unleashed | Stock (official) |
| **Connection** | Bluetooth wireless | USB cable |
| **Best for** | Couch mode, across the room | Desk use, no pairing needed |
| **App menu** | Apps → Bluetooth → Claupper BLE | Apps → Bluetooth → Claupper USB |

**Why two apps?** Stock Flipper firmware doesn't expose the BLE HID profile to external apps — only custom firmwares (Momentum, Unleashed) unlock it. So we compile the same source code twice: once with USB-only HID, once with BLE + USB. The BLE build is the primary version. The USB build exists so stock firmware users aren't left out.

**Install the one that matches your firmware.** If you're on Momentum or Unleashed, grab the BLE version. If you're on stock, grab USB. They're functionally identical — same remote, same manual, same macros, same settings — just different transport.

---

## Remote Mode

Every interaction with Claude Code boils down to choosing option 1, 2, or 3, then hitting Enter. Claupper maps these to the Flipper's d-pad:

| Button | Single Press | Double Press |
|--------|-------------|--------------|
| Left | `1` (approve/yes) | **Clear entire line** |
| Up | `2` (decline/no) | Page Up |
| Right | `3` (other/skip) | **Previous command** (history recall) |
| OK | Enter (confirm) | **Switch window** (Cmd+\` on Mac, Alt+Tab on Win/Linux) |
| Down | **Voice dictation** (Mac/Linux) / **Win+H** (Windows) | Page Down |
| Back (short) | Return home | — |
| Back (long) | **Send Escape key** | — |

### Double-Click Actions

The double-click layer is where it gets powerful:

- **Clear entire line** (double-Left) — Sends `Ctrl+A` then `Ctrl+K`: jumps to the start of the line and kills everything to the end. One gesture wipes the entire input buffer. Works in any terminal, any shell.
- **Switch window** (double-OK) — Cmd+\` on Mac, Alt+Tab on Windows/Linux. Instantly cycle between terminal windows from the Flipper.
- **Previous command** (double-Right) — Sends Up Arrow to recall the last terminal command. Re-run tests, restart servers, repeat builds.
- **Page Up / Page Down** (double-Up / double-Down) — Scroll through long Claude Code output without reaching for your keyboard.

### Voice Dictation

Single-press Down triggers your OS dictation service (macOS Dictation, Windows Speech Recognition). Talk to Claude Code through your Flipper — describe bugs, dictate instructions, explain what you want built. No typing.

### Visual Feedback

Every keypress flashes a rounded overlay on the Flipper screen showing exactly what was sent ("1", "Enter", "Dictate", "Clear", "Switch"). You always know what just happened.

## Manual Mode

A complete offline Claude Code reference guide on the Flipper's 128x64 screen. No internet, no phone, no computer needed.

Seven categories, 29 sections:

- **Getting Started** — Installing, first launch, system requirements
- **Workspace** — Project setup, CLAUDE.md, /init, .claude/ directory
- **Commands** — Navigation, sessions, config, debugging
- **Tools** — File ops, search, sub-agents, web
- **Advanced** — Hooks, MCP, permissions, headless mode
- **Workflows** — New project, debug & test, code review
- **Quiz** — 24-question multiple choice quiz

### Quiz Mode

Pick your difficulty from a classic Mac-style modal: Easy (8 questions), Medium (16), or Hard (24). All multiple choice, questions shuffled each round.

When you answer, a Mac-style modal pops up over the question showing whether you got it right and the correct answer — so you learn as you go. Tracks your score, percentage, and best streak.

## Claupper Mode

For the best experience, add `claupper_mode.md` to your project as `CLAUDE.md` (or append it to an existing one). This tells Claude Code to always present decisions as numbered 1/2/3 choices — so you can control everything from the remote without typing.

See [`claupper_mode.md`](claupper_mode.md) for the full template.

## Settings

Access from the Home screen (Left button). Three options:

| Setting | Values | Default |
|---------|--------|---------|
| **Haptics** | ON / OFF | ON |
| **LED** | ON / OFF | ON |
| **OS** | Mac / Win / Linux | Mac |

Press OK to toggle. Changes save automatically. Back returns to Home.

The **OS setting** controls platform-specific keybindings:
- **Mac** — Voice dictation via macOS Dictation (Fn Fn), window switch via Cmd+\`
- **Windows** — Voice dictation via Win+H, window switch via Alt+Tab
- **Linux** — Voice dictation via consumer key passthrough, window switch via Alt+Tab

## Custom Macros

Load up to 10 custom text macros from an SD card file. Navigate to Macros from the Home screen (Up button).

**Setup:**
1. Create a file at `SD Card/apps_data/<appid>/macros.txt` on your Flipper
   - BLE build: `apps_data/claude_remote_ble/macros.txt`
   - USB build: `apps_data/claude_remote_usb/macros.txt`
2. Add one macro per line (max 32 characters each, max 10 lines)
3. Open Macros from the Home screen — scroll with Up/Down, press OK to send

Each macro is typed out character-by-character via HID and followed by Enter — as if you typed it on the keyboard.

**Preset templates** are included in the `macros/` directory:

| File | Style |
|------|-------|
| `default.txt` | Balanced mix of commands and prompts |
| `minimal.txt` | Slash commands only |
| `maximalist.txt` | Detailed natural-language prompts |
| `barebones.txt` | Just approval keys and basics |
| `workflow.txt` | Full dev cycle (init → code → test → commit) |
| `debugging.txt` | Bug hunting and diagnostics |
| `review.txt` | Code review prompts |

Copy any preset to your Flipper's `macros.txt` path to use it.

## Haptic Feedback

When enabled (default), the Flipper vibrates on every keypress:
- **Single pulse** on regular key sends (1, 2, 3, Enter, etc.)
- **Double pulse** on double-click actions (Clear, Switch, Previous, Page Up/Down)

Toggle in Settings.

## LED Indicators

When enabled (default), the Flipper LED shows which mode you're in:
- **Blue** — Remote mode (USB or BLE)
- **Green** — Manual mode
- **Orange** — Home, Settings, or Macros

Toggle in Settings.

## Install

### BLE Version (Momentum / Unleashed)

1. Install [Momentum](https://momentum-fw.dev/update) or Unleashed firmware
2. Download `claude_remote_ble.fap` from [Releases](https://github.com/Wet-wr-Labs/claupper/releases)
3. Copy to `SD Card/apps/Bluetooth/` on your Flipper
4. Open from **Apps → Bluetooth → Claupper BLE**
5. Pair via Bluetooth on your computer

### USB Version (Stock Firmware)

1. Download `claude_remote_usb.fap` from [Releases](https://github.com/Wet-wr-Labs/claupper/releases)
2. Copy to `SD Card/apps/Bluetooth/` on your Flipper
3. Plug Flipper into your computer via USB
4. Open from **Apps → Bluetooth → Claupper USB**

## Build From Source

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
ufbt                                    # builds both .fap files
ufbt launch APPID=claude_remote_usb     # deploy + run USB version
ufbt launch APPID=claude_remote_ble     # deploy + run BLE version
```

## License

[MIT](LICENSE)

## Links

- [Wetware Labs](https://WetwareOfficial.com)
- [Claude Code](https://claude.ai/code)
- [Momentum Firmware](https://momentum-fw.dev)
