#ifndef VISUALIZER_H
#define VISUALIZER_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>
#include <time.h>
#include <raylib.h>
#include <raymath.h>

#define AVORA_VERSION "3.2.0"
#define AVORA_WINDOW_TITLE "AVORA"

#define MAX_TREE_NODES        256
#define MAX_FOREST_ROOTS      16
#define MAX_LOCAL_VARS        32
#define MAX_SCALAR_VARS       32
#define MAX_CALL_STACK        32
#define MAX_VISUAL_ARRAYS     4
#define MAX_ARRAY_ELEMENTS    32
#define MAX_CODE_SIZE         65536
#define MAX_TEST_INPUT        512
#define SNAPSHOT_BUF_SIZE     16384
#define MAX_STDOUT_SIZE       16384
#define MAX_UNDO_HISTORY      128
#define WATCHDOG_TIMEOUT_SEC  1.5
#define PRESET_COUNT          24
#define MAX_DYNAMIC_TEMPLATES 64
#define MAX_RECENT_FILES      6
#define MAX_TIME_TRAVEL_SNAPS 128
#define MAX_USER_BREAKPOINTS  32

/* ============================================================================
 * 0. MONKEYTYPE-INSPIRED THEME SYSTEM & PERSISTENT APP CONFIGURATION (v3.1.0)
 * ============================================================================ */

typedef enum {
    LANG_PT_BR = 0,
    LANG_EN_US,
    LANG_ES,
    LANG_COUNT
} AppLanguage;

typedef enum {
    STR_MENU_FILE = 0,
    STR_MENU_EDIT,
    STR_MENU_RUN,
    STR_MENU_WINDOW,
    STR_MENU_HELP,
    STR_BTN_SETTINGS,
    STR_BTN_RESET,
    STR_BTN_COMPILE_START,
    STR_BTN_STOP,
    STR_BTN_STEP,
    STR_BTN_STEP_BACK,
    STR_BTN_CONTINUE,
    STR_BTN_AUTOPLAY,
    STR_BTN_PAUSE_AUTO,
    STR_TAB_DIAGNOSTICS,
    STR_TAB_TERMINAL,
    STR_TAB_TERMINAL_NEW,
    STR_BTN_HIDE_TERMINAL,
    STR_HUD_HEADER_FMT,
    STR_HUD_CALL_STACK,
    STR_HUD_LOCAL_VARS,
    STR_HUD_NO_VARS,
    STR_CANVAS_EMPTY_IDLE,
    STR_CANVAS_EMPTY_RUN,
    STR_CANVAS_STAGING_BOX,
    STR_SETTINGS_TITLE,
    STR_SETTINGS_TAB_APPEARANCE,
    STR_SETTINGS_TAB_EDITOR,
    STR_SETTINGS_TAB_DEBUGGER,
    STR_SETTINGS_TAB_TEMPLATES,
    STR_SETTINGS_THEME_HDR,
    STR_SETTINGS_LANG_HDR,
    STR_SETTINGS_BG_HDR,
    STR_SETTINGS_RESTORE_DEFAULTS,
    STR_SETTINGS_CLOSE,
    STR_FILE_NEW,
    STR_FILE_OPEN,
    STR_FILE_RECENT,
    STR_FILE_SAVE,
    STR_FILE_SAVE_AS,
    STR_FILE_SAVE_TPL,
    STR_FILE_EXPORT_PNG,
    STR_FILE_OPEN_TPL_DIR,
    STR_FILE_EXIT,
    STR_EDIT_UNDO,
    STR_EDIT_REDO,
    STR_EDIT_CUT,
    STR_EDIT_COPY,
    STR_EDIT_PASTE,
    STR_EDIT_SELECT_ALL,
    STR_EDIT_RESET_TPL,
    STR_EDIT_PREFERENCES,
    STR_HELP_SHORTCUTS,
    STR_HELP_BLOB_GUIDE,
    STR_HELP_SYSDIAG,
    STR_HELP_OPEN_CFG_DIR,
    STR_HELP_ABOUT,
    STR_SYSDIAG_TITLE,
    STR_SYSDIAG_COPY_MD,
    STR_SYSDIAG_COPIED_TOAST,
    STR_STATUS_EDIT_FMT,
    STR_STATUS_DEBUG_FMT,
    STR_STATUS_DONE_FMT,
    STR_COUNT
} StrId;

