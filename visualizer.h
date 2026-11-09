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

#define AVORA_WINDOW_TITLE \
    "AVORA — Analisador Visual de Operações e Rastreamento de Árvores (Algoritmos & Estruturas de Dados)"

#define MAX_TREE_NODES       256
#define MAX_FOREST_ROOTS     16
#define MAX_LOCAL_VARS       32
#define MAX_CALL_STACK       64
#define MAX_CODE_SIZE        65536
#define MAX_TEST_INPUT       512
#define SNAPSHOT_BUF_SIZE    16384
#define WATCHDOG_TIMEOUT_SEC 1.5
#define PRESET_COUNT         16

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

typedef enum {
    BLOB_HINT_NONE = 0,
    BLOB_HINT_MASCOT,
    BLOB_HINT_BUILD_TREE,
    BLOB_HINT_LEETCODE,
    BLOB_HINT_POINTERS
} BlobHintTopic;

/* ============================================================================
 * 2. CORE VISUAL & INTROSPECTION DATA STRUCTURES
 * ============================================================================ */

typedef struct {
    uint64_t  addr;
    uint64_t  left_addr;
    uint64_t  right_addr;
    int       val;

    /* 60 FPS smooth interpolation coordinates */
    Vector2   current_pos;
    Vector2   target_pos;
    float     subtree_width;

    /* Visual animation & state */
    float     alpha;
    float     scale;
    float     highlight_timer;
    EdgeState left_state;
    EdgeState right_state;
    bool      is_active_focus;
    bool      is_newly_allocated;
    bool      is_detached;
    bool      is_dangling;
    int       tree_depth;
    int       forest_id;
} VisualNode;

typedef struct {
    char     name[64];
    uint64_t addr;
    bool     is_valid_node;
    bool     is_null;
    bool     is_garbage;
} LocalPointerVar;

typedef struct {
    VisualNode      nodes[MAX_TREE_NODES];
    int             node_count;
    uint64_t        active_root_addr;
    uint64_t        forest_roots[MAX_FOREST_ROOTS];
    int             forest_root_count;
    int             current_executing_line;
    int             call_stack_depth;
    char            current_func[64];

    /* Local pointer variables (curr, parent, novo, temp, root, arvore1, arvore2, etc.) */
    LocalPointerVar local_vars[MAX_LOCAL_VARS];
    int             local_var_count;
} TreeSnapshot;

typedef struct {
    ErrorType error_type;
    int       fault_line;
    uint64_t  fault_addr;
    uint64_t  secondary_addr;
    bool      auto_main_warning;
    bool      has_uninit_ptr_warning;
    char      auto_main_func[64];
    char      raw_trace[1024];
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

    /* Watchdog timer (CLOCK_MONOTONIC) */
    struct timespec  last_cmd_timestamp;
    bool             watchdog_armed;

    /* Exponential Playback Speed (0.05x to 100.0x) & Auto-Play control */
    bool             auto_play;
    float            speed_slider_t;    /* [0.0f .. 1.0f], default 0.391f -> ~1.00x */
    float            play_speed;        /* Computed: 0.05f * powf(2000.0f, speed_slider_t) */
    float            step_accumulator;
    bool             asan_enabled;
    bool             waiting_for_step;
    int              step_bounce_guard; /* Prevents infinite bounce outside student_code.c */

    /* Code & Test Harness inputs */
    char             code_buffer[MAX_CODE_SIZE];
    char             test_input[MAX_TEST_INPUT];
    int              selected_preset;

    /* Snapshot & Diagnostics */
    TreeSnapshot     snapshot;
    DiagnosticReport diagnostic;
} DebuggerSession;

/* ============================================================================
 * 4. UI WORKSPACE, SELECTION ENGINE, SPLITTERS & AVORA BLOB STATE
 * ============================================================================ */

typedef struct {
    /* Hit-tested Code Editor & Full Selection Engine */
    int           sel_anchor_pos;
    int           sel_active_pos;
    bool          is_selecting_with_mouse;
    float         scroll_y;
    float         scroll_x;
    bool          has_focus;
    float         cursor_blink_timer;

    /* Interactive 3-Panel Docking Splitters */
    float         split_vertical_ratio;   /* Default 0.40f, clamped [0.22f .. 0.68f] */
    float         split_horizontal_ratio; /* Default 0.72f, clamped [0.45f .. 0.88f] */
    bool          dragging_v_split;
    bool          dragging_h_split;

    /* 2D Canvas Camera (Pan & Zoom) */
    Camera2D      camera;
    bool          is_panning;
    Vector2       pan_start_mouse;
    Vector2       pan_start_target;

    /* Animated Pedagogical "Blob" Mascot & Spring Tooltips */
    BlobHintTopic active_blob_topic;
    BlobHintTopic displayed_blob_topic;
    float         blob_anim_progress;     /* 0.0f -> 1.0f speech bubble spring animation */
    float         blob_awake_anim;        /* 1.0f = awake & expressive, 0.0f = shrunk into cute idle ball */
    float         blob_idle_timer;        /* Seconds since last interaction/step */
    float         blob_blink_timer;
    Vector2       blob_eye_look;          /* Smooth eye tracking target */
    Rectangle     blob_mascot_bounds;

    /* UI Controls & Blob Dismissal State */
    bool          dropdown_edit_mode;
    bool          test_input_edit_mode;
    bool          blob_error_dismissed;
    ErrorType     last_seen_error_type;
    float         pulse_timer;
    Font          mono_font;
    bool          has_custom_font;
} WorkspaceUI;

/* ============================================================================
 * 5. MODULE FUNCTION PROTOTYPES
 * ============================================================================ */

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
void gdb_poll_and_parse(DebuggerSession *session);
void watchdog_check(DebuggerSession *session);
void load_code_preset(DebuggerSession *session, int preset_index);

/* --- src/layout_renderer.c --- */
void layout_init_camera(WorkspaceUI *ui, Rectangle canvas_bounds);
void layout_center_camera(WorkspaceUI *ui, const TreeSnapshot *snapshot, Rectangle canvas_bounds);
void layout_compute_tree_positions(TreeSnapshot *snapshot, Rectangle canvas_bounds);
void layout_update_animations(TreeSnapshot *snapshot, float dt, float *pulse_timer);
void layout_render_canvas(
    const DebuggerSession *session,
    WorkspaceUI           *ui,
    Rectangle              canvas_bounds
);

/* --- src/ui_workspace.c --- */
void ui_workspace_init(WorkspaceUI *ui, DebuggerSession *session);
void ui_workspace_Unload(WorkspaceUI *ui);
void ui_workspace_update_and_draw(DebuggerSession *session, WorkspaceUI *ui);

#endif /* VISUALIZER_H */
