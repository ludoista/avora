#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "visualizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#define CODE_FONT_SIZE 16.0f
#define CODE_LINE_H    21.0f
#define SPLITTER_THICK 6.0f
#define FOOTER_BAR_H   44.0f

/* ============================================================================
 * 1. UNDO / REDO HISTORY ENGINE
 * ============================================================================ */

static void editor_clear_undo_history(WorkspaceUI *ui)
{
    if (!ui) return;
    for (int i = 0; i < MAX_UNDO_HISTORY; i++) {
        if (ui->undo_stack[i].code_snapshot) {
            free(ui->undo_stack[i].code_snapshot);
            ui->undo_stack[i].code_snapshot = NULL;
        }
    }
    ui->undo_count = 0;
    ui->undo_index = -1;
}

static void editor_push_undo(WorkspaceUI *ui, const char *code)
{
    if (!ui || !code) return;

    /* Avoid pushing duplicate consecutive state */
    if (ui->undo_index >= 0 && ui->undo_index < ui->undo_count &&
        ui->undo_stack[ui->undo_index].code_snapshot &&
        strcmp(ui->undo_stack[ui->undo_index].code_snapshot, code) == 0) {
        return;
    }

    /* Discard any redo entries above current undo_index */
    for (int i = ui->undo_index + 1; i < ui->undo_count; i++) {
        if (ui->undo_stack[i].code_snapshot) {
            free(ui->undo_stack[i].code_snapshot);
            ui->undo_stack[i].code_snapshot = NULL;
        }
    }
    ui->undo_count = ui->undo_index + 1;

    if (ui->undo_count >= MAX_UNDO_HISTORY) {
        free(ui->undo_stack[0].code_snapshot);
        memmove(&ui->undo_stack[0], &ui->undo_stack[1], sizeof(EditorUndoEntry) * (MAX_UNDO_HISTORY - 1));
        ui->undo_stack[MAX_UNDO_HISTORY - 1].code_snapshot = NULL;
        ui->undo_count = MAX_UNDO_HISTORY - 1;
    }

    char *copy = strdup(code);
    if (!copy) return;

    ui->undo_stack[ui->undo_count].code_snapshot = copy;
    ui->undo_stack[ui->undo_count].sel_anchor    = ui->sel_anchor_pos;
    ui->undo_stack[ui->undo_count].sel_active    = ui->sel_active_pos;
    ui->undo_index = ui->undo_count;
    ui->undo_count++;
}

static void editor_perform_undo(WorkspaceUI *ui, char *code, size_t max_sz)
{
    if (!ui || !code || ui->undo_index <= 0) return;
    ui->undo_index--;
    const EditorUndoEntry *e = &ui->undo_stack[ui->undo_index];
    if (e->code_snapshot) {
        snprintf(code, max_sz, "%s", e->code_snapshot);
        ui->sel_anchor_pos = e->sel_anchor;
        ui->sel_active_pos = e->sel_active;
    }
}

static void editor_perform_redo(WorkspaceUI *ui, char *code, size_t max_sz)
{
    if (!ui || !code || ui->undo_index + 1 >= ui->undo_count) return;
    ui->undo_index++;
    const EditorUndoEntry *e = &ui->undo_stack[ui->undo_index];
    if (e->code_snapshot) {
        snprintf(code, max_sz, "%s", e->code_snapshot);
        ui->sel_anchor_pos = e->sel_anchor;
        ui->sel_active_pos = e->sel_active;
    }
}

/* ============================================================================
 * 2. WORKSPACE INITIALIZATION & TRUE MONOSPACE FONT LOADING
 * ============================================================================ */

void ui_workspace_init(WorkspaceUI *ui, DebuggerSession *session)
{
    if (!ui) return;
    memset(ui, 0, sizeof(*ui));
    GuiLoadStyleDark();

    ui->split_vertical_ratio   = 0.42f;
    ui->split_horizontal_ratio = 0.72f;
    ui->active_blob_topic      = BLOB_HINT_NONE;
    ui->displayed_blob_topic   = BLOB_HINT_MASCOT;
    ui->blob_awake_anim        = 1.0f;
    ui->blob_idle_timer        = 0.0f;
    ui->camera_auto_focus      = true;
    ui->undo_index             = -1;

    /* Load a system monospace TTF font covering ASCII + Latin-1 Supplement (0x0020..0x00FF) */
    const char *font_candidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        "/usr/share/fonts/adwaita-mono-fonts/AdwaitaMono-Regular.ttf",
        "/usr/share/fonts/dejavu-sans-mono-fonts/DejaVuSansMono.ttf",
        "/usr/share/fonts/liberation-mono-fonts/LiberationMono-Regular.ttf",
        NULL
    };

    int codepoints[540];
    int cp_count = 0;
    for (int c = 0x0020; c <= 0x00FF && cp_count < 540; c++) {
        codepoints[cp_count++] = c;
    }
    int extra_cps[] = {
        0x2013, /* – */
        0x2014, /* — */
        0x2190, /* ← */
        0x2192, /* → */
        0x21BA, /* ↺ */
        0x21BB, /* ↻ */
        0x2316, /* ⌖ */
        0x25B2, /* ▲ */
        0x25B6, /* ▶ */
        0x25B8, /* ▸ */
        0x25BC, /* ▼ */
        0x25BE, /* ▾ */
        0x25A0, /* ■ */
        0x25CF, /* ● */
        0x23ED, /* ⏭ */
        0x23EF, /* ⏯ */
        0x23F9, /* ⏹ */
        0x2705, /* ✅ */
        0x2715, /* ✕ */
        0x26A0  /* ⚠ */
    };
    for (size_t i = 0; i < sizeof(extra_cps) / sizeof(extra_cps[0]) && cp_count < 540; i++) {
        codepoints[cp_count++] = extra_cps[i];
    }

    for (int i = 0; font_candidates[i] != NULL; i++) {
        if (access(font_candidates[i], R_OK) == 0) {
            ui->mono_font = LoadFontEx(font_candidates[i], 20, codepoints, cp_count);
            if (ui->mono_font.texture.id != 0) {
                SetTextureFilter(ui->mono_font.texture, TEXTURE_FILTER_BILINEAR);
                GuiSetFont(ui->mono_font);
                ui->has_custom_font = true;
                break;
            }
        }
    }

    /* Midnight Slate Palette (#161925 Panels, #1F2437 Elevated Surfaces, #2A3047 Subtle Borders) */
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR,     0x161925ff);
    GuiSetStyle(DEFAULT, LINE_COLOR,           0x2a3047ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,    0x1f2437ff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,  0x2a3047ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,    0xe2e8f0ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,   0x282e47ff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED, 0x38bdf8ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,   0xffffffff);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,   0x10b981ff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED, 0x10b981ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,   0x0f111aff);
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED,  0x131622ff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED,0x1e2233ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,  0x64748bff);

    /* Load logo.jpg, resize to 256x256, and set OS window icon */
    char home_logo[512] = { 0 };
    const char *home_env = getenv("HOME");
    if (home_env && home_env[0] != '\0') {
        snprintf(home_logo, sizeof(home_logo), "%s/.local/share/avora/logo.jpg", home_env);
    }
    const char *logo_candidates[] = {
        "logo.jpg",
        "./assets/logo.jpg",
        home_logo[0] ? home_logo : "logo.jpg",
        "/usr/local/share/avora/logo.jpg",
        "/usr/share/avora/logo.jpg",
        NULL
    };
    for (int i = 0; logo_candidates[i] != NULL; i++) {
        if (access(logo_candidates[i], R_OK) == 0) {
            Image logo_img = LoadImage(logo_candidates[i]);
            if (logo_img.data != NULL) {
                ImageResize(&logo_img, 256, 256);
                SetWindowIcon(logo_img);
                UnloadImage(logo_img);
            }
            break;
        }
    }

    Rectangle initial_canvas = { 1440.0f * 0.42f, 0.0f, 1440.0f * 0.58f, 900.0f - FOOTER_BAR_H };
    layout_init_camera(ui, initial_canvas);
    if (session) {
        int len = (int)strlen(session->code_buffer);
        ui->sel_anchor_pos = len;
        ui->sel_active_pos = len;
        editor_push_undo(ui, session->code_buffer);
    }
}