typedef enum {
    THEME_MIDNIGHT_SLATE = 0,  /* Default AVORA Midnight Slate */
    THEME_SERIKA_DARK,         /* Monkeytype Classic (#323437 / #E2B714 / #D1D0C5) */
    THEME_CATPPUCCIN_MOCHA,    /* Catppuccin Mocha (#1E1E2E / #313244 / #CBA6F7) */
    THEME_NORD,                /* Nord (#2E3440 / #3B4252 / #88C0D0) */
    THEME_GRUVBOX_DARK,        /* Gruvbox Dark (#282828 / #3C3836 / #FABD2F) */
    THEME_DRACULA,             /* Dracula (#282A36 / #44475A / #BD93F9) */
    THEME_SERIKA_LIGHT,        /* Serika Light / Paper (#F5F5F0 / #2C2E31 / #D97706) */
    THEME_COUNT
} ThemeId;

typedef struct {
    const char *name;
    bool        is_light;
    Color       bg_canvas;          /* 2D Canvas world background */
    Color       canvas_grid;        /* Dot grid / Line grid color */
    Color       bg_panel;           /* Code editor, menu bar & footer surface */
    Color       bg_gutter;          /* Line number gutter & terminal drawer bg */
    Color       bg_elevated;        /* Buttons, cards, tree nodes & array cells */
    Color       bg_hover;           /* Hovered / focused control fill */
    Color       border;             /* Subtle divider & border lines */
    Color       text_primary;       /* Main readable foreground text */
    Color       text_secondary;     /* Muted labels, line numbers, comments */
    Color       accent_primary;     /* Primary action button & active badge */
    Color       accent_primary_fg;  /* Text on primary action button */
    Color       accent_secondary;   /* Focus border, left/prev pointer edge */
    Color       accent_tertiary;    /* Right/next pointer edge */
    Color       accent_warning;     /* Active debugger line & mutated node halo */
    Color       accent_error;       /* Compile/runtime fault & cycle indicator */
    /* Syntax Highlighting Colors */
    Color       syntax_keyword;
    Color       syntax_type;
    Color       syntax_func;
    Color       syntax_string;
    Color       syntax_number;
    Color       syntax_comment;
} ThemePalette;

typedef struct {
    /* Appearance & Localization */
    int   language;                     /* 0 = PT-BR, 1 = EN-US, 2 = ES */
    int   theme_id;                     /* 0..6 (ThemeId) */
    int   canvas_bg_style;              /* 0 = Dot Grid, 1 = Grade de Linhas, 2 = Liso */
    bool  reduce_animations;            /* Fast/instant transitions */
    bool  show_memory_addresses;        /* Show 0x... badge below nodes */
    bool  show_hud;                     /* Show Call Stack & Local Variables HUD */
    bool  show_blob;                    /* Show Blob mascot & interactive decision tree */
    bool  presentation_mode;            /* Classroom/Projector high-readability mode */
    bool  audio_enabled;                /* Procedural memory-synthesized SFX enabled */
    float audio_volume;                 /* [0.0f .. 1.0f], default 0.45f */

    /* Code Editor */
    int   editor_font_size;             /* 12..24 (default 16) */
    int   tab_size;                     /* 2 or 4 spaces (default 4) */
    bool  show_line_numbers;            /* Show gutter line numbers */
    bool  highlight_current_line;       /* Highlight caret line in editor */
    bool  auto_indent;                  /* Smart indent on Enter */
    bool  restore_last_session;         /* Restore last open file or template on startup */

    /* Debugger (GCC / GDB) */
    bool  asan_enabled;                 /* Compile with -fsanitize=address */
    float watchdog_timeout_sec;         /* 1.5, 3.0, 5.0, 10.0 */
    float default_speed_t;              /* Default auto-play slider position [0..1] */
    bool  auto_open_terminal_on_stdout; /* Open terminal automatically when printf outputs */
    bool  start_paused_on_main;         /* Pause at first line of main() vs immediate Auto-Play */
    bool  enable_stdin;                 /* Show editable stdin split inside Terminal tab */

    /* Templates & Classroom Folder */
    int   templates_mode;               /* 0 = Embutidos + Pasta, 1 = Apenas Pasta, 2 = Apenas Embutidos */
    char  templates_dir[512];           /* Default: ~/.config/avora/templates */

    /* Window & Performance */
    bool  remember_window_geometry;
    int   window_w;
    int   window_h;
    bool  window_maximized;
    float split_ratio;
    bool  vsync_enabled;
    int   target_fps;                   /* 30, 60, 120, 144 */
    bool  confirm_unsaved_changes;

    /* Recent Files & Last Session */
    char  recent_files[MAX_RECENT_FILES][512];
    int   recent_count;
    char  last_open_file[512];
    int   last_selected_template;
} AppConfig;

