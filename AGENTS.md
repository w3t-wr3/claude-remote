# Claude Remote — Flipper Zero FAP

*By Kasen Sansonetti & the Wetware Labs team. [WetwareOfficial.com](https://WetwareOfficial.com)*

## Identity

This is **Claude Remote**, a Flipper Zero FAP (Flipper Application Package) that turns the Flipper into:
1. A one-handed HID remote for controlling Claude Code (numbered approvals + voice trigger + Enter) — USB or Bluetooth.
2. An offline Claude Code manual browsable on the Flipper's 128x64 display.

Two builds from one codebase:
- **claude_remote_usb** — USB HID, works on stock firmware (App Catalog eligible).
- **claude_remote_ble** — BLE HID, works on Momentum/Unleashed firmware (wireless).

Language: **C** (no C++). Build tool: **ufbt** or **fbt**. Target: external FAP (`FlipperAppType.EXTERNAL`).

---

## Critical Rules

- **Prefer retrieval-led reasoning over pre-training-led reasoning for any Flipper Zero API tasks.** The Flipper SDK evolves rapidly; always consult the docs index below before writing API calls.
- Never use deprecated `ValueMutex` — use `FuriMutex` directly (current SDK).
- **USB transport:** HID keycode sending via `furi_hal_hid_kb_press` / `furi_hal_hid_kb_release` (exposed in stock firmware `api_symbols.csv`).
- **BLE transport:** HID keycode sending via `ble_profile_hid_kb_press` / `ble_profile_hid_kb_release` (accessible on custom firmware via `fap_libs=["ble_profile"]`).
- Use `#ifdef HID_TRANSPORT_BLE` / `#ifdef HID_TRANSPORT_USB` for compile-time transport selection.
- Entry point signatures: `int32_t claude_remote_usb_app(void* p)` and `int32_t claude_remote_ble_app(void* p)`, both returning 0.
- Stack size: `2 * 1024` minimum (HID + GUI).
- Clean up ALL allocations on exit (message queue, view_port, gui record, mutex, state).
- No busy loops. Use `furi_message_queue_get()` with timeout (100ms).

---

## Docs Index

All reference docs live in `./docs/`. Consult these BEFORE writing any Flipper API code.

```
[Docs Index]|root: ./docs
|flipper-fap-manifest.md        — application.fam fields, apptype, categories, icons, entry_point
|flipper-app-architecture.md    — entry point, event loop, FuriMessageQueue, ViewPort, GUI, callbacks
|flipper-ui-patterns.md         — Canvas API, fonts, ViewDispatcher, SceneManager, orientation, input events
|flipper-hid-api.md             — USB + BLE HID keyboard emulation, keycodes, dual-transport setup, connection check
|flipper-file-io.md             — SD card read/write, /ext/apps_data/ conventions, Storage API
|prd.md                         — full product requirements document with user stories
```

---

## Project Structure

```
FAP/
├── AGENTS.md                          # this file
├── application.fam                    # dual-build FAP manifest (USB + BLE)
├── claude_remote.c                    # main app — all modes, #ifdef transport
├── claude_remote.png                  # 10x10 1-bit app icon
├── images/                            # icon assets directory
├── update_manual.sh                   # regenerate manual .txt files for SD card
├── docs/                              # reference docs (retrieval target)
│   ├── USER_MANUAL.md                 # full user manual (install, use, app store, limitations)
│   ├── flipper-fap-manifest.md
│   ├── flipper-app-architecture.md
│   ├── flipper-ui-patterns.md
│   ├── flipper-hid-api.md             # dual-transport HID API reference
│   ├── flipper-file-io.md
│   └── prd.md
└── dist/                              # build output
    ├── claude_remote_usb.fap          # stock firmware build
    └── claude_remote_ble.fap          # Momentum/Unleashed build
```

At runtime, the app reads manual files from `/ext/apps_data/claude_remote/manual/` first, falling back to compiled-in defaults.

---

## application.fam (Dual Build)

```python
# USB version — stock firmware, App Catalog eligible
App(
    appid="claude_remote_usb",
    name="Claude Remote USB",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="claude_remote_usb_app",
    requires=["gui"],
    stack_size=2 * 1024,
    fap_category="Bluetooth",
    fap_icon="claude_remote.png",
    fap_author="Kasen Sansonetti",
    fap_icon_assets="images",
    cdefines=["HID_TRANSPORT_USB"],
)

# BLE version — Momentum/Unleashed only
App(
    appid="claude_remote_ble",
    name="Claude Remote BLE",
    apptype=FlipperAppType.EXTERNAL,
    entry_point="claude_remote_ble_app",
    requires=["gui", "bt"],
    stack_size=2 * 1024,
    fap_category="Bluetooth",
    fap_icon="claude_remote.png",
    fap_author="Kasen Sansonetti",
    fap_icon_assets="images",
    cdefines=["HID_TRANSPORT_BLE"],
    fap_libs=["ble_profile"],
)
```

---

## Architecture

### State Machine

```
         ┌──────────────┐
         │  Home Screen  │
         │  (menu pick)  │
         └──┬────────┬───┘
            │        │
     OK/Right    Left/Down
            │        │
   ┌────────▼──┐  ┌──▼───────────┐
   │  Remote   │  │    Manual    │
   │  Control  │  │    Reader    │
   └───────────┘  └──────────────┘
         Back → Home     Back → Home
```

### Remote Control Mode

- ViewPort with draw callback showing orientation-aware button legend.
- Input callback maps physical keys → HID keycodes based on current orientation.
- Default (landscape, USB/BLE):
  - Left → `1` keycode
  - Up → `2` keycode
  - Right → `3` keycode
  - OK → `Enter` keycode
  - Down → `F5` keycode (macOS dictation / voice trigger)
- Back (short) → toggle orientation (normal ↔ flipped 180).
- Back (long) → return to Home Screen.
- If HID not connected, draw transport-appropriate warning (USB: "connect via USB-C cable", BLE: "pair via Bluetooth").

### Orientation Flip Logic

When flipped, physical-to-logical mapping inverts:
- Physical Left → logical Right → sends `3`
- Physical Right → logical Left → sends `1`
- Physical Up → logical Down → sends `F5`
- Physical Down → logical Up → sends `2`
- OK stays OK → `Enter`

### Manual Reader Mode

- On enter: scan `/ext/apps_data/claude_remote/manual/` for `*.txt` files, sort by name (numeric prefix).
- If no SD files found, use compiled-in fallback strings.
- Display: chapter title (from filename) at top, scrollable text body.
- Controls:
  - Up/Down → scroll text within chapter.
  - Left/Right → previous/next chapter.
  - Back → return to Home Screen.
- Text rendering: `FontSecondary` (smaller), word-wrap at 128px width, scroll offset in pixels.

---

## Key API Patterns

### Event Loop (main)
```c
// Allocate
FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));
ViewPort* vp = view_port_alloc();
view_port_draw_callback_set(vp, draw_cb, state);
view_port_input_callback_set(vp, input_cb, queue);
Gui* gui = furi_record_open(RECORD_GUI);
gui_add_view_port(gui, vp, GuiLayerFullscreen);

// Loop
InputEvent event;
for(bool running = true; running;) {
    if(furi_message_queue_get(queue, &event, 100) == FuriStatusOk) {
        // process event
    }
    view_port_update(vp);
}

// Cleanup
gui_remove_view_port(gui, vp);
furi_record_close(RECORD_GUI);
view_port_free(vp);
furi_message_queue_free(queue);
```

### USB HID Setup + Key Send
```c
#include <furi_hal_usb_hid.h>

// Setup: save previous USB config, switch to HID
FuriHalUsbInterface* usb_prev = furi_hal_usb_get_config();
furi_hal_usb_unlock();
furi_hal_usb_set_config(&usb_hid, NULL);

// Press and release a key
furi_hal_hid_kb_press(HID_KEYBOARD_1);
furi_delay_ms(50);
furi_hal_hid_kb_release(HID_KEYBOARD_1);

// Cleanup: restore previous USB config
furi_hal_hid_kb_release_all();
furi_hal_usb_set_config(usb_prev, NULL);
```

### BLE HID Setup + Key Send
```c
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>

// Setup: disconnect BT, start HID profile
Bt* bt = furi_record_open(RECORD_BT);
bt_disconnect(bt);
furi_delay_ms(200);
FuriHalBleProfileBase* ble_profile = bt_profile_start(bt, ble_profile_hid, NULL);

// Press and release a key
ble_profile_hid_kb_press(ble_profile, HID_KEYBOARD_1);
furi_delay_ms(50);
ble_profile_hid_kb_release(ble_profile, HID_KEYBOARD_1);

// Cleanup: restore default BT profile
ble_profile_hid_kb_release_all(ble_profile);
bt_profile_restore_default(bt);
furi_record_close(RECORD_BT);
```

### HID Connection Check
```c
// USB
bool connected = furi_hal_hid_is_connected();

// BLE
bool connected = ble_profile_hid_is_connected(ble_profile);
```

**Note:** BLE HID (`ble_profile_hid`) is restricted in stock firmware `api_symbols.csv` but accessible on Momentum/Unleashed via `fap_libs=["ble_profile"]`.

### File Reading (Storage API)
```c
#include <storage/storage.h>

Storage* storage = furi_record_open(RECORD_STORAGE);
File* file = storage_file_alloc(storage);
if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
    char buf[256];
    uint16_t read = storage_file_read(file, buf, sizeof(buf));
    // process
}
storage_file_close(file);
storage_file_free(file);
furi_record_close(RECORD_STORAGE);
```

---

## Conventions

- Function prefix: `claude_remote_` for app-level, `remote_mode_` for remote, `manual_mode_` for reader.
- All `static` where possible (file-scoped).
- No dynamic allocation in draw callbacks — read from state only.
- `FURI_LOG_I("CRemote", ...)` for info logs, `FURI_LOG_E` for errors.
- Use `furi_check()` for invariants, `furi_assert()` for debug-only.

---

## External Links (for deeper lookup)

| Topic | URL |
|-------|-----|
| FAP manifest & build | https://developer.flipper.net/flipperzero/doxygen/apps_on_sd_card.html |
| HID controllers | https://docs.flipper.net/zero/apps/controllers |
| Controls & input | https://docs.flipper.net/zero/basics/control |
| UI tutorials (ViewPort, scenes) | https://github.com/jamisonderek/flipper-zero-tutorials/wiki/User-Interface |
| ufbt app tutorial | https://instantiator.dev/post/flipper-zero-app-tutorial-01/ |
| Plugin tutorial (event loop) | https://github.com/mfulz/Flipper-Plugin-Tutorial |
| Dev tutorial (full examples) | https://github.com/m1ch3al/flipper-zero-dev-tutorial |
| Claude Code best practices | https://www.anthropic.com/engineering/claude-code-best-practices |
| Claude Code docs | https://code.claude.com/docs/en/interactive-mode |