void ui_workspace_Unload(WorkspaceUI *ui)
{
    if (!ui) return;
    editor_clear_undo_history(ui);
    if (ui->has_custom_font && ui->mono_font.texture.id != 0) {
        UnloadFont(ui->mono_font);
        ui->has_custom_font = false;
    }
}

/* ============================================================================
 * 3. UTF-8 NAVIGATION, WORD JUMPING, EXACT MEASUREMENT & SELECTION ENGINE
 * ============================================================================ */

static int utf8_prev_char_pos(const char *buf, int pos)
{
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && (((unsigned char)buf[pos] & 0xC0) == 0x80)) {
        pos--;
    }
    return pos;
}

static int utf8_next_char_pos(const char *buf, int pos, int len)
{
    if (pos >= len) return len;
    pos++;
    while (pos < len && (((unsigned char)buf[pos] & 0xC0) == 0x80)) {
        pos++;
    }
    return pos;
}

static bool is_word_char(char c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static int find_prev_word_pos(const char *buf, int pos)
{
    if (pos <= 0) return 0;
    pos = utf8_prev_char_pos(buf, pos);
    while (pos > 0 && isspace((unsigned char)buf[pos]) && buf[pos] != '\n') {
        pos = utf8_prev_char_pos(buf, pos);
    }
    if (pos > 0 && is_word_char(buf[pos])) {
        while (pos > 0 && is_word_char(buf[pos - 1])) {
            pos--;
        }
    }
    return pos;
}

static int find_next_word_pos(const char *buf, int pos, int len)
{
    if (pos >= len) return len;
    if (is_word_char(buf[pos])) {
        while (pos < len && is_word_char(buf[pos])) pos++;
    } else if (!isspace((unsigned char)buf[pos])) {
        pos = utf8_next_char_pos(buf, pos, len);
    }
    while (pos < len && isspace((unsigned char)buf[pos]) && buf[pos] != '\n') {
        pos++;
    }
    return pos;
}

static float measure_line_prefix_width(Font font, const char *line_start, int byte_count, float font_size)
{
    if (!line_start || byte_count <= 0) return 0.0f;
    char prefix_buf[1024];
    if (byte_count >= (int)sizeof(prefix_buf)) {
        byte_count = (int)sizeof(prefix_buf) - 1;
    }
    memcpy(prefix_buf, line_start, (size_t)byte_count);
    prefix_buf[byte_count] = '\0';
    return MeasureTextEx(font, prefix_buf, font_size, 1.0f).x;
}

static int hit_test_editor_index(
    Font        font,
    const char *code,
    Vector2     mouse,
    float       text_start_x,
    float       text_start_y,
    float       scroll_x,
    float       scroll_y
)
{
    int code_len = (int)strlen(code);
    int target_line = (int)floorf((mouse.y - text_start_y + scroll_y) / CODE_LINE_H) + 1;
    if (target_line < 1) target_line = 1;

    int cur_line = 1;
    int line_start = 0;
    while (line_start < code_len && cur_line < target_line) {
        if (code[line_start] == '\n') cur_line++;
        line_start++;
    }

    int line_end = line_start;
    while (line_end < code_len && code[line_end] != '\n') {
        line_end++;
    }
    int line_len = line_end - line_start;

    float target_x = mouse.x - text_start_x + scroll_x;
    if (target_x <= 0.0f) return line_start;

    int   best_col  = 0;
    float best_diff = fabsf(target_x);

    int col = 0;
    while (col < line_len) {
        int next_col = utf8_next_char_pos(code + line_start, col, line_len);
        float w = measure_line_prefix_width(font, code + line_start, next_col, CODE_FONT_SIZE);
        float diff = fabsf(target_x - w);
        if (diff <= best_diff) {
            best_diff = diff;
            best_col  = next_col;
        } else {
            break;
        }
        col = next_col;
    }

    return line_start + best_col;
}

static bool editor_has_selection(const WorkspaceUI *ui)
{
    return ui->sel_anchor_pos != ui->sel_active_pos;
}

static void editor_get_selection_range(const WorkspaceUI *ui, int code_len, int *out_start, int *out_end)
{
    int a = ui->sel_anchor_pos;
    int b = ui->sel_active_pos;
    if (a < 0) a = 0;
    if (b < 0) b = 0;
    if (a > code_len) a = code_len;
    if (b > code_len) b = code_len;
    *out_start = (a < b) ? a : b;
    *out_end   = (a > b) ? a : b;
}

static bool editor_delete_selection(char *code, WorkspaceUI *ui)
{
    if (!editor_has_selection(ui)) return false;
    int len = (int)strlen(code);
    int s = 0, e = 0;
    editor_get_selection_range(ui, len, &s, &e);
    if (e > s) {
        memmove(code + s, code + e, (size_t)(len - e + 1));
    }
    ui->sel_anchor_pos = s;
    ui->sel_active_pos = s;
    return true;
}

static void editor_insert_text(char *code, size_t max_sz, WorkspaceUI *ui, const char *ins)
{
    if (!code || !ui || !ins) return;
    editor_delete_selection(code, ui);

    char clean[16384];
    size_t w = 0;
    for (size_t r = 0; ins[r] != '\0' && w < sizeof(clean) - 1; r++) {
        if (ins[r] != '\r') {
            clean[w++] = ins[r];
        }
    }
    clean[w] = '\0';
    if (w == 0) return;

    size_t cur_len = strlen(code);
    int cursor = ui->sel_active_pos;
    if (cursor < 0) cursor = 0;
    if ((size_t)cursor > cur_len) cursor = (int)cur_len;
    if (cur_len + w >= max_sz - 1) return;

    memmove(code + cursor + w, code + cursor, cur_len - (size_t)cursor + 1);
    memcpy(code + cursor, clean, w);
    cursor += (int)w;
    ui->sel_anchor_pos = cursor;
    ui->sel_active_pos = cursor;
}

static void editor_copy_selection_to_clipboard(const char *code, const WorkspaceUI *ui)
{
    int len = (int)strlen(code);
    if (!editor_has_selection(ui)) {
        SetClipboardText(code);
        return;
    }
    int s = 0, e = 0;
    editor_get_selection_range(ui, len, &s, &e);
    int slen = e - s;
    if (slen <= 0) return;

    char *clip = (char *)malloc((size_t)slen + 1);
    if (clip) {
        memcpy(clip, code + s, (size_t)slen);
        clip[slen] = '\0';
        SetClipboardText(clip);
        free(clip);
    }
}

/* Keep active cursor inside visible viewport both horizontally and vertically */
static void editor_ensure_cursor_visible(Font font, const char *code, WorkspaceUI *ui, Rectangle editor_bounds, float gutter_w)
{
    int total_len = (int)strlen(code);
    int cur = ui->sel_active_pos;
    if (cur < 0) cur = 0;
    if (cur > total_len) cur = total_len;

    int line_num = 1;
    int line_start = 0;
    for (int i = 0; i < cur; i++) {
        if (code[i] == '\n') {
            line_num++;
            line_start = i + 1;
        }
    }

    float cursor_y = (float)(line_num - 1) * CODE_LINE_H;
    float view_h   = editor_bounds.height - 20.0f;
    if (cursor_y < ui->scroll_y) {
        ui->scroll_y = cursor_y;
    } else if (cursor_y + CODE_LINE_H > ui->scroll_y + view_h) {
        ui->scroll_y = fmaxf(0.0f, cursor_y + CODE_LINE_H - view_h);
    }

    float cursor_x = measure_line_prefix_width(font, code + line_start, cur - line_start, CODE_FONT_SIZE);
    float view_w   = editor_bounds.width - gutter_w - 28.0f;
    if (cursor_x < ui->scroll_x) {
        ui->scroll_x = fmaxf(0.0f, cursor_x - 24.0f);
    } else if (cursor_x > ui->scroll_x + view_w) {
        ui->scroll_x = cursor_x - view_w + 24.0f;
    }
}

/* ============================================================================
 * 4. C SYNTAX HIGHLIGHTER & SCROLLABLE MIDNIGHT SLATE EDITOR
 * ============================================================================ */

static bool is_c_keyword(const char *word, size_t len)
{
    static const char *keywords[] = {
        "struct", "int", "void", "char", "long", "float", "double", "bool",
        "if", "else", "while", "for", "return", "break", "continue", "switch",
        "case", "default", "sizeof", "typedef", "const", "static", "unsigned"
    };
    for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
        if (strlen(keywords[i]) == len && strncmp(word, keywords[i], len) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_c_builtin_or_type(const char *word, size_t len)
{
    static const char *types[] = {
        "TreeNode", "ListNode", "BUILD_TREE", "BUILD_LIST", "new_node", "new_list_node",
        "bst_insert", "NULL", "malloc", "free", "calloc", "printf", "puts", "true", "false"
    };
    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (strlen(types[i]) == len && strncmp(word, types[i], len) == 0) {
            return true;
        }
    }
    return false;
}

static void draw_syntax_colored_line(Font font, const char *line_str, Vector2 pos, float font_size)
{
    const char *p = line_str;
    float y = pos.y;

    while (*p) {
        int byte_offset = (int)(p - line_str);
        float exact_x   = pos.x + measure_line_prefix_width(font, line_str, byte_offset, font_size);

        if (p[0] == '/' && p[1] == '/') {
            DrawTextEx(font, p, (Vector2){ exact_x, y }, font_size, 1.0f, (Color){ 100, 116, 139, 255 });
            return;
        }

        if (*p == '"') {
            const char *start = p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p += 2;
                else p++;
            }
            if (*p == '"') p++;
            size_t len = (size_t)(p - start);
            char tok[256];
            if (len >= sizeof(tok)) len = sizeof(tok) - 1;
            memcpy(tok, start, len);
            tok[len] = '\0';
            DrawTextEx(font, tok, (Vector2){ exact_x, y }, font_size, 1.0f, (Color){ 52, 211, 153, 255 });
            continue;
        }

        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            size_t len = (size_t)(p - start);
            char tok[128];
            if (len >= sizeof(tok)) len = sizeof(tok) - 1;
            memcpy(tok, start, len);
            tok[len] = '\0';

            Color col = { 226, 232, 240, 255 };
            if (is_c_keyword(tok, len)) {
                col = (Color){ 244, 114, 182, 255 }; /* Pink/Magenta keywords */
            } else if (is_c_builtin_or_type(tok, len)) {
                col = (Color){ 56, 189, 248, 255 };  /* Cyan types/builtins */
            } else if (*p == '(') {
                col = (Color){ 250, 204, 21, 255 };  /* Amber/Yellow function calls */
            }

            DrawTextEx(font, tok, (Vector2){ exact_x, y }, font_size, 1.0f, col);
        } else if (isdigit((unsigned char)*p)) {
            const char *start = p;
            while (isalnum((unsigned char)*p)) p++;
            size_t len = (size_t)(p - start);
            char tok[64];
            if (len >= sizeof(tok)) len = sizeof(tok) - 1;
            memcpy(tok, start, len);
            tok[len] = '\0';

            DrawTextEx(font, tok, (Vector2){ exact_x, y }, font_size, 1.0f, (Color){ 167, 139, 250, 255 });
        } else {
            int next_off = utf8_next_char_pos(p, 0, (int)strlen(p));
            char ch[8];
            if (next_off >= (int)sizeof(ch)) next_off = (int)sizeof(ch) - 1;
            memcpy(ch, p, (size_t)next_off);
            ch[next_off] = '\0';

            Color col = (next_off == 1 && strchr("{}()[];->=<>+-*/&|!", *p) != NULL)
                        ? (Color){ 148, 163, 184, 255 }
                        : (Color){ 226, 232, 240, 255 };
            DrawTextEx(font, ch, (Vector2){ exact_x, y }, font_size, 1.0f, col);
            p += next_off;
        }
    }
}