typedef struct {
    char  display_name[128];
    char  file_path[512];
    bool  is_builtin;
    int   builtin_index;
} TemplateEntry;

/* ============================================================================
 * 1. STATE MACHINES & ENUMERATIONS
 * ============================================================================ */

typedef enum {
    STATE_IDLE = 0,
    STATE_COMPILING,
    STATE_RUNNING,
    STATE_PAUSED,
    STATE_STEPPING,
    STATE_CRASHED,
    STATE_TIMEOUT,
    STATE_FINISHED
} DebuggerState;

typedef enum {
    EDGE_NONE = 0,
    EDGE_OK,
    EDGE_CYCLE,
    EDGE_GARBAGE_PTR,
    EDGE_DANGLING
} EdgeState;

typedef enum {
    ERR_NONE = 0,
    ERR_COMPILE,
    ERR_SIGSEGV_NULL,
    ERR_SIGSEGV_GARBAGE,
    ERR_ASAN_UAF,
    ERR_ASAN_OOB,
    ERR_CYCLE_DETECTED,
    ERR_INFINITE_LOOP
} ErrorType;

/* Interactive Decision-Tree Nodes for the Pedagogical Blob Mascot */
typedef enum {
    BLOB_HINT_NONE = 0,
    /* Idle / Exploratory Decision Tree */
    BLOB_HINT_MASCOT,             /* Root idle menu */
    BLOB_HINT_BUILD_TREE,         /* Subtree: BUILD_TREE & BUILD_LIST */
    BLOB_HINT_BUILD_TREE_FOREST,  /* Leaf: Multi-tree forest & linked list example */
    BLOB_HINT_LEETCODE,           /* Subtree: LeetCode mode & Arrays */
    BLOB_HINT_LEETCODE_ARRAYS,    /* Leaf: Visualizing vectors & index pointers */
    BLOB_HINT_POINTERS,           /* Subtree: Heap memory & pointer semantics */
    BLOB_HINT_POINTERS_UNINIT,    /* Leaf: Soft uninitialized pointers [ ? não inic. ] */
    /* Error Context Decision Tree */
    BLOB_HINT_ERR_ROOT,           /* Root error diagnosis card */
    BLOB_HINT_ERR_WHY,            /* Leaf: Why did this happen on this line? */
    BLOB_HINT_ERR_FIX,            /* Leaf: How do I fix this in C? */
    BLOB_HINT_STDIN_DETECTED      /* Smart prompt when scanf/fgets/getchar is used with enable_stdin == false */
} BlobHintTopic;

/* ============================================================================
 * 2. CORE VISUAL & INTROSPECTION DATA STRUCTURES
 * ============================================================================ */

