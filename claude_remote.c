#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <gui/gui.h>
#include <gui/view_port.h>
#include <input/input.h>
#include <storage/storage.h>

#ifdef HID_TRANSPORT_BLE
#include <bt/bt_service/bt.h>
#include <extra_profiles/hid_profile.h>
#endif

#ifdef HID_TRANSPORT_BLE
#include <claude_remote_ble_icons.h>
#else
#include <claude_remote_usb_icons.h>
#endif

#define TAG "CRemote"

#define MAX_CHAPTERS 10
#define MAX_CHAPTER_SIZE 2048
#define MANUAL_DIR APP_DATA_PATH("manual")

/* ── Modes ── */

typedef enum {
    ModeHome,
    ModeRemote,
    ModeManual,
} AppMode;

/* ── Manual chapter ── */

typedef struct {
    char title[32];
    char* content;
    uint16_t content_len;
    uint16_t line_count;
} Chapter;

/* ── App state ── */

typedef struct {
    AppMode mode;
    bool is_flipped;
    bool hid_connected;
    FuriMutex* mutex;

#ifdef HID_TRANSPORT_BLE
    Bt* bt;
    FuriHalBleProfileBase* ble_profile;
#else
    FuriHalUsbInterface* usb_prev;
#endif

    /* manual reader */
    Chapter chapters[MAX_CHAPTERS];
    uint8_t chapter_count;
    uint8_t chapter_index;
    int16_t scroll_offset;
    bool manual_loaded;
} ClaudeRemoteState;

/* ── Default manual content (compiled-in fallback) ── */

static const char* default_titles[] = {
    "Getting Started",
    "Keybindings",
    "Slash Commands",
    "Skills & Tools",
    "Playbooks",
};

static const char* default_content[] = {
    "Claude Code is an agentic AI\n"
    "coding tool by Anthropic.\n\n"
    "It runs in your terminal and\n"
    "can read, write, and execute\n"
    "code across your project.\n\n"
    "Start with: claude\n"
    "New session: claude --new\n"
    "Resume: claude --continue\n",

    "Navigation:\n"
    " Ctrl+C  Cancel/interrupt\n"
    " Ctrl+D  Exit Claude Code\n"
    " Up/Down Scroll history\n"
    " Esc     Cancel current input\n\n"
    "Voice:\n"
    " F5 or fn+fn  macOS dictation\n"
    " Configure in System Settings\n"
    "  > Keyboard > Dictation\n",

    "Common slash commands:\n\n"
    " /help     Show help\n"
    " /clear    Clear conversation\n"
    " /compact  Summarize context\n"
    " /config   Open settings\n"
    " /cost     Show token usage\n"
    " /doctor   Check health\n"
    " /init     Create CLAUDE.md\n"
    " /review   Review PR changes\n"
    " /terminal-setup  Fix terminal\n",

    "Skills are specialized\n"
    "capabilities Claude loads\n"
    "from CLAUDE.md or .claude/\n"
    "files in your project.\n\n"
    "Tools available to Claude:\n"
    " Read, Write, Edit files\n"
    " Bash (run commands)\n"
    " Grep, Glob (search)\n"
    " Task (sub-agents)\n"
    " WebFetch, WebSearch\n"
    " NotebookEdit (Jupyter)\n",

    "Recipe: New Project Setup\n"
    " 1. claude --new\n"
    " 2. /init to create CLAUDE.md\n"
    " 3. Describe your project\n\n"
    "Recipe: Debug Failing Tests\n"
    " 1. Paste test output\n"
    " 2. Ask Claude to investigate\n"
    " 3. Approve fixes with 1\n\n"
    "Recipe: Refactor Module\n"
    " 1. Ask to read the module\n"
    " 2. Describe desired changes\n"
    " 3. Review diff, approve/deny\n",
};

#define DEFAULT_CHAPTER_COUNT 5

/* ── Manual loading ── */