static void update_and_draw_code_editor(
    DebuggerSession *session,
    WorkspaceUI     *ui,
    Rectangle        editor_bounds
)
{
    Font font = GuiGetFont();
    char *code = session->code_buffer;
    int code_len = (int)strlen(code);

    if (ui->sel_anchor_pos > code_len) ui->sel_anchor_pos = code_len;
    if (ui->sel_anchor_pos < 0)        ui->sel_anchor_pos = 0;
    if (ui->sel_active_pos > code_len) ui->sel_active_pos = code_len;
    if (ui->sel_active_pos < 0)        ui->sel_active_pos = 0;

    Vector2 mouse = GetMousePosition();
    bool hovered = CheckCollisionPointRec(mouse, editor_bounds) &&
                   !ui->dropdown_edit_mode &&
                   !ui->dragging_v_split;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui->has_focus = hovered;
        if (!hovered) {
            ui->is_selecting_with_mouse = false;
        }
    }

    /* Mouse wheel scrolling (vertical & horizontal with Shift) */
    if (hovered) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                ui->scroll_x = fmaxf(0.0f, ui->scroll_x - wheel * 42.0f);
            } else {
                ui->scroll_y = fmaxf(0.0f, ui->scroll_y - wheel * CODE_LINE_H * 2.5f);
            }
        }
    }

    float gutter_w     = 44.0f;
    float text_start_x = editor_bounds.x + gutter_w + 10.0f;
    float text_start_y = editor_bounds.y + 8.0f;

    /* Mouse Click, Double-Click (Word), Triple-Click (Line) & Drag Selection */
    bool shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int hit = hit_test_editor_index(font, code, mouse, text_start_x, text_start_y, ui->scroll_x, ui->scroll_y);
        double now = GetTime();
        float click_dist = Vector2Distance(mouse, ui->last_click_pos);

        if ((now - ui->last_click_time) < 0.38 && click_dist < 10.0f) {
            ui->click_streak++;
        } else {
            ui->click_streak = 1;
        }
        ui->last_click_time = now;
        ui->last_click_pos  = mouse;

        if (ui->click_streak == 2) {
            /* Double-click: Select word under cursor */
            int ws = hit;
            int we = hit;
            while (ws > 0 && is_word_char(code[ws - 1])) ws--;
            while (we < code_len && is_word_char(code[we])) we++;
            ui->sel_anchor_pos          = ws;
            ui->sel_active_pos          = we;
            ui->is_selecting_with_mouse = false;
        } else if (ui->click_streak >= 3) {
            /* Triple-click: Select entire line */
            int ls = hit;
            int le = hit;
            while (ls > 0 && code[ls - 1] != '\n') ls--;
            while (le < code_len && code[le] != '\n') le++;
            if (le < code_len && code[le] == '\n') le++;
            ui->sel_anchor_pos          = ls;
            ui->sel_active_pos          = le;
            ui->is_selecting_with_mouse = false;
        } else {
            if (!shift_down) {
                ui->sel_anchor_pos = hit;
            }
            ui->sel_active_pos          = hit;
            ui->is_selecting_with_mouse = true;
        }
        ui->cursor_blink_timer = 0.0f;
    }

    if (ui->is_selecting_with_mouse) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            int hit = hit_test_editor_index(font, code, mouse, text_start_x, text_start_y, ui->scroll_x, ui->scroll_y);
            ui->sel_active_pos = hit;
            ui->cursor_blink_timer = 0.0f;

            if (mouse.y < editor_bounds.y + 18.0f) {
                ui->scroll_y = fmaxf(0.0f, ui->scroll_y - CODE_LINE_H * 0.35f);
            } else if (mouse.y > editor_bounds.y + editor_bounds.height - 18.0f) {
                ui->scroll_y += CODE_LINE_H * 0.35f;
            }
        } else {
            ui->is_selecting_with_mouse = false;
        }
    }

    /* Keyboard Editing, Undo/Redo, Word Navigation & Shortcuts */
    if (ui->has_focus) {
        ui->cursor_blink_timer += GetFrameTime();
        bool ctrl   = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool moved  = false;
        bool edited = false;

        if (ctrl && IsKeyPressed(KEY_Z)) {
            if (shift_down) {
                editor_perform_redo(ui, code, MAX_CODE_SIZE);
            } else {
                editor_perform_undo(ui, code, MAX_CODE_SIZE);
            }
            moved = true;
        } else if (ctrl && IsKeyPressed(KEY_Y)) {
            editor_perform_redo(ui, code, MAX_CODE_SIZE);
            moved = true;
        } else if (ctrl && IsKeyPressed(KEY_A)) {
            ui->sel_anchor_pos = 0;
            ui->sel_active_pos = (int)strlen(code);
        } else if (ctrl && IsKeyPressed(KEY_C)) {
            editor_copy_selection_to_clipboard(code, ui);
        } else if (ctrl && IsKeyPressed(KEY_X)) {
            editor_copy_selection_to_clipboard(code, ui);
            editor_delete_selection(code, ui);
            edited = true;
        } else if (ctrl && IsKeyPressed(KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip && clip[0] != '\0') {
                editor_insert_text(code, MAX_CODE_SIZE, ui, clip);
                edited = true;
            }
        } else if (!ctrl) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && ch < 127) {
                    char ins[2] = { (char)ch, '\0' };
                    editor_insert_text(code, MAX_CODE_SIZE, ui, ins);
                    edited = true;
                }
                ch = GetCharPressed();
            }
        }

        if (IsKeyPressed(KEY_TAB)) {
            if (shift_down) {
                /* Shift+Tab: Unindent current line by up to 4 spaces */
                int ls = ui->sel_active_pos;
                while (ls > 0 && code[ls - 1] != '\n') ls--;
                int rem = 0;
                while (rem < 4 && code[ls + rem] == ' ') rem++;
                if (rem > 0) {
                    int len = (int)strlen(code);
                    memmove(code + ls, code + ls + rem, (size_t)(len - (ls + rem) + 1));
                    ui->sel_active_pos = (ui->sel_active_pos - rem >= ls) ? (ui->sel_active_pos - rem) : ls;
                    ui->sel_anchor_pos = ui->sel_active_pos;
                    edited = true;
                }
            } else {
                editor_insert_text(code, MAX_CODE_SIZE, ui, "    ");
                edited = true;
            }
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressedRepeat(KEY_ENTER)) {
            editor_delete_selection(code, ui);
            int cur = ui->sel_active_pos;
            int line_start = cur;
            while (line_start > 0 && code[line_start - 1] != '\n') line_start--;
            int spaces = 0;
            while (code[line_start + spaces] == ' ' && spaces < 24) spaces++;
            if (cur > 0 && code[cur - 1] == '{') spaces += 4;

            char nl_indent[32];
            nl_indent[0] = '\n';
            for (int i = 0; i < spaces && i < 28; i++) nl_indent[1 + i] = ' ';
            nl_indent[1 + spaces] = '\0';
            editor_insert_text(code, MAX_CODE_SIZE, ui, nl_indent);
            edited = true;
        }

        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (!editor_delete_selection(code, ui)) {
                int cur = ui->sel_active_pos;
                if (cur > 0) {
                    int prev = ctrl ? find_prev_word_pos(code, cur) : utf8_prev_char_pos(code, cur);
                    int len  = (int)strlen(code);
                    memmove(code + prev, code + cur, (size_t)(len - cur + 1));
                    ui->sel_anchor_pos = prev;
                    ui->sel_active_pos = prev;
                }
            }
            edited = true;
        }

        if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
            if (!editor_delete_selection(code, ui)) {
                int cur = ui->sel_active_pos;
                int len = (int)strlen(code);
                if (cur < len) {
                    int next = ctrl ? find_next_word_pos(code, cur, len) : utf8_next_char_pos(code, cur, len);
                    memmove(code + cur, code + next, (size_t)(len - next + 1));
                }
            }
            edited = true;
        }

        /* Cursor Navigation + Ctrl Word Jumping + Shift Selection Extension */
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
            if (!shift_down && editor_has_selection(ui)) {
                int s = 0, e = 0;
                editor_get_selection_range(ui, (int)strlen(code), &s, &e);
                ui->sel_anchor_pos = s;
                ui->sel_active_pos = s;
            } else {
                ui->sel_active_pos = ctrl
                                     ? find_prev_word_pos(code, ui->sel_active_pos)
                                     : utf8_prev_char_pos(code, ui->sel_active_pos);
                if (!shift_down) ui->sel_anchor_pos = ui->sel_active_pos;
            }
            moved = true;
        }

        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
            int len = (int)strlen(code);
            if (!shift_down && editor_has_selection(ui)) {
                int s = 0, e = 0;
                editor_get_selection_range(ui, len, &s, &e);
                ui->sel_anchor_pos = e;
                ui->sel_active_pos = e;
            } else {
                ui->sel_active_pos = ctrl
                                     ? find_next_word_pos(code, ui->sel_active_pos, len)
                                     : utf8_next_char_pos(code, ui->sel_active_pos, len);
                if (!shift_down) ui->sel_anchor_pos = ui->sel_active_pos;
            }
            moved = true;
        }

        if (IsKeyPressed(KEY_HOME)) {
            int ls = ui->sel_active_pos;
            while (ls > 0 && code[ls - 1] != '\n') ls--;
            int first_non_space = ls;
            while (code[first_non_space] == ' ') first_non_space++;
            ui->sel_active_pos = (ui->sel_active_pos == first_non_space) ? ls : first_non_space;
            if (!shift_down) ui->sel_anchor_pos = ui->sel_active_pos;
            moved = true;
        }

        if (IsKeyPressed(KEY_END)) {
            int le = ui->sel_active_pos;
            while (code[le] && code[le] != '\n') le++;
            ui->sel_active_pos = le;
            if (!shift_down) ui->sel_anchor_pos = le;
            moved = true;
        }

        if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
            int ls = ui->sel_active_pos;
            while (ls > 0 && code[ls - 1] != '\n') ls--;
            float cur_px = measure_line_prefix_width(font, code + ls, ui->sel_active_pos - ls, CODE_FONT_SIZE);
            if (ls > 0) {
                int prev_end = ls - 1;
                int prev_ls  = prev_end;
                while (prev_ls > 0 && code[prev_ls - 1] != '\n') prev_ls--;
                int prev_len = prev_end - prev_ls;

                int best_col = 0;
                float best_d = fabsf(cur_px);
                int c = 0;
                while (c < prev_len) {
                    int nc = utf8_next_char_pos(code + prev_ls, c, prev_len);
                    float w = measure_line_prefix_width(font, code + prev_ls, nc, CODE_FONT_SIZE);
                    if (fabsf(cur_px - w) <= best_d) {
                        best_d   = fabsf(cur_px - w);
                        best_col = nc;
                    }
                    c = nc;
                }
                ui->sel_active_pos = prev_ls + best_col;
                if (!shift_down) ui->sel_anchor_pos = ui->sel_active_pos;
            }
            moved = true;
        }

        if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
            int ls = ui->sel_active_pos;
            while (ls > 0 && code[ls - 1] != '\n') ls--;
            float cur_px = measure_line_prefix_width(font, code + ls, ui->sel_active_pos - ls, CODE_FONT_SIZE);
            int le = ui->sel_active_pos;
            while (code[le] && code[le] != '\n') le++;
            if (code[le] == '\n') {
                int next_ls = le + 1;
                int next_le = next_ls;
                while (code[next_le] && code[next_le] != '\n') next_le++;
                int next_len = next_le - next_ls;

                int best_col = 0;
                float best_d = fabsf(cur_px);
                int c = 0;
                while (c < next_len) {
                    int nc = utf8_next_char_pos(code + next_ls, c, next_len);
                    float w = measure_line_prefix_width(font, code + next_ls, nc, CODE_FONT_SIZE);
                    if (fabsf(cur_px - w) <= best_d) {
                        best_d   = fabsf(cur_px - w);
                        best_col = nc;
                    }
                    c = nc;
                }
                ui->sel_active_pos = next_ls + best_col;
                if (!shift_down) ui->sel_anchor_pos = ui->sel_active_pos;
            }
            moved = true;
        }

        if (edited) {
            editor_push_undo(ui, code);
        }
        if (edited || moved) {
            ui->cursor_blink_timer = 0.0f;
            editor_ensure_cursor_visible(font, code, ui, editor_bounds, gutter_w);
        }
    }

    /* Render Midnight Slate Editor Background (#161925) & Line Gutter (#121520) */
    DrawRectangleRec(editor_bounds, (Color){ 22, 25, 37, 255 });
    Rectangle gutter_rect = { editor_bounds.x, editor_bounds.y, gutter_w, editor_bounds.height };
    DrawRectangleRec(gutter_rect, (Color){ 16, 19, 29, 255 });
    DrawLineEx(
        (Vector2){ editor_bounds.x + gutter_w, editor_bounds.y },
        (Vector2){ editor_bounds.x + gutter_w, editor_bounds.y + editor_bounds.height },
        1.0f,
        (Color){ 42, 48, 71, 255 }
    );

    BeginScissorMode((int)editor_bounds.x, (int)editor_bounds.y, (int)editor_bounds.width, (int)editor_bounds.height);

    int active_line = (session->diagnostic.error_type == ERR_COMPILE)
                      ? session->diagnostic.fault_line
                      : session->snapshot.current_executing_line;
    bool is_error_line = (session->diagnostic.error_type != ERR_NONE);

    int total_len = (int)strlen(code);
    int sel_min = 0, sel_max = 0;
    editor_get_selection_range(ui, total_len, &sel_min, &sel_max);

    int   line_num   = 1;
    int   idx        = 0;
    float max_line_w = 0.0f;

    while (idx <= total_len) {
        int line_end = idx;
        while (line_end < total_len && code[line_end] != '\n') line_end++;

        int llen = line_end - idx;
        float line_w = measure_line_prefix_width(font, code + idx, llen, CODE_FONT_SIZE);
        if (line_w > max_line_w) max_line_w = line_w;

        float y = text_start_y + (float)(line_num - 1) * CODE_LINE_H - ui->scroll_y;

        if (y + CODE_LINE_H >= editor_bounds.y && y <= editor_bounds.y + editor_bounds.height) {
            /* 1. Highlight active debugger line (#F59E0B Amber) or fault line (#F43F5E Crimson) */
            if (session->state != STATE_IDLE && line_num == active_line) {
                Color hl  = is_error_line ? (Color){ 244, 63, 94, 55 }  : (Color){ 245, 158, 11, 45 };
                Color bar = is_error_line ? (Color){ 244, 63, 94, 255 } : (Color){ 245, 158, 11, 255 };
                DrawRectangle(
                    (int)(editor_bounds.x + gutter_w + 1.0f),
                    (int)y,
                    (int)(editor_bounds.width - gutter_w - 2.0f),
                    (int)CODE_LINE_H,
                    hl
                );
                DrawRectangle((int)(editor_bounds.x + gutter_w + 1.0f), (int)y, 4, (int)CODE_LINE_H, bar);
            }

            /* 2. Line number in gutter (#64748B or #F59E0B when active) */
            char lnum_str[16];
            snprintf(lnum_str, sizeof(lnum_str), "%2d", line_num);
            Color lnum_col = (session->state != STATE_IDLE && line_num == active_line)
                             ? (Color){ 245, 158, 11, 255 }
                             : (Color){ 100, 116, 139, 225 };
            DrawTextEx(font, lnum_str, (Vector2){ editor_bounds.x + 10.0f, y + 2.0f }, 15.0f, 1.0f, lnum_col);

            /* 3. Clip code text area to the right of gutter */
            BeginScissorMode(
                (int)(editor_bounds.x + gutter_w + 2.0f),
                (int)editor_bounds.y,
                (int)(editor_bounds.width - gutter_w - 4.0f),
                (int)editor_bounds.height
            );

            char line_buf[1024];
            if (llen >= (int)sizeof(line_buf)) llen = (int)sizeof(line_buf) - 1;
            memcpy(line_buf, code + idx, (size_t)llen);
            line_buf[llen] = '\0';

            float draw_x = text_start_x - ui->scroll_x;

            /* 4. Visual Selection Highlight Rectangle */
            if (sel_max > sel_min && sel_max > idx && sel_min <= line_end) {
                int local_s = (sel_min > idx) ? (sel_min - idx) : 0;
                int local_e = (sel_max < line_end) ? (sel_max - idx) : llen;
                if (local_s > llen) local_s = llen;
                if (local_e > llen) local_e = llen;

                float sx = draw_x + measure_line_prefix_width(font, line_buf, local_s, CODE_FONT_SIZE);
                float ex = draw_x + measure_line_prefix_width(font, line_buf, local_e, CODE_FONT_SIZE);
                if (sel_max > line_end) ex += 8.0f;
                if (ex > sx) {
                    DrawRectangleRec(
                        (Rectangle){ sx, y + 1.0f, ex - sx, CODE_LINE_H - 1.0f },
                        (Color){ 56, 189, 248, 55 }
                    );
                }
            }

            /* 5. Syntax-colored characters */
            draw_syntax_colored_line(font, line_buf, (Vector2){ draw_x, y + 2.0f }, CODE_FONT_SIZE);

            /* 6. Blinking caret at sel_active_pos */
            if (ui->has_focus && ui->sel_active_pos >= idx && ui->sel_active_pos <= line_end) {
                if (((int)(ui->cursor_blink_timer * 2.2f) % 2) == 0) {
                    int col_bytes = ui->sel_active_pos - idx;
                    if (col_bytes > llen) col_bytes = llen;
                    float cx = draw_x + measure_line_prefix_width(font, line_buf, col_bytes, CODE_FONT_SIZE);
                    DrawRectangle((int)cx, (int)(y + 2.0f), 2, (int)(CODE_LINE_H - 3.0f), (Color){ 56, 189, 248, 255 });
                }
            }
            EndScissorMode();
        }

        if (line_end == total_len) break;
        idx = line_end + 1;
        line_num++;
    }

    /* Subtle Horizontal & Vertical Scrollbars when content overflows */
    float content_h = (float)line_num * CODE_LINE_H + 16.0f;
    if (content_h > editor_bounds.height) {
        float sb_h = fmaxf(24.0f, (editor_bounds.height / content_h) * editor_bounds.height);
        float sb_y = editor_bounds.y + (ui->scroll_y / (content_h - editor_bounds.height)) * (editor_bounds.height - sb_h);
        DrawRectangleRounded(
            (Rectangle){ editor_bounds.x + editor_bounds.width - 6.0f, sb_y, 4.0f, sb_h },
            0.5f, 4, (Color){ 100, 116, 139, 130 }
        );
    }
    float view_w = editor_bounds.width - gutter_w - 16.0f;
    if (max_line_w > view_w) {
        float sb_w = fmaxf(28.0f, (view_w / (max_line_w + 40.0f)) * view_w);
        float sb_x = editor_bounds.x + gutter_w + (ui->scroll_x / (max_line_w + 40.0f - view_w)) * (view_w - sb_w);
        DrawRectangleRounded(
            (Rectangle){ sb_x, editor_bounds.y + editor_bounds.height - 6.0f, sb_w, 4.0f },
            0.5f, 4, (Color){ 100, 116, 139, 130 }
        );
    }

    EndScissorMode();
    DrawRectangleLinesEx(editor_bounds, 1.0f, ui->has_focus ? (Color){ 56, 189, 248, 180 } : (Color){ 42, 48, 71, 255 });
}