typedef struct {
    uint64_t  addr;
    uint64_t  left_addr;   /* tree->left  OR list->prev */
    uint64_t  right_addr;  /* tree->right OR list->next */
    int       val;

    /* 60 FPS smooth interpolation coordinates */
    Vector2   current_pos;
    Vector2   target_pos;
    float     subtree_width;

    /* Visual animation & state */
    float     alpha;
    float     scale;
    float     highlight_timer;
    float     border_flash_timer; /* ~150ms crisp border flash on allocation/mutation */
    EdgeState left_state;
    EdgeState right_state;
    bool      is_active_focus;
    bool      is_newly_allocated;
    bool      is_detached;
    bool      is_dangling;
    bool      is_list_node; /* true when allocated as struct ListNode (linear layout) */
    int       tree_depth;
    int       forest_id;
} VisualNode;

typedef struct {
    char     name[64];
    uint64_t addr;
    bool     is_valid_node;
    bool     is_null;
    bool     is_garbage;
    bool     changed;
    float    change_timer;
} LocalPointerVar;

typedef struct {
    char     name[64];
    char     type_str[48];
    char     value_str[128];
    int      int_val;
    bool     is_int;
    bool     is_index_candidate;
    bool     changed;
    float    change_timer;
} LocalScalarVar;

typedef struct {
    int      level;
    int      line;
    char     func[64];
    char     args_summary[128];
} StackFrameInfo;

typedef struct {
    char     name[64];
    int      count;
    int      values[MAX_ARRAY_ELEMENTS];
    bool     changed[MAX_ARRAY_ELEMENTS];
    float    change_timer[MAX_ARRAY_ELEMENTS];
} VisualArray;

typedef struct {
    VisualNode      nodes[MAX_TREE_NODES];
    int             node_count;
    uint64_t        active_root_addr;
    uint64_t        forest_roots[MAX_FOREST_ROOTS];
    int             forest_root_count;
    int             current_executing_line;
    int             call_stack_depth;
    char            current_func[64];

    /* Local pointer variables (curr, parent, novo, temp, root, head, etc.) */
    LocalPointerVar local_vars[MAX_LOCAL_VARS];
    int             local_var_count;

    /* Local scalar/primitive variables (int, char, bool, float) for HUD & Array indexing */
    LocalScalarVar  scalar_vars[MAX_SCALAR_VARS];
    int             scalar_var_count;

    /* Active Call Stack frames from GDB/MI (-stack-list-frames) */
    StackFrameInfo  frames[MAX_CALL_STACK];
    int             frame_count;

    /* Visualized 1D Arrays / Vectors for Search & Sorting visualization */
    VisualArray     arrays[MAX_VISUAL_ARRAYS];
    int             array_count;
    int             mutated_node_count; /* Number of nodes/cells allocated or mutated in the latest step */
} TreeSnapshot;

typedef struct {
    ErrorType error_type;
    int       fault_line;
    uint64_t  fault_addr;
    uint64_t  secondary_addr;
    bool      auto_main_warning;
    bool      has_uninit_ptr_warning;
    char      auto_main_func[64];
    char      raw_trace[2048];
    char      raw_compiler_log[4096]; /* Literal multi-line GCC / ASan output for Retractable Terminal */
    char      pedagogical_title[160];
    char      pedagogical_explanation[640];
} DiagnosticReport;

/* ============================================================================
 * 3. IPC / GDB SESSION CONTROLLER CONTEXT
 * ============================================================================ */

