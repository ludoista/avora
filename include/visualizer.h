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

#define AVORA_VERSION "3.0.0"
#define AVORA_WINDOW_TITLE \
    "AVORA v3.0 — Analisador Visual de Operações e Rastreamento de Árvores (Algoritmos & Estruturas de Dados)"

#define MAX_TREE_NODES       256
#define MAX_FOREST_ROOTS     16
#define MAX_LOCAL_VARS       32
#define MAX_SCALAR_VARS      32
#define MAX_CALL_STACK       32
#define MAX_VISUAL_ARRAYS    4
#define MAX_ARRAY_ELEMENTS   32
#define MAX_CODE_SIZE        65536
#define MAX_TEST_INPUT       512
#define SNAPSHOT_BUF_SIZE    16384
#define MAX_STDOUT_SIZE      16384
#define MAX_UNDO_HISTORY     48
#define WATCHDOG_TIMEOUT_SEC 1.5
#define PRESET_COUNT         18

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
    BLOB_HINT_ERR_FIX             /* Leaf: How do I fix this in C? */
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
 * 4. UI WORKSPACE, SELECTION ENGINE, UNDO/REDO, HUD & AVORA BLOB STATE
 * ============================================================================ */

typedef struct {
    char *code_snapshot;
    int   sel_anchor;
    int   sel_active;
} EditorUndoEntry;

typedef struct {
    /* Hit-tested Code Editor & Full Selection Engine */
    int              sel_anchor_pos;
    int              sel_active_pos;
    bool             is_selecting_with_mouse;
    float            scroll_y;
    float            scroll_x;
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

    /* Retractable Bottom Terminal Drawer (GCC/ASan Diagnostics & Program stdout) */
    bool             terminal_open;
    int              terminal_active_tab;    /* 0 = Diagnóstico / GCC / ASan, 1 = Saída (stdout / printf) */
    float            terminal_anim_h;        /* Smooth height animation [0 .. 225px] */
    float            terminal_scroll_y;

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
    Font             mono_font;
    bool             has_custom_font;
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
    DebuggerSession *session,
    WorkspaceUI     *ui,
    Rectangle        canvas_bounds
);

/* --- src/ui_workspace.c --- */
void ui_workspace_init(WorkspaceUI *ui, DebuggerSession *session);
void ui_workspace_Unload(WorkspaceUI *ui);
void ui_workspace_update_and_draw(DebuggerSession *session, WorkspaceUI *ui);

#endif /* VISUALIZER_H */