static void extract_title(const char* filename, char* title, size_t title_size) {
    /* "01_getting_started.txt" -> "Getting Started" */
    const char* p = filename;

    /* skip numeric prefix + underscore */
    while(*p && (*p >= '0' && *p <= '9')) p++;
    if(*p == '_') p++;

    size_t i = 0;
    bool capitalize_next = true;
    while(*p && *p != '.' && i < title_size - 1) {
        if(*p == '_') {
            title[i++] = ' ';
            capitalize_next = true;
        } else {
            if(capitalize_next && *p >= 'a' && *p <= 'z') {
                title[i++] = *p - 32;
            } else {
                title[i++] = *p;
            }
            capitalize_next = false;
        }
        p++;
    }
    title[i] = '\0';
}

static uint16_t count_lines(const char* text) {
    uint16_t count = 1;
    for(const char* p = text; *p; p++) {
        if(*p == '\n') count++;
    }
    return count;
}

static bool load_manual_from_sd(ClaudeRemoteState* state) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* dir = storage_file_alloc(storage);
    state->chapter_count = 0;

    if(!storage_dir_open(dir, MANUAL_DIR)) {
        storage_dir_close(dir);
        storage_file_free(dir);
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    /* collect filenames */
    char names[MAX_CHAPTERS][48];
    uint8_t count = 0;
    FileInfo fi;
    char name_buf[48];
    while(storage_dir_read(dir, &fi, name_buf, sizeof(name_buf)) && count < MAX_CHAPTERS) {
        if(!(fi.flags & FSF_DIRECTORY)) {
            memcpy(names[count], name_buf, sizeof(name_buf));
            count++;
        }
    }
    storage_dir_close(dir);
    storage_file_free(dir);

    if(count == 0) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    /* sort by name (bubble sort on small array) */
    for(int i = 0; i < count - 1; i++) {
        for(int j = 0; j < count - i - 1; j++) {
            if(strcmp(names[j], names[j + 1]) > 0) {
                char tmp[48];
                memcpy(tmp, names[j], 48);
                memcpy(names[j], names[j + 1], 48);
                memcpy(names[j + 1], tmp, 48);
            }
        }
    }

    /* load each file */
    for(int i = 0; i < count; i++) {
        FuriString* path = furi_string_alloc_printf("%s/%s", MANUAL_DIR, names[i]);

        File* file = storage_file_alloc(storage);
        if(storage_file_open(
               file, furi_string_get_cstr(path), FSAM_READ, FSOM_OPEN_EXISTING)) {
            uint64_t size = storage_file_size(file);
            if(size > MAX_CHAPTER_SIZE) size = MAX_CHAPTER_SIZE;

            Chapter* ch = &state->chapters[state->chapter_count];
            ch->content = malloc(size + 1);
            if(ch->content) {
                uint16_t read = storage_file_read(file, ch->content, size);
                ch->content[read] = '\0';
                ch->content_len = read;
                ch->line_count = count_lines(ch->content);
                extract_title(names[i], ch->title, sizeof(ch->title));
                state->chapter_count++;
            }
        }
        storage_file_close(file);
        storage_file_free(file);
        furi_string_free(path);
    }

    furi_record_close(RECORD_STORAGE);
    return state->chapter_count > 0;
}

static void load_manual_defaults(ClaudeRemoteState* state) {
    state->chapter_count = DEFAULT_CHAPTER_COUNT;
    for(int i = 0; i < DEFAULT_CHAPTER_COUNT; i++) {
        Chapter* ch = &state->chapters[i];
        strncpy(ch->title, default_titles[i], sizeof(ch->title) - 1);
        ch->title[sizeof(ch->title) - 1] = '\0';
        size_t len = strlen(default_content[i]);
        ch->content = malloc(len + 1);
        if(ch->content) {
            memcpy(ch->content, default_content[i], len + 1);
            ch->content_len = len;
            ch->line_count = count_lines(ch->content);
        }
    }
}