typedef struct {
    DebuggerState    state;
    pid_t            gdb_pid;
    int              stdin_fd;
    int              stdout_fd;
    int              stderr_fd;

    /* Non-blocking line accumulation buffers */
    char             stdout_buf[65536];
    size_t           stdout_len;
    char             stderr_buf[16384];
    size_t           stderr_len;

    /* Real-time Student Program stdout capture (/tmp/aed_program_stdout.txt) */
    char             program_stdout[MAX_STDOUT_SIZE];
    size_t           program_stdout_len;
    off_t            program_stdout_offset;
    bool             has_new_stdout;

    /* Watchdog timer (CLOCK_MONOTONIC) */
    struct timespec  last_cmd_timestamp;
    bool             watchdog_armed;

    /* Exponential Playback Speed (0.05x to 100.0x) & Auto-Play control */
    bool             auto_play;
    float            speed_slider_t;    /* [0.0f .. 1.0f], default 0.391f -> ~1.00x */
    float            play_speed;        /* Computed: 0.05f * powf(2000.0f, speed_slider_t) */
    float            step_accumulator;
    bool             asan_enabled;
    bool             user_asan_pref;    /* Controlled via AppConfig / Settings */
    float            watchdog_timeout_sec; /* Controlled via AppConfig (default 1.5s) */
    bool             waiting_for_step;
    int              step_bounce_guard; /* Prevents infinite bounce outside student_code.c */

    /* Code & Test Harness inputs */
    char             code_buffer[MAX_CODE_SIZE];
    char             test_input[MAX_TEST_INPUT];
    char             stdin_buffer[2048];
    bool             enable_stdin;
    int              selected_preset;

    /* Snapshot & Diagnostics */
    TreeSnapshot     snapshot;
    DiagnosticReport diagnostic;

    /* Visual Time-Travel History Ring Buffer (Last 128 Snapshots) */
    TreeSnapshot     history_snapshots[MAX_TIME_TRAVEL_SNAPS];
    int              history_count;
    int              history_cursor;

    /* Clickable Gutter User Breakpoints (Line numbers in student_code.c) */
    int              breakpoints[MAX_USER_BREAKPOINTS];
    int              breakpoint_count;
} DebuggerSession;

/* ============================================================================
 * 4. UI WORKSPACE, SELECTION ENGINE, UNDO/REDO, HUD & AVORA BLOB STATE
 * ============================================================================ */

typedef struct {
    char *code_snapshot;
    int   sel_anchor;
    int   sel_active;
} EditorUndoEntry;