/* ============================================================================
 * 5. RETRACTABLE TERMINAL DRAWER (GCC / ASAN LOGS & REAL-TIME STDOUT)
 * ============================================================================ */

static void draw_multiline_log(Font font, const char *text, Rectangle bounds, float scroll_y, float font_size, Color default_col)
{
    if (!text || text[0] == '\0') return;
    float y = bounds.y - scroll_y;
    const char *p = text;

    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);

        if (y + font_size >= bounds.y - 4.0f && y <= bounds.y + bounds.height) {
            char line[512];
            if (len >= sizeof(line)) len = sizeof(line) - 1;
            memcpy(line, p, len);
            line[len] = '\0';

            Color lcol = default_col;
            if (strstr(line, "error:") != NULL || strstr(line, "ERROR:") != NULL || strstr(line, "SIGSEGV") != NULL) {
                lcol = (Color){ 251, 113, 133, 255 };
            } else if (strstr(line, "warning:") != NULL || strstr(line, "Nota:") != NULL) {
                lcol = (Color){ 250, 204, 21, 255 };
            }
            DrawTextEx(font, line, (Vector2){ bounds.x, y }, font_size, 1.0f, lcol);
        }

        y += font_size + 5.0f;
        if (!nl) break;
        p = nl + 1;
    }
}