static void free_manual(ClaudeRemoteState* state) {
    for(int i = 0; i < state->chapter_count; i++) {
        if(state->chapters[i].content) {
            free(state->chapters[i].content);
            state->chapters[i].content = NULL;
        }
    }
    state->chapter_count = 0;
}

/* ── HID helpers ── */

#ifdef HID_TRANSPORT_BLE
static void send_hid_key(FuriHalBleProfileBase* profile, uint16_t keycode) {
    ble_profile_hid_kb_press(profile, keycode);
    furi_delay_ms(50);
    ble_profile_hid_kb_release(profile, keycode);
}
#else
static void send_hid_key(uint16_t keycode) {
    furi_hal_hid_kb_press(keycode);
    furi_delay_ms(50);
    furi_hal_hid_kb_release(keycode);
}
#endif

/* ── Draw callbacks ── */

static void draw_home(Canvas* canvas) {
    canvas_clear(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Claude Remote");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 26, AlignCenter, AlignCenter, "OK: Remote Control");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "Down: Claude Manual");

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "Back: exit");
}

static void draw_remote(Canvas* canvas, ClaudeRemoteState* state) {
    canvas_clear(canvas);

    if(!state->hid_connected) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 16, AlignCenter, AlignCenter, "Not Connected");
        canvas_set_font(canvas, FontSecondary);
#ifdef HID_TRANSPORT_BLE
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Pair Flipper as");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "BT keyboard first");
#else
        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "Connect Flipper");
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "via USB-C cable");
#endif
        canvas_draw_str_aligned(canvas, 64, 58, AlignCenter, AlignBottom, "Back: home");
        return;
    }

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 8, AlignCenter, AlignTop, "Claude Remote");

    canvas_set_font(canvas, FontSecondary);

    /* button legend */
    canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignCenter, "[2] No");
    canvas_draw_str_aligned(canvas, 16, 36, AlignCenter, AlignCenter, "[1] Yes");
    canvas_draw_str_aligned(canvas, 64, 36, AlignCenter, AlignCenter, "ENTER");
    canvas_draw_str_aligned(canvas, 112, 36, AlignCenter, AlignCenter, "[3]");
    canvas_draw_str_aligned(canvas, 64, 48, AlignCenter, AlignCenter, "Voice");

    const char* orient = state->is_flipped ? "Flipped" : "Normal";
    canvas_draw_str_aligned(canvas, 2, 62, AlignLeft, AlignBottom, orient);
    canvas_draw_str_aligned(canvas, 126, 62, AlignRight, AlignBottom, "Bk:flip");
}

static void draw_manual(Canvas* canvas, ClaudeRemoteState* state) {
    canvas_clear(canvas);

    if(state->chapter_count == 0) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, 64, 28, AlignCenter, AlignCenter, "No manual found");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(canvas, 64, 44, AlignCenter, AlignCenter, "Back: home");
        return;
    }

    Chapter* ch = &state->chapters[state->chapter_index];

    /* title bar */
    canvas_set_font(canvas, FontPrimary);
    char header[48];
    snprintf(
        header,
        sizeof(header),
        "%d/%d %s",
        state->chapter_index + 1,
        state->chapter_count,
        ch->title);
    canvas_draw_str(canvas, 2, 10, header);
    canvas_draw_line(canvas, 0, 13, 128, 13);

    /* scrollable body */
    canvas_set_font(canvas, FontSecondary);

    if(!ch->content) return;

    int y = 24;
    int line_start = state->scroll_offset;
    int current_line = 0;
    const char* p = ch->content;

    /* skip lines before scroll offset */
    while(*p && current_line < line_start) {
        if(*p == '\n') current_line++;
        p++;
    }

    /* render visible lines */
    while(*p && y < 62) {
        char line_buf[32];
        int i = 0;
        while(*p && *p != '\n' && i < 30) {
            line_buf[i++] = *p++;
        }
        line_buf[i] = '\0';
        if(*p == '\n') p++;

        canvas_draw_str(canvas, 2, y, line_buf);
        y += 10;
    }

    /* scroll indicators */
    if(state->scroll_offset > 0) {
        canvas_draw_str_aligned(canvas, 124, 15, AlignRight, AlignTop, "^");
    }
    if(*p) {
        canvas_draw_str_aligned(canvas, 124, 62, AlignRight, AlignBottom, "v");
    }

    /* nav hint */
    canvas_draw_str_aligned(canvas, 64, 62, AlignCenter, AlignBottom, "<  >");
}