typedef struct {
    /* Persistent AppConfig & Active Monkeytype Theme Palette (v3.1.0) */
    AppConfig        config;
    ThemePalette     theme;
    char             font_path[512];
    int              loaded_editor_font_px;

    /* Dynamic Templates (Built-in + Classroom Folder) & File Tracking */
    TemplateEntry    templates[MAX_DYNAMIC_TEMPLATES];
    int              template_count;
    int              active_template_idx;
    char             current_file_path[512];
    bool             is_dirty;

    /* Top Menu Bar & Desktop Modal States */
    int              active_menu;           /* -1 = None, 0 = Arquivo, 1 = Editar, 2 = Executar, 3 = Janela, 4 = Ajuda */
    bool             show_recent_submenu;
    bool             modal_settings_open;
    int              settings_active_tab;   /* 0 = Aparência, 1 = Editor, 2 = Depurador, 3 = Templates & Janela */
    bool             modal_shortcuts_open;
    bool             modal_about_open;
    bool             modal_sysdiag_open;
    bool             sysdiag_probed;
    char             sysdiag_gcc_ver[128];
    char             sysdiag_gdb_ver[128];
    bool             modal_path_input_open;
    int              path_input_mode;       /* 0 = Abrir .c, 1 = Salvar .c, 2 = Pasta Templates, 3 = Exportar PNG */
    char             path_input_buf[512];
    bool             path_textbox_edit;
    bool             modal_unsaved_confirm_open;
    int              pending_action_type;   /* 0=none, 1=new, 2=open, 3=template, 4=reset, 5=quit, 6=open_recent */
    int              pending_action_param;
    bool             request_exit;
    bool             pending_png_capture;
    char             pending_png_path[512];
    char             toast_message[160];
    float            toast_timer;

    /* Hit-tested Code Editor & Full Selection Engine */
    int              sel_anchor_pos;
    int              sel_active_pos;
    bool             is_selecting_with_mouse;
    float            scroll_y;
    float            scroll_x;
    float            target_scroll_y;
    float            target_scroll_x;
    float            undo_hold_timer;
    float            undo_repeat_accum;
    bool             has_focus;
    float            cursor_blink_timer;

    /* Double / Triple Click & Undo/Redo Stack */
    double           last_click_time;
    Vector2          last_click_pos;
    int              click_streak;
    EditorUndoEntry  undo_stack[MAX_UNDO_HISTORY];
    int              undo_count;
    int              undo_index;

    /* Interactive Vertical Splitter (Editor vs. Full-Height Canvas) */
    float            split_vertical_ratio;   /* Default 0.42f, clamped [0.22f .. 0.68f] */
    float            split_horizontal_ratio; /* Kept for compatibility */
    bool             dragging_v_split;
    bool             dragging_h_split;

    /* 2D Canvas Camera (Pan, Zoom & Auto-Focus Toggle) */
    Camera2D         camera;
    bool             is_panning;
    bool             camera_auto_focus;      /* [ ⌖ ] Auto-framing toggle */
    Vector2          pan_start_mouse;
    Vector2          pan_start_target;

    /* Floating Observability HUD (Call Stack & Local Variables) */
    bool             hud_collapsed;
    float            hud_scroll_y;

    /* Retractable Bottom Terminal Drawer (GCC/ASan Diagnostics & Program Terminal) */
    bool             terminal_open;
    int              terminal_active_tab;    /* 0 = Diagnóstico, 1 = Terminal (stdout + optional stdin) */
    float            terminal_anim_h;        /* Smooth height animation [0 .. 215px] */
    float            terminal_scroll_y;
    float            terminal_target_scroll_y;
    bool             stdin_textbox_focused;

    /* Animated Pedagogical "Blob" Mascot & Interactive Decision Tree */
    BlobHintTopic    active_blob_topic;
    BlobHintTopic    displayed_blob_topic;
    float            blob_anim_progress;     /* 0.0f -> 1.0f speech bubble spring animation */
    float            blob_awake_anim;        /* 1.0f = awake & expressive, 0.0f = shrunk into cute idle ball */
    float            blob_idle_timer;        /* Seconds since last interaction/step */
    float            blob_blink_timer;
    Vector2          blob_eye_look;          /* Smooth eye tracking target */
    Rectangle        blob_mascot_bounds;
    Rectangle        blob_card_bounds;

    /* UI Controls & Blob Dismissal State */
    bool             dropdown_edit_mode;
    bool             test_input_edit_mode;
    bool             blob_error_dismissed;
    ErrorType        last_seen_error_type;
    float            pulse_timer;
    float            error_shake_timer;   /* ~150ms 2-3px micro-shake on compile/runtime trap */
    float            error_flash_timer;   /* ~150ms subtle crimson flash on trap */
    int              last_step_line_sfx;  /* Prevents duplicate tick SFX on same step */
    Font             mono_font;    /* Dynamic 12..26px 1:1 — Code Editor */
    Font             font_ui;      /* 15px 1:1 — RayGUI controls, titles, gutter */
    Font             font_sm;      /* 13px 1:1 — HUD, Terminal tabs/logs, Blob, Status */
    Font             font_canvas;  /* 32px + Mipmaps (Trilinear) — Zoomed 2D World */
    bool             has_custom_font;
} WorkspaceUI;

/* iOS-style framerate-independent exponential decay helper */
static inline float avora_exp_decay(float speed, float dt, bool reduce_animations)
{
    if (reduce_animations || dt <= 0.0f) return 1.0f;
    float f = 1.0f - expf(-speed * dt);
    return (f < 0.0f) ? 0.0f : ((f > 1.0f) ? 1.0f : f);
}

/* ============================================================================
 * 5. MODULE FUNCTION PROTOTYPES
 * ============================================================================ */

/* --- src/i18n.c --- */
void        i18n_set_language(AppLanguage lang);
AppLanguage i18n_get_language(void);
const char *tr(StrId id);

/* --- src/audio.c --- */
typedef enum {
    SFX_STEP_TICK = 0,
    SFX_NODE_POP,
    SFX_COMPILE_CHIME,
    SFX_ERROR_THUD,
    SFX_UI_CLICK,
    SFX_COUNT
} SfxType;