static void draw_retractable_terminal(
    DebuggerSession *session,
    WorkspaceUI     *ui,
    float            sw,
    float            footer_top_y,
    Font             font
)
{
    float dt = GetFrameTime();
    float target_h = ui->terminal_open ? 215.0f : 0.0f;
    ui->terminal_anim_h += (target_h - ui->terminal_anim_h) * fminf(1.0f, dt * 14.0f);
    if (ui->terminal_anim_h < 1.5f) {
        if (!ui->terminal_open) ui->terminal_anim_h = 0.0f;
        return;
    }

    Rectangle drawer = {
        0.0f,
        footer_top_y - ui->terminal_anim_h,
        sw,
        ui->terminal_anim_h
    };
    Vector2 mouse = GetMousePosition();
    if (CheckCollisionPointRec(mouse, drawer)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            ui->terminal_scroll_y = fmaxf(0.0f, ui->terminal_scroll_y - wheel * 28.0f);
        }
    }

    /* Drawer Surface & Top Border */
    DrawRectangleRec(drawer, (Color){ 15, 17, 26, 248 });
    DrawLineEx((Vector2){ 0.0f, drawer.y }, (Vector2){ sw, drawer.y }, 1.5f, (Color){ 56, 189, 248, 200 });

    /* Drawer Header Bar with Tabs */
    Rectangle hdr = { drawer.x, drawer.y, drawer.width, 32.0f };
    DrawRectangleRec(hdr, (Color){ 22, 25, 37, 255 });
    DrawLineEx((Vector2){ 0.0f, hdr.y + hdr.height }, (Vector2){ sw, hdr.y + hdr.height }, 1.0f, (Color){ 42, 48, 71, 255 });

    /* Tab 0: Diagnóstico & Compilador (GCC / ASan) */
    Rectangle tab0 = { 12.0f, hdr.y + 4.0f, 265.0f, 24.0f };
    bool hov0 = CheckCollisionPointRec(mouse, tab0);
    DrawRectangleRounded(tab0, 0.25f, 4, (ui->terminal_active_tab == 0) ? (Color){ 31, 36, 55, 255 } : (Color){ 22, 25, 37, 255 });
    DrawRectangleRoundedLinesEx(tab0, 0.25f, 4, 1.0f, (ui->terminal_active_tab == 0) ? (Color){ 56, 189, 248, 255 } : (Color){ 42, 48, 71, 255 });
    DrawTextEx(font, "Diagnóstico & Compilador (GCC/ASan)", (Vector2){ tab0.x + 10.0f, tab0.y + 5.0f }, 13.0f, 1.0f,
               (ui->terminal_active_tab == 0 || hov0) ? (Color){ 226, 232, 240, 255 } : (Color){ 100, 116, 139, 255 });
    if (hov0 && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->terminal_active_tab = 0;
        ui->terminal_scroll_y   = 0.0f;
    }

    /* Tab 1: Saída do Programa (stdout / printf) */
    Rectangle tab1 = { 286.0f, hdr.y + 4.0f, 250.0f, 24.0f };
    bool hov1 = CheckCollisionPointRec(mouse, tab1);
    DrawRectangleRounded(tab1, 0.25f, 4, (ui->terminal_active_tab == 1) ? (Color){ 31, 36, 55, 255 } : (Color){ 22, 25, 37, 255 });
    DrawRectangleRoundedLinesEx(tab1, 0.25f, 4, 1.0f, (ui->terminal_active_tab == 1) ? (Color){ 16, 185, 129, 255 } : (Color){ 42, 48, 71, 255 });
    const char *t1_lbl = session->has_new_stdout ? "● Saída do Programa (stdout / printf)" : "Saída do Programa (stdout / printf)";
    DrawTextEx(font, t1_lbl, (Vector2){ tab1.x + 10.0f, tab1.y + 5.0f }, 13.0f, 1.0f,
               (ui->terminal_active_tab == 1 || hov1) ? (Color){ 226, 232, 240, 255 } :
               (session->has_new_stdout ? (Color){ 52, 211, 153, 255 } : (Color){ 100, 116, 139, 255 }));
    if (hov1 && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->terminal_active_tab = 1;
        ui->terminal_scroll_y   = 0.0f;
        session->has_new_stdout = false;
    }

    /* Close Button [ ✕ Ocultar ▼ ] */
    Rectangle btn_close = { sw - 110.0f, hdr.y + 4.0f, 98.0f, 24.0f };
    bool hov_close = CheckCollisionPointRec(mouse, btn_close);
    DrawRectangleRounded(btn_close, 0.25f, 4, hov_close ? (Color){ 40, 46, 71, 255 } : (Color){ 31, 36, 55, 255 });
    DrawTextEx(font, "✕ Ocultar ▼", (Vector2){ btn_close.x + 10.0f, btn_close.y + 5.0f }, 13.0f, 1.0f, (Color){ 226, 232, 240, 255 });
    if (hov_close && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->terminal_open = false;
    }

    /* Drawer Content Body */
    Rectangle content = {
        drawer.x + 16.0f,
        drawer.y + 38.0f,
        drawer.width - 32.0f,
        drawer.height - 44.0f
    };
    if (content.height <= 4.0f) return;

    BeginScissorMode((int)content.x, (int)content.y, (int)content.width, (int)content.height);
    if (ui->terminal_active_tab == 0) {
        Color tcol = (session->diagnostic.error_type != ERR_NONE)
                     ? (Color){ 251, 113, 133, 255 }
                     : (Color){ 56, 189, 248, 255 };
        DrawTextEx(font, session->diagnostic.pedagogical_title, (Vector2){ content.x, content.y - ui->terminal_scroll_y }, 15.0f, 1.0f, tcol);
        DrawTextEx(font, session->diagnostic.pedagogical_explanation, (Vector2){ content.x, content.y + 22.0f - ui->terminal_scroll_y }, 14.0f, 1.0f, (Color){ 226, 232, 240, 255 });

        const char *raw_log = session->diagnostic.raw_compiler_log[0]
                              ? session->diagnostic.raw_compiler_log
                              : (session->stderr_buf[0] ? session->stderr_buf : "Nenhum erro ou warning emitido pelo GCC / AddressSanitizer.");
        Rectangle raw_rect = { content.x, content.y + 50.0f, content.width, content.height - 50.0f };
        draw_multiline_log(font, raw_log, raw_rect, ui->terminal_scroll_y, 13.0f, (Color){ 148, 163, 184, 245 });
    } else {
        session->has_new_stdout = false;
        const char *out_txt = session->program_stdout[0]
                              ? session->program_stdout
                              : "(Nenhuma saída impressa em stdout ainda. Use printf(\"Valor = %d\\n\", x); no seu código C!)";
        draw_multiline_log(
            font,
            out_txt,
            content,
            ui->terminal_scroll_y,
            14.0f,
            session->program_stdout[0] ? (Color){ 167, 243, 208, 255 } : (Color){ 100, 116, 139, 240 }
        );
    }
    EndScissorMode();
}