static void draw_callback(Canvas* canvas, void* ctx) {
    ClaudeRemoteState* state = (ClaudeRemoteState*)ctx;
    furi_mutex_acquire(state->mutex, FuriWaitForever);

    switch(state->mode) {
    case ModeHome:
        draw_home(canvas);
        break;
    case ModeRemote:
        draw_remote(canvas, state);
        break;
    case ModeManual:
        draw_manual(canvas, state);
        break;
    }

    furi_mutex_release(state->mutex);
}

/* ── Input callback ── */

static void input_callback(InputEvent* input_event, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(queue, input_event, FuriWaitForever);
}

/* ── Input handling per mode ── */

static bool handle_home_input(ClaudeRemoteState* state, InputEvent* event, ViewPort* vp) {
    UNUSED(vp);
    if(event->type != InputTypeShort) return true;

    switch(event->key) {
    case InputKeyOk:
    case InputKeyRight:
        state->mode = ModeRemote;
        break;
    case InputKeyDown:
    case InputKeyLeft:
        if(!state->manual_loaded) {
            if(!load_manual_from_sd(state)) {
                load_manual_defaults(state);
            }
            state->manual_loaded = true;
        }
        state->chapter_index = 0;
        state->scroll_offset = 0;
        state->mode = ModeManual;
        break;
    case InputKeyBack:
        return false; /* exit app */
    default:
        break;
    }
    return true;
}

static bool handle_remote_input(
    ClaudeRemoteState* state,
    InputEvent* event,
    ViewPort* view_port) {
    if(event->key == InputKeyBack) {
        if(event->type == InputTypeLong) {
            /* long back = return home */
            state->mode = ModeHome;
            /* reset orientation */
            if(state->is_flipped) {
                state->is_flipped = false;
                view_port_set_orientation(view_port, ViewPortOrientationHorizontal);
            }
            return true;
        } else if(event->type == InputTypeShort) {
            /* short back = toggle flip */
            state->is_flipped = !state->is_flipped;
            view_port_set_orientation(
                view_port,
                state->is_flipped ? ViewPortOrientationHorizontalFlip :
                                    ViewPortOrientationHorizontal);
            return true;
        }
        return true;
    }

    if(event->type != InputTypeShort) return true;

    /* check connection before sending */
    state->hid_connected = furi_hal_hid_is_connected();
    if(!state->hid_connected) return true;

#ifdef HID_TRANSPORT_BLE
#define SEND_KEY(k) send_hid_key(state->ble_profile, (k))
#else
#define SEND_KEY(k) send_hid_key((k))
#endif

    switch(event->key) {
    case InputKeyLeft:
        SEND_KEY(HID_KEYBOARD_1);
        FURI_LOG_I(TAG, "Sent: 1");
        break;
    case InputKeyUp:
        SEND_KEY(HID_KEYBOARD_2);
        FURI_LOG_I(TAG, "Sent: 2");
        break;
    case InputKeyRight:
        SEND_KEY(HID_KEYBOARD_3);
        FURI_LOG_I(TAG, "Sent: 3");
        break;
    case InputKeyOk:
        SEND_KEY(HID_KEYBOARD_RETURN);
        FURI_LOG_I(TAG, "Sent: Enter");
        break;
    case InputKeyDown:
        SEND_KEY(HID_KEYBOARD_F5);
        FURI_LOG_I(TAG, "Sent: F5 (voice)");
        break;
    default:
        break;
    }

#undef SEND_KEY
    return true;
}