void audio_engine_init(void);
void audio_engine_cleanup(void);
bool audio_engine_is_ready(void);
void audio_play_sfx(SfxType type, const AppConfig *cfg, float pitch_variation);

/* --- src/config.c --- */
void config_get_dir_path(char *out_buf, size_t max_len);
void config_get_ini_path(char *out_buf, size_t max_len);
void config_reset_defaults(AppConfig *cfg);
bool config_load(AppConfig *cfg);
bool config_save(const AppConfig *cfg);
void config_add_recent_file(AppConfig *cfg, const char *file_path);
ThemePalette theme_get_palette(int theme_id);
void theme_apply_to_raygui(const ThemePalette *pal);
void templates_refresh_list(WorkspaceUI *ui);
int  templates_export_builtins_to_folder(const char *folder_path);
void platform_open_folder_in_file_manager(const char *folder_path);
bool platform_native_file_dialog(
    int         mode, /* 0 = Open .c, 1 = Save .c, 2 = Pick Folder, 3 = Save .png */
    const char *initial_path,
    char       *out_path,
    size_t      out_max
);

/* --- src/diagnostics.c --- */
void sanitize_utf8_text(char *str);
void diagnostics_clear(DiagnosticReport *report);
void diagnostics_set_compile_error(DiagnosticReport *report, const char *raw_gcc_err);
void diagnostics_set_runtime_trap(
    DiagnosticReport *report,
    ErrorType         type,
    int               fault_line,
    uint64_t          fault_addr,
    uint64_t          secondary_addr,
    const char       *raw_trace
);
void diagnostics_inspect_snapshot(
    const TreeSnapshot *snapshot,
    DiagnosticReport   *report,
    DebuggerState      *state_inout
);

/* --- src/ipc_gdb.c --- */
void session_init(DebuggerSession *session);
void session_cleanup(DebuggerSession *session);
bool compile_student_code(DebuggerSession *session);
bool gdb_start_session(DebuggerSession *session);
void gdb_stop_session(DebuggerSession *session);
void gdb_send_command(DebuggerSession *session, const char *cmd);
void gdb_request_step(DebuggerSession *session);
void gdb_request_continue(DebuggerSession *session);
bool session_can_step_back(const DebuggerSession *session);
bool session_step_back(DebuggerSession *session);
void session_step_forward_or_gdb(DebuggerSession *session);
bool session_has_breakpoint(const DebuggerSession *session, int line);
void session_toggle_breakpoint(DebuggerSession *session, int line);
void session_clear_breakpoints(DebuggerSession *session);
void gdb_poll_and_parse(DebuggerSession *session);
void watchdog_check(DebuggerSession *session);
void load_code_preset(DebuggerSession *session, int preset_index);
const char *get_builtin_preset_info(int index, const char **out_display_name, const char **out_filename);

/* --- src/layout_renderer.c --- */
void layout_init_camera(WorkspaceUI *ui, Rectangle canvas_bounds);
void layout_center_camera(WorkspaceUI *ui, const TreeSnapshot *snapshot, Rectangle canvas_bounds);
void layout_compute_tree_positions(TreeSnapshot *snapshot, Rectangle canvas_bounds);
void layout_update_animations(TreeSnapshot *snapshot, float dt, float *pulse_timer, bool reduce_animations);
void layout_render_canvas(
    DebuggerSession *session,
    WorkspaceUI     *ui,
    Rectangle        canvas_bounds
);

/* --- src/ui_workspace.c --- */
void ui_workspace_init(WorkspaceUI *ui, DebuggerSession *session);
void ui_workspace_Unload(WorkspaceUI *ui);
void ui_workspace_update_window_title(WorkspaceUI *ui);
void ui_workspace_reload_editor_font(WorkspaceUI *ui);
void ui_workspace_update_and_draw(DebuggerSession *session, WorkspaceUI *ui);

#endif /* VISUALIZER_H */