/* ============================================================================
 * 6. MAIN WORKSPACE UPDATE & DRAW (PROGRESSIVE DISCLOSURE & MIDNIGHT SLATE)
 * ============================================================================ */

void ui_workspace_update_and_draw(DebuggerSession *session, WorkspaceUI *ui)
{
    if (!session || !ui) return;

    float dt = GetFrameTime();
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Font  font = GuiGetFont();
    float fs   = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);

    /* 1. Poll GDB non-blocking pipes and check 1.5s POSIX process-group watchdog */
    ErrorType prev_err = session->diagnostic.error_type;
    gdb_poll_and_parse(session);
    watchdog_check(session);

    /* Automatically pop open the Retractable Terminal when a compile/runtime error occurs */
    if (prev_err == ERR_NONE && session->diagnostic.error_type != ERR_NONE) {
        ui->terminal_open       = true;
        ui->terminal_active_tab = 0;
        ui->terminal_scroll_y   = 0.0f;
    }

    /* 2. Exponential Playback Speed (0.05x to 100.0x) & Auto-Play Stepper */
    if (session->speed_slider_t < 0.0f) session->speed_slider_t = 0.0f;
    if (session->speed_slider_t > 1.0f) session->speed_slider_t = 1.0f;
    session->play_speed = 0.05f * powf(2000.0f, session->speed_slider_t);
    float step_interval_sec = 0.50f / session->play_speed;

    if (session->auto_play && session->state == STATE_RUNNING && !session->waiting_for_step) {
        session->step_accumulator += dt;
        if (session->step_accumulator >= step_interval_sec) {
            session->step_accumulator = 0.0f;
            gdb_request_step(session);
        }
    }

    /* 3. Interactive Vertical Splitter (Editor Left vs. Full-Height Canvas Right) */
    Vector2 mouse   = GetMousePosition();
    float   main_h  = sh - FOOTER_BAR_H;
    float   left_w  = floorf(sw * ui->split_vertical_ratio);
    float   right_w = sw - left_w;

    Rectangle v_splitter = { left_w - SPLITTER_THICK * 0.5f, 0.0f, SPLITTER_THICK, main_h };
    bool hover_v = CheckCollisionPointRec(mouse, v_splitter) && !ui->dropdown_edit_mode;

    if (hover_v && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui->dragging_v_split = true;
        ui->is_panning       = false;
    }
    if (ui->dragging_v_split) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            ui->split_vertical_ratio = mouse.x / sw;
            if (ui->split_vertical_ratio < 0.24f) ui->split_vertical_ratio = 0.24f;
            if (ui->split_vertical_ratio > 0.68f) ui->split_vertical_ratio = 0.68f;
            left_w  = floorf(sw * ui->split_vertical_ratio);
            right_w = sw - left_w;
        } else {
            ui->dragging_v_split = false;
        }
    }
    SetMouseCursor((hover_v || ui->dragging_v_split) ? MOUSE_CURSOR_RESIZE_EW : MOUSE_CURSOR_DEFAULT);

    Rectangle left_panel   = { 0.0f,   0.0f,   left_w,  main_h };
    Rectangle canvas_panel = { left_w, 0.0f,   right_w, main_h };
    Rectangle footer_bar   = { 0.0f,   main_h, sw,      FOOTER_BAR_H };

    /* 4. Update Non-Overlapping Tree/Forest/List Layout & 60 FPS Lerp Animations */
    layout_compute_tree_positions(&session->snapshot, canvas_panel);
    layout_update_animations(&session->snapshot, dt, &ui->pulse_timer);

    /* --- PANEL 1: LEFT PANEL (Midnight Slate Editor without Redundant Header) --- */
    DrawRectangleRec(left_panel, (Color){ 22, 25, 37, 255 });

    /* Reset Template Button [ ↺ Resetar ] next to Template Dropdown */
    float reset_w = 88.0f;
    Rectangle reset_btn = { left_panel.x + left_panel.width - reset_w - 8.0f, 8.0f, reset_w, 28.0f };
    if (GuiButton(reset_btn, "↺ Resetar")) {
        gdb_stop_session(session);
        session->state = STATE_IDLE;
        load_code_preset(session, session->selected_preset);
        diagnostics_clear(&session->diagnostic);
        memset(&session->snapshot, 0, sizeof(session->snapshot));
        int len = (int)strlen(session->code_buffer);
        ui->sel_anchor_pos = len;
        ui->sel_active_pos = len;
        ui->scroll_y = 0.0f;
        ui->scroll_x = 0.0f;
        editor_push_undo(ui, session->code_buffer);
    }

    Rectangle editor_rect = {
        left_panel.x + 8.0f,
        left_panel.y + 42.0f,
        left_panel.width - 16.0f,
        left_panel.height - 48.0f
    };
    update_and_draw_code_editor(session, ui, editor_rect);

    /* --- PANEL 2: RIGHT PANEL (100% Height Interactive Memory Canvas + Floating HUDs) --- */
    layout_render_canvas(session, ui, canvas_panel);

    /* --- VERTICAL SPLITTER BAR --- */
    Color v_col = (hover_v || ui->dragging_v_split) ? (Color){ 56, 189, 248, 240 } : (Color){ 42, 48, 71, 255 };
    DrawRectangleRec(v_splitter, v_col);

    /* --- RETRACTABLE TERMINAL DRAWER (Slides up above Footer Bar) --- */
    draw_retractable_terminal(session, ui, sw, footer_bar.y, font);

    /* --- FOOTER BAR: PROGRESSIVE DISCLOSURE PLAYBACK + STATUS SUMMARY PILL --- */
    DrawRectangleRec(footer_bar, (Color){ 22, 25, 37, 255 });
    DrawLineEx((Vector2){ 0.0f, footer_bar.y }, (Vector2){ sw, footer_bar.y }, 1.0f, (Color){ 42, 48, 71, 255 });

    float ctrl_y = footer_bar.y + 7.0f;
    float cur_x  = 10.0f;
    float row_h  = 30.0f;
    bool  is_running_session = (session->gdb_pid > 0) &&
                               (session->state == STATE_PAUSED ||
                                session->state == STATE_RUNNING ||
                                session->state == STATE_STEPPING);

    if (!is_running_session) {
        /* STATE: EDITING — Show Only Primary Emerald Button [ ▶ Compilar & Iniciar ] */
        const char *lbl_run = "▶ Compilar & Iniciar";
        float w_run = MeasureTextEx(font, lbl_run, fs, 1.0f).x + 28.0f;
        if (GuiPrimaryButton((Rectangle){ cur_x, ctrl_y, w_run, row_h }, lbl_run)) {
            ui->blob_idle_timer   = 0.0f;
            ui->camera_auto_focus = true;
            if (compile_student_code(session)) {
                ui->terminal_open = false;
                gdb_start_session(session);
            } else {
                ui->terminal_open       = true;
                ui->terminal_active_tab = 0;
            }
        }
        cur_x += w_run + 12.0f;
    } else {
        /* STATE: RUNNING — Reveal [ ■ Parar ], [ ⏭ Passo (Step) ], [ ⏯ Auto-Play ] & Speed Slider */
        const char *lbl_stop = "■ Parar";
        float w_stop = MeasureTextEx(font, lbl_stop, fs, 1.0f).x + 22.0f;
        if (GuiButton((Rectangle){ cur_x, ctrl_y, w_stop, row_h }, lbl_stop)) {
            gdb_stop_session(session);
            session->state = STATE_IDLE;
            diagnostics_clear(&session->diagnostic);
        }
        cur_x += w_stop + 8.0f;

        const char *lbl_step = "⏭ Passo (Step)";
        float w_step = MeasureTextEx(font, lbl_step, fs, 1.0f).x + 24.0f;
        if (GuiPrimaryButton((Rectangle){ cur_x, ctrl_y, w_step, row_h }, lbl_step)) {
            ui->blob_idle_timer = 0.0f;
            session->auto_play  = false;
            gdb_request_step(session);
        }
        cur_x += w_step + 8.0f;

        const char *lbl_play = session->auto_play ? "⏸ Pausar Auto" : "⏯ Auto-Play";
        float w_play = MeasureTextEx(font, lbl_play, fs, 1.0f).x + 24.0f;
        if (GuiButton((Rectangle){ cur_x, ctrl_y, w_play, row_h }, lbl_play)) {
            ui->blob_idle_timer = 0.0f;
            session->auto_play  = !session->auto_play;
            if (session->auto_play && session->state == STATE_PAUSED) {
                session->state = STATE_RUNNING;
                session->step_accumulator = step_interval_sec;
            } else if (!session->auto_play && session->state == STATE_RUNNING) {
                session->state = STATE_PAUSED;
            }
        }
        cur_x += w_play + 12.0f;

        /* Speed Slider */
        char speed_txt[32];
        if (session->play_speed < 1.0f) {
            snprintf(speed_txt, sizeof(speed_txt), "%.2fx", session->play_speed);
        } else if (session->play_speed < 10.0f) {
            snprintf(speed_txt, sizeof(speed_txt), "%.1fx", session->play_speed);
        } else {
            snprintf(speed_txt, sizeof(speed_txt), "%.0fx", session->play_speed);
        }
        float slider_w = 105.0f;
        GuiSliderBar((Rectangle){ cur_x, ctrl_y + 6.0f, slider_w, 18.0f }, NULL, NULL, &session->speed_slider_t, 0.0f, 1.0f);
        cur_x += slider_w + 6.0f;
        float w_spd = MeasureTextEx(font, speed_txt, fs, 1.0f).x + 10.0f;
        GuiLabel((Rectangle){ cur_x, ctrl_y, w_spd, row_h }, speed_txt);
        cur_x += w_spd + 10.0f;
    }

    /* Clickable Status Bar Pill on the right side of the Footer (~28px high) */
    float status_x = fmaxf(cur_x + 6.0f, left_w + 8.0f);
    Rectangle status_pill = {
        status_x,
        footer_bar.y + 8.0f,
        fmaxf(180.0f, sw - status_x - 10.0f),
        28.0f
    };

    bool has_alert = (session->diagnostic.error_type != ERR_NONE);
    Color dot_col  = has_alert ? (Color){ 244, 63, 94, 255 } :
                     (session->state == STATE_FINISHED ? (Color){ 16, 185, 129, 255 } :
                     (is_running_session ? (Color){ 245, 158, 11, 255 } : (Color){ 56, 189, 248, 255 }));

    bool hov_status = CheckCollisionPointRec(mouse, status_pill);
    DrawRectangleRounded(status_pill, 0.30f, 6, hov_status ? (Color){ 40, 46, 71, 255 } : (Color){ 31, 36, 55, 255 });
    DrawRectangleRoundedLinesEx(status_pill, 0.30f, 6, 1.2f, has_alert ? (Color){ 244, 63, 94, 200 } : (Color){ 42, 48, 71, 255 });

    if (hov_status && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->terminal_open = !ui->terminal_open;
    }

    char status_msg[256];
    const char *arrow = ui->terminal_open ? "▼" : "▲";
    if (has_alert) {
        snprintf(status_msg, sizeof(status_msg), "● %s — Clique para detalhes %s",
                 session->diagnostic.pedagogical_title, arrow);
    } else if (session->state == STATE_FINISHED) {
        snprintf(status_msg, sizeof(status_msg), "● [CONCLUÍDO] %d nós na memória Heap%s — Clique para detalhes %s",
                 session->snapshot.node_count,
                 session->program_stdout_len > 0 ? " (stdout disponível)" : "",
                 arrow);
    } else if (is_running_session) {
        snprintf(status_msg, sizeof(status_msg), "● [DEPURANDO] Linha %d (%s) | %d nós na Heap — Terminal %s",
                 session->snapshot.current_executing_line,
                 session->snapshot.current_func[0] ? session->snapshot.current_func : "main",
                 session->snapshot.node_count,
                 arrow);
    } else {
        snprintf(status_msg, sizeof(status_msg), "● [MODO EDIÇÃO] Configure sua entrada em main() — Terminal & Logs %s", arrow);
    }

    BeginScissorMode((int)status_pill.x + 6, (int)status_pill.y, (int)status_pill.width - 12, (int)status_pill.height);
    DrawTextEx(
        font,
        status_msg,
        (Vector2){ status_pill.x + 10.0f, status_pill.y + 6.5f },
        13.5f,
        1.0f,
        has_alert ? (Color){ 254, 205, 211, 255 } : dot_col
    );
    EndScissorMode();

    /* --- RENDER 18-TEMPLATE DROPDOWN LAST SO OPEN MENU DRAWS ON TOP --- */
    Rectangle drop_rect = {
        left_panel.x + 8.0f,
        8.0f,
        left_panel.width - reset_w - 22.0f,
        28.0f
    };
    const char *preset_items =
        "0. Vazio (Scratchpad Livre);"
        "1. LeetCode 538: BST para Greater Tree (BUILD_TREE);"
        "2. LeetCode 108: Vetor Ordenado -> BST (main c/ Vetor);"
        "3. LeetCode 105: Pré & Em-Ordem -> Árvore (2 Vetores);"
        "4. LeetCode 617: Mesclar Duas Árvores (main Floresta);"
        "5. LeetCode 100: Same Tree — Comparar Floresta;"
        "6. LeetCode 226+572: Inverter & Subtree (Floresta);"
        "7. Buscas: Binária (BST), DFS (Profundidade) & BFS;"
        "8. BST: Inserção & Remoção (3 Casos);"
        "9. Travessias Clássicas (Pré, Em e Pós-Ordem);"
        "10. Árvore AVL (Balanceamento & Rotações);"
        "11. Árvore Rubro-Negra (Rotações & Flip de Cores);"
        "12. Árvore B / 2-3 (Cisão / Split & Mediana);"
        "13. Armadilha 1: Ponteiro Sem NULL (Lixo);"
        "14. Armadilha 2: Ciclo Estrutural (Rotação);"
        "15. Armadilha 3: Segfault (Desreferenciar NULL);"
        "16. Lista Encadeada: Inversão & Ponteiros (next/prev);"
        "17. Vetores: Ordenação (Selection) & Busca Binária";

    int prev_preset = session->selected_preset;
    if (GuiDropdownBox(drop_rect, preset_items, &session->selected_preset, ui->dropdown_edit_mode)) {
        ui->dropdown_edit_mode = !ui->dropdown_edit_mode;
        ui->blob_idle_timer    = 0.0f;
        if (session->selected_preset != prev_preset) {
            gdb_stop_session(session);
            session->state = STATE_IDLE;
            load_code_preset(session, session->selected_preset);
            diagnostics_clear(&session->diagnostic);
            memset(&session->snapshot, 0, sizeof(session->snapshot));
            int len = (int)strlen(session->code_buffer);
            ui->sel_anchor_pos = len;
            ui->sel_active_pos = len;
            ui->scroll_y = 0.0f;
            ui->scroll_x = 0.0f;
            editor_clear_undo_history(ui);
            editor_push_undo(ui, session->code_buffer);
        }
    }
}