static bool handle_manual_input(ClaudeRemoteState* state, InputEvent* event) {
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return true;

    switch(event->key) {
    case InputKeyUp:
        if(state->scroll_offset > 0) state->scroll_offset--;
        break;
    case InputKeyDown: {
        Chapter* ch = &state->chapters[state->chapter_index];
        int max_scroll = ch->line_count > 4 ? ch->line_count - 4 : 0;
        if(state->scroll_offset < max_scroll) state->scroll_offset++;
        break;
    }
    case InputKeyLeft:
        if(state->chapter_index > 0) {
            state->chapter_index--;
            state->scroll_offset = 0;
        }
        break;
    case InputKeyRight:
        if(state->chapter_index < state->chapter_count - 1) {
            state->chapter_index++;
            state->scroll_offset = 0;
        }
        break;
    case InputKeyBack:
        state->mode = ModeHome;
        break;
    case InputKeyOk:
        break;
    default:
        break;
    }
    return true;
}

/* ── Main app logic (shared by both transports) ── */

static int32_t claude_remote_main(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Starting Claude Remote");

    /* allocate state */
    ClaudeRemoteState* state = malloc(sizeof(ClaudeRemoteState));
    memset(state, 0, sizeof(ClaudeRemoteState));
    state->mode = ModeHome;
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    /* message queue */
    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    /* viewport */
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, state);
    view_port_input_callback_set(view_port, input_callback, queue);

    /* GUI */
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    /* set up HID transport */
#ifdef HID_TRANSPORT_BLE
    state->bt = furi_record_open(RECORD_BT);
    bt_disconnect(state->bt);
    furi_delay_ms(200);
    state->ble_profile = bt_profile_start(state->bt, ble_profile_hid, NULL);
    FURI_LOG_I(TAG, "BLE HID profile started");
#else
    state->usb_prev = furi_hal_usb_get_config();
    furi_hal_usb_unlock();
    furi_hal_usb_set_config(&usb_hid, NULL);
#endif

    /* check initial HID state */
    state->hid_connected = furi_hal_hid_is_connected();

    /* main event loop */
    InputEvent event;
    bool running = true;

    while(running) {
        FuriStatus status = furi_message_queue_get(queue, &event, 100);

        furi_mutex_acquire(state->mutex, FuriWaitForever);

        if(status == FuriStatusOk) {
            switch(state->mode) {
            case ModeHome:
                running = handle_home_input(state, &event, view_port);
                break;
            case ModeRemote:
                running = handle_remote_input(state, &event, view_port);
                break;
            case ModeManual:
                running = handle_manual_input(state, &event);
                break;
            }
        }

        /* periodically update connection status in remote mode */
        if(state->mode == ModeRemote) {
            state->hid_connected = furi_hal_hid_is_connected();
        }

        furi_mutex_release(state->mutex);
        view_port_update(view_port);
    }

    /* cleanup */
    FURI_LOG_I(TAG, "Exiting Claude Remote");

#ifdef HID_TRANSPORT_BLE
    ble_profile_hid_kb_release_all(state->ble_profile);
    bt_profile_restore_default(state->bt);
    furi_record_close(RECORD_BT);
#else
    furi_hal_hid_kb_release_all();
    furi_hal_usb_set_config(state->usb_prev, NULL);
#endif

    free_manual(state);

    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(state->mutex);
    free(state);

    return 0;
}

/* ── Entry points (one per transport, matching application.fam) ── */

int32_t claude_remote_usb_app(void* p) {
    return claude_remote_main(p);
}

int32_t claude_remote_ble_app(void* p) {
    return claude_remote_main(p);
}
