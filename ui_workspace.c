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

/* ============================================================================
 * 1. WORKSPACE INITIALIZATION & TRUE MONOSPACE FONT LOADING (SUBSYSTEM A.1)
 * ============================================================================ */

void ui_workspace_init(WorkspaceUI *ui, DebuggerSession *session)
{
    if (!ui) return;
    memset(ui, 0, sizeof(*ui));
    GuiLoadStyleDark();

    /* Default 3-Panel Docking Ratios (Subsystem B.1) */
    ui->split_vertical_ratio   = 0.40f;
    ui->split_horizontal_ratio = 0.72f;
    ui->active_blob_topic      = BLOB_HINT_NONE;
    ui->displayed_blob_topic   = BLOB_HINT_MASCOT;
    ui->blob_awake_anim        = 1.0f;
    ui->blob_idle_timer        = 0.0f;

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

    int codepoints[512];
    int cp_count = 0;
    for (int c = 0x0020; c <= 0x00FF && cp_count < 512; c++) {
        codepoints[cp_count++] = c;
    }
    int extra_cps[] = {
        0x2013, /* – */
        0x2014, /* — */
        0x2192, /* → */
        0x25B6, /* ▶ */
        0x23ED, /* ⏭ */
        0x23EF, /* ⏯ */
        0x23F9, /* ⏹ */
        0x2705, /* ✅ */
        0x26A0  /* ⚠ */
    };
    for (size_t i = 0; i < sizeof(extra_cps) / sizeof(extra_cps[0]) && cp_count < 512; i++) {
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

    /* Harmonize Raygui theme palette with logo.jpg (Pure Monochrome: Matte Black / Graphite / Crisp White) */
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR,    0x141416ff);
    GuiSetStyle(DEFAULT, LINE_COLOR,          0x38383eff);
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,   0x222226ff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x52525bff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,   0xf4f4f5ff);
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,  0x38383eff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED,0xfafafaff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,  0xffffffff);
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,  0xe4e4e7ff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED,0xffffffff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,  0x18181bff);
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED,  0x18181bff);
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED,0x2e2e33ff);
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,  0x6e6e77ff);

    /* Load logo.jpg, resize to 256x256, and set OS window icon (no in-app logo rendering) */
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

    Rectangle initial_canvas = { 1440.0f * 0.40f, 0.0f, 1440.0f * 0.60f, 900.0f * 0.72f };
    layout_init_camera(ui, initial_canvas);
    if (session) {
        int len = (int)strlen(session->code_buffer);
        ui->sel_anchor_pos = len;
        ui->sel_active_pos = len;
    }
}

void ui_workspace_Unload(WorkspaceUI *ui)
{
    if (!ui) return;
    if (ui->has_custom_font && ui->mono_font.texture.id != 0) {
        UnloadFont(ui->mono_font);
        ui->has_custom_font = false;
    }
}

/* ============================================================================
 * 2. UTF-8 NAVIGATION, EXACT PREFIX MEASUREMENT & SELECTION ENGINE (SUBSYSTEM A)
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

/* Never use col * fixed_char_width: always measure exact prefix substring via MeasureTextEx */
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

/* Hit-test mouse (x, y) to exact byte index in code buffer using MeasureTextEx */
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

/* Delete selected range [sel_start .. sel_end) in one atomic operation */
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

    /* Normalize \r\n to \n when inserting or pasting */
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

/* ============================================================================
 * 3. C SYNTAX HIGHLIGHTER & SCROLLABLE EDITOR RENDERER
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
        "TreeNode", "BUILD_TREE", "new_node", "bst_insert",
        "NULL", "malloc", "free", "calloc", "true", "false"
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
            DrawTextEx(font, tok, (Vector2){ exact_x, y }, font_size, 1.0f, (Color){ 134, 239, 172, 255 });
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
                col = (Color){ 250, 204, 21, 255 };  /* Yellow function calls */
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
                   !ui->dragging_v_split &&
                   !ui->dragging_h_split;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui->has_focus = hovered;
        if (hovered) {
            ui->test_input_edit_mode = false;
        } else {
            ui->is_selecting_with_mouse = false;
        }
    }

    /* Mouse wheel scrolling */
    if (hovered) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                ui->scroll_x = fmaxf(0.0f, ui->scroll_x - wheel * 38.0f);
            } else {
                ui->scroll_y = fmaxf(0.0f, ui->scroll_y - wheel * CODE_LINE_H * 2.5f);
            }
        }
    }

    float gutter_w     = 44.0f;
    float text_start_x = editor_bounds.x + gutter_w + 8.0f;
    float text_start_y = editor_bounds.y + 8.0f;

    /* Mouse Click & Drag Selection Engine (Subsystem A.3) */
    bool shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        int hit = hit_test_editor_index(font, code, mouse, text_start_x, text_start_y, ui->scroll_x, ui->scroll_y);
        if (!shift_down) {
            ui->sel_anchor_pos = hit;
        }
        ui->sel_active_pos          = hit;
        ui->is_selecting_with_mouse = true;
        ui->cursor_blink_timer      = 0.0f;
    }

    if (ui->is_selecting_with_mouse) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            int hit = hit_test_editor_index(font, code, mouse, text_start_x, text_start_y, ui->scroll_x, ui->scroll_y);
            ui->sel_active_pos = hit;
            ui->cursor_blink_timer = 0.0f;

            /* Auto-scroll when dragging near top/bottom edges of editor */
            if (mouse.y < editor_bounds.y + 18.0f) {
                ui->scroll_y = fmaxf(0.0f, ui->scroll_y - CODE_LINE_H * 0.35f);
            } else if (mouse.y > editor_bounds.y + editor_bounds.height - 18.0f) {
                ui->scroll_y += CODE_LINE_H * 0.35f;
            }
        } else {
            ui->is_selecting_with_mouse = false;
        }
    }

    /* Keyboard Editing, Selection & Clipboard Shortcuts (Subsystem A.3) */
    if (ui->has_focus) {
        ui->cursor_blink_timer += GetFrameTime();
        bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        if (ctrl && IsKeyPressed(KEY_A)) {
            ui->sel_anchor_pos = 0;
            ui->sel_active_pos = (int)strlen(code);
        } else if (ctrl && IsKeyPressed(KEY_C)) {
            editor_copy_selection_to_clipboard(code, ui);
        } else if (ctrl && IsKeyPressed(KEY_X)) {
            editor_copy_selection_to_clipboard(code, ui);
            editor_delete_selection(code, ui);
        } else if (ctrl && IsKeyPressed(KEY_V)) {
            const char *clip = GetClipboardText();
            if (clip && clip[0] != '\0') {
                editor_insert_text(code, MAX_CODE_SIZE, ui, clip);
                ui->cursor_blink_timer = 0.0f;
            }
        } else if (!ctrl) {
            int ch = GetCharPressed();
            while (ch > 0) {
                if (ch >= 32 && ch < 127) {
                    char ins[2] = { (char)ch, '\0' };
                    editor_insert_text(code, MAX_CODE_SIZE, ui, ins);
                    ui->cursor_blink_timer = 0.0f;
                }
                ch = GetCharPressed();
            }
        }

        if (IsKeyPressed(KEY_TAB)) {
            editor_insert_text(code, MAX_CODE_SIZE, ui, "    ");
            ui->cursor_blink_timer = 0.0f;
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
            ui->cursor_blink_timer = 0.0f;
        }

        if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
            if (!editor_delete_selection(code, ui)) {
                int cur = ui->sel_active_pos;
                if (cur > 0) {
                    int prev = utf8_prev_char_pos(code, cur);
                    int len  = (int)strlen(code);
                    memmove(code + prev, code + cur, (size_t)(len - cur + 1));
                    ui->sel_anchor_pos = prev;
                    ui->sel_active_pos = prev;
                }
            }
            ui->cursor_blink_timer = 0.0f;
        }

        if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
            if (!editor_delete_selection(code, ui)) {
                int cur = ui->sel_active_pos;
                int len = (int)strlen(code);
                if (cur < len) {
                    int next = utf8_next_char_pos(code, cur, len);
                    memmove(code + cur, code + next, (size_t)(len - next + 1));
                }
            }
            ui->cursor_blink_timer = 0.0f;
        }

        /* Cursor Navigation + Shift Selection Extension */
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
            if (!shift_down && editor_has_selection(ui)) {
                int s = 0, e = 0;
                editor_get_selection_range(ui, (int)strlen(code), &s, &e);
                ui->sel_anchor_pos = s;
                ui->sel_active_pos = s;
            } else {
                ui->sel_active_pos = utf8_prev_char_pos(code, ui->sel_active_pos);
                if (!shift_down) ui->sel_anchor_pos = ui->sel_active_pos;
            }
            ui->cursor_blink_timer = 0.0f;
        }

        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
            int len = (int)strlen(code);
            if (!shift_down && editor_has_selection(ui)) {
                int s = 0, e = 0;
                editor_get_selection_range(ui, len, &s, &e);
                ui->sel_anchor_pos = e;
                ui->sel_active_pos = e;
            } else {
                ui->sel_active_pos = utf8_next_char_pos(code, ui->sel_active_pos, len);
                if (!shift_down) ui->sel_anchor_pos = ui->sel_active_pos;
            }
            ui->cursor_blink_timer = 0.0f;
        }

        if (IsKeyPressed(KEY_HOME)) {
            int ls = ui->sel_active_pos;
            while (ls > 0 && code[ls - 1] != '\n') ls--;
            ui->sel_active_pos = ls;
            if (!shift_down) ui->sel_anchor_pos = ls;
            ui->cursor_blink_timer = 0.0f;
        }

        if (IsKeyPressed(KEY_END)) {
            int le = ui->sel_active_pos;
            while (code[le] && code[le] != '\n') le++;
            ui->sel_active_pos = le;
            if (!shift_down) ui->sel_anchor_pos = le;
            ui->cursor_blink_timer = 0.0f;
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
            ui->cursor_blink_timer = 0.0f;
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
            ui->cursor_blink_timer = 0.0f;
        }
    }

    /* Render Editor Background & Line Gutter (Monochrome Matte Graphite) */
    DrawRectangleRec(editor_bounds, (Color){ 20, 20, 22, 255 });
    Rectangle gutter_rect = { editor_bounds.x, editor_bounds.y, gutter_w, editor_bounds.height };
    DrawRectangleRec(gutter_rect, (Color){ 14, 14, 16, 255 });
    DrawLineEx(
        (Vector2){ editor_bounds.x + gutter_w, editor_bounds.y },
        (Vector2){ editor_bounds.x + gutter_w, editor_bounds.y + editor_bounds.height },
        1.0f,
        (Color){ 46, 46, 52, 255 }
    );

    BeginScissorMode((int)editor_bounds.x, (int)editor_bounds.y, (int)editor_bounds.width, (int)editor_bounds.height);

    int active_line = (session->diagnostic.error_type == ERR_COMPILE)
                      ? session->diagnostic.fault_line
                      : session->snapshot.current_executing_line;
    bool is_error_line = (session->diagnostic.error_type != ERR_NONE);

    int total_len = (int)strlen(code);
    int sel_min = 0, sel_max = 0;
    editor_get_selection_range(ui, total_len, &sel_min, &sel_max);

    int line_num = 1;
    int idx = 0;

    while (idx <= total_len) {
        int line_end = idx;
        while (line_end < total_len && code[line_end] != '\n') line_end++;

        float y = text_start_y + (float)(line_num - 1) * CODE_LINE_H - ui->scroll_y;

        if (y + CODE_LINE_H >= editor_bounds.y && y <= editor_bounds.y + editor_bounds.height) {
            /* 1. Highlight active debugger line or fault line */
            if (session->state != STATE_IDLE && line_num == active_line) {
                Color hl  = is_error_line ? (Color){ 225, 29, 72, 85 }  : (Color){ 250, 204, 21, 60 };
                Color bar = is_error_line ? (Color){ 244, 63, 94, 255 } : (Color){ 250, 204, 21, 255 };
                DrawRectangle(
                    (int)(editor_bounds.x + gutter_w + 1.0f),
                    (int)y,
                    (int)(editor_bounds.width - gutter_w - 2.0f),
                    (int)CODE_LINE_H,
                    hl
                );
                DrawRectangle((int)(editor_bounds.x + gutter_w + 1.0f), (int)y, 4, (int)CODE_LINE_H, bar);
            }

            /* 2. Line number in gutter */
            char lnum_str[16];
            snprintf(lnum_str, sizeof(lnum_str), "%2d", line_num);
            Color lnum_col = (session->state != STATE_IDLE && line_num == active_line)
                             ? (Color){ 250, 204, 21, 255 }
                             : (Color){ 113, 113, 122, 220 };
            DrawTextEx(font, lnum_str, (Vector2){ editor_bounds.x + 10.0f, y + 2.0f }, 15.0f, 1.0f, lnum_col);

            /* 3. Clip code text area to the right of gutter */
            char line_buf[1024];
            int llen = line_end - idx;
            if (llen >= (int)sizeof(line_buf)) llen = (int)sizeof(line_buf) - 1;
            memcpy(line_buf, code + idx, (size_t)llen);
            line_buf[llen] = '\0';

            float draw_x = text_start_x - ui->scroll_x;

            /* 4. Visual Selection Highlight Rectangle behind text (Monochrome Silver/White) */
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
                        (Color){ 212, 212, 216, 65 }
                    );
                }
            }

            /* 5. Syntax-colored characters using exact prefix widths */
            draw_syntax_colored_line(font, line_buf, (Vector2){ draw_x, y + 2.0f }, CODE_FONT_SIZE);

            /* 6. Blinking caret at sel_active_pos via exact MeasureTextEx prefix */
            if (ui->has_focus && ui->sel_active_pos >= idx && ui->sel_active_pos <= line_end) {
                if (((int)(ui->cursor_blink_timer * 2.2f) % 2) == 0) {
                    int col_bytes = ui->sel_active_pos - idx;
                    if (col_bytes > llen) col_bytes = llen;
                    float cx = draw_x + measure_line_prefix_width(font, line_buf, col_bytes, CODE_FONT_SIZE);
                    DrawRectangle((int)cx, (int)(y + 2.0f), 2, (int)(CODE_LINE_H - 3.0f), (Color){ 244, 244, 245, 255 });
                }
            }
        }

        if (line_end == total_len) break;
        idx = line_end + 1;
        line_num++;
    }

    EndScissorMode();
    DrawRectangleLinesEx(editor_bounds, 1.0f, ui->has_focus ? (Color){ 212, 212, 216, 200 } : (Color){ 56, 56, 62, 255 });
}

/* ============================================================================
 * 4. DIAGNOSTIC CARD, DOCKING SPLITTERS & RESPONSIVE TRANSPORT BAR (SUBSYSTEM B & C)
 * ============================================================================ */

static void draw_wrapped_text(Font font, const char *text, Rectangle bounds, float font_size, Color color)
{
    if (!text || text[0] == '\0') return;
    char words[1024];
    snprintf(words, sizeof(words), "%s", text);

    float x = bounds.x;
    float y = bounds.y;
    float space_w = MeasureTextEx(font, " ", font_size, 1.0f).x;

    char *saveptr = NULL;
    char *tok = strtok_r(words, " ", &saveptr);
    while (tok != NULL) {
        Vector2 wsz = MeasureTextEx(font, tok, font_size, 1.0f);
        if (x + wsz.x > bounds.x + bounds.width && x > bounds.x) {
            x = bounds.x;
            y += font_size + 5.0f;
            if (y + font_size > bounds.y + bounds.height) break;
        }
        DrawTextEx(font, tok, (Vector2){ x, y }, font_size, 1.0f, color);
        x += wsz.x + space_w;
        tok = strtok_r(NULL, " ", &saveptr);
    }
}

void ui_workspace_update_and_draw(DebuggerSession *session, WorkspaceUI *ui)
{
    if (!session || !ui) return;

    float dt = GetFrameTime();
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Font  font = GuiGetFont();
    float fs   = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);

    /* 1. Poll GDB non-blocking pipes and check 1.5s POSIX process-group watchdog */
    gdb_poll_and_parse(session);
    watchdog_check(session);

    /* 2. Exponential Playback Speed (0.05x to 100.0x) & Auto-Play Stepper (Subsystem C) */
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

    /* 3. Interactive Resizable 3-Panel Docking Splitters (Subsystem B.1) */
    Vector2 mouse = GetMousePosition();
    float left_w  = floorf(sw * ui->split_vertical_ratio);
    float top_h   = floorf(sh * ui->split_horizontal_ratio);

    Rectangle v_splitter = { left_w - SPLITTER_THICK * 0.5f, 0.0f, SPLITTER_THICK, top_h };
    Rectangle h_splitter = { 0.0f, top_h - SPLITTER_THICK * 0.5f, sw, SPLITTER_THICK };

    bool hover_v = CheckCollisionPointRec(mouse, v_splitter) && !ui->dropdown_edit_mode && !ui->dragging_h_split;
    bool hover_h = CheckCollisionPointRec(mouse, h_splitter) && !ui->dropdown_edit_mode && !ui->dragging_v_split;

    if (hover_v && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui->dragging_v_split = true;
        ui->is_panning = false;
    }
    if (hover_h && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui->dragging_h_split = true;
        ui->is_panning = false;
    }

    if (ui->dragging_v_split) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            ui->split_vertical_ratio = mouse.x / sw;
            if (ui->split_vertical_ratio < 0.22f) ui->split_vertical_ratio = 0.22f;
            if (ui->split_vertical_ratio > 0.68f) ui->split_vertical_ratio = 0.68f;
            left_w = floorf(sw * ui->split_vertical_ratio);
        } else {
            ui->dragging_v_split = false;
        }
    }

    if (ui->dragging_h_split) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            ui->split_horizontal_ratio = mouse.y / sh;
            if (ui->split_horizontal_ratio < 0.45f) ui->split_horizontal_ratio = 0.45f;
            if (ui->split_horizontal_ratio > 0.88f) ui->split_horizontal_ratio = 0.88f;
            top_h = floorf(sh * ui->split_horizontal_ratio);
        } else {
            ui->dragging_h_split = false;
        }
    }

    /* Update mouse cursor icon for splitters */
    if (hover_v || ui->dragging_v_split) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_EW);
    } else if (hover_h || ui->dragging_h_split) {
        SetMouseCursor(MOUSE_CURSOR_RESIZE_NS);
    } else {
        SetMouseCursor(MOUSE_CURSOR_DEFAULT);
    }

    float right_w  = sw - left_w;
    float bottom_h = sh - top_h;

    Rectangle left_panel   = { 0.0f,   0.0f,  left_w,  top_h };
    Rectangle canvas_panel = { left_w, 0.0f,  right_w, top_h };
    Rectangle bottom_panel = { 0.0f,   top_h, sw,      bottom_h };

    /* 4. Update Non-Overlapping Tree/Forest Layout & 60 FPS Lerp Animations */
    layout_compute_tree_positions(&session->snapshot, canvas_panel);
    layout_update_animations(&session->snapshot, dt, &ui->pulse_timer);

    /* --- PANEL 1: LEFT PANEL (Code Editor & 12-Template Selector) --- */
    GuiPanel(left_panel, "AVORA Editor C11 (struct TreeNode) — student_code.c");

    Rectangle editor_rect = {
        left_panel.x + 8.0f,
        left_panel.y + 66.0f,
        left_panel.width - 16.0f,
        left_panel.height - 74.0f
    };
    update_and_draw_code_editor(session, ui, editor_rect);

    /* --- PANEL 2: TOP-RIGHT PANEL (Interactive 2D Heap Memory & Forest Canvas) --- */
    layout_render_canvas(session, ui, canvas_panel);

    /* --- PANEL 3: BOTTOM PANEL (Responsive Transport Bar & Pedagogical Console) --- */
    GuiPanel(bottom_panel, "AVORA — Analisador Visual de Operações e Rastreamento de Árvores (Algoritmos & Estruturas de Dados)");

    BeginScissorMode((int)bottom_panel.x + 1, (int)bottom_panel.y + 26, (int)bottom_panel.width - 2, (int)bottom_panel.height - 27);

    float ctrl_y = bottom_panel.y + 32.0f;
    float cur_x  = bottom_panel.x + 12.0f;
    float row_h  = 30.0f;

    /* 1. [ Compilar & Iniciar ] (Dynamic Width) */
    const char *lbl_run = "Compilar & Iniciar";
    float w_run = MeasureTextEx(font, lbl_run, fs, 1.0f).x + 28.0f;
    if (GuiButton((Rectangle){ cur_x, ctrl_y, w_run, row_h }, lbl_run)) {
        ui->blob_idle_timer = 0.0f;
        if (compile_student_code(session)) {
            gdb_start_session(session);
        }
    }
    cur_x += w_run + 10.0f;

    /* 2. [ Passo (Step Into) ] */
    const char *lbl_step = "Passo (Step)";
    float w_step = MeasureTextEx(font, lbl_step, fs, 1.0f).x + 28.0f;
    bool can_step = (session->gdb_pid > 0) &&
                    (session->state == STATE_PAUSED || session->state == STATE_RUNNING);
    if (!can_step) GuiDisable();
    if (GuiButton((Rectangle){ cur_x, ctrl_y, w_step, row_h }, lbl_step)) {
        ui->blob_idle_timer = 0.0f;
        session->auto_play = false;
        gdb_request_step(session);
    }
    GuiEnable();
    cur_x += w_step + 10.0f;

    /* 3. [ Auto-Play / Pausar ] */
    const char *lbl_play = session->auto_play ? "Pausar Auto" : "Auto-Play";
    float w_play = MeasureTextEx(font, lbl_play, fs, 1.0f).x + 28.0f;
    if (session->gdb_pid <= 0) GuiDisable();
    if (GuiButton((Rectangle){ cur_x, ctrl_y, w_play, row_h }, lbl_play)) {
        ui->blob_idle_timer = 0.0f;
        session->auto_play = !session->auto_play;
        if (session->auto_play && session->state == STATE_PAUSED) {
            session->state = STATE_RUNNING;
            session->step_accumulator = step_interval_sec;
        } else if (!session->auto_play && session->state == STATE_RUNNING) {
            session->state = STATE_PAUSED;
        }
    }
    GuiEnable();
    cur_x += w_play + 10.0f;

    /* 4. [ Parar ] */
    const char *lbl_stop = "Parar";
    float w_stop = MeasureTextEx(font, lbl_stop, fs, 1.0f).x + 28.0f;
    if (session->gdb_pid <= 0) GuiDisable();
    if (GuiButton((Rectangle){ cur_x, ctrl_y, w_stop, row_h }, lbl_stop)) {
        gdb_stop_session(session);
        session->state = STATE_IDLE;
        diagnostics_clear(&session->diagnostic);
    }
    GuiEnable();
    cur_x += w_stop + 14.0f;

    /* 5. Exponential Speed Slider (0.05x to 100.0x) with Dynamic Readout (Subsystem C) */
    char speed_txt[32];
    if (session->play_speed < 1.0f) {
        snprintf(speed_txt, sizeof(speed_txt), "%.2fx", session->play_speed);
    } else if (session->play_speed < 10.0f) {
        snprintf(speed_txt, sizeof(speed_txt), "%.1fx", session->play_speed);
    } else {
        snprintf(speed_txt, sizeof(speed_txt), "%.0fx", session->play_speed);
    }

    const char *lbl_vel = "Velocidade:";
    float w_vel_lbl  = MeasureTextEx(font, lbl_vel, fs, 1.0f).x + 8.0f;
    float w_vel_read = MeasureTextEx(font, speed_txt, fs, 1.0f).x + 14.0f;
    float slider_w   = 125.0f;

    /* Wrap to a second toolbar row if window is narrow */
    if (cur_x + w_vel_lbl + slider_w + w_vel_read > sw - 16.0f) {
        cur_x   = bottom_panel.x + 12.0f;
        ctrl_y += row_h + 6.0f;
    }

    GuiLabel((Rectangle){ cur_x, ctrl_y, w_vel_lbl, row_h }, lbl_vel);
    cur_x += w_vel_lbl + 2.0f;
    GuiSliderBar((Rectangle){ cur_x, ctrl_y + 5.0f, slider_w, 20.0f }, NULL, NULL, &session->speed_slider_t, 0.0f, 1.0f);
    cur_x += slider_w + 6.0f;
    GuiLabel((Rectangle){ cur_x, ctrl_y, w_vel_read, row_h }, speed_txt);
    cur_x += w_vel_read + 12.0f;

    /* 6. Quick Input Bar (BUILD_TREE / Test Input) */
    const char *lbl_in = "Entrada (BUILD_TREE):";
    float w_in_lbl = MeasureTextEx(font, lbl_in, fs, 1.0f).x + 8.0f;
    if (cur_x + w_in_lbl + 180.0f > sw - 14.0f) {
        cur_x   = bottom_panel.x + 12.0f;
        ctrl_y += row_h + 6.0f;
    }
    GuiLabel((Rectangle){ cur_x, ctrl_y, w_in_lbl, row_h }, lbl_in);
    cur_x += w_in_lbl + 4.0f;

    float input_w = fmaxf(140.0f, sw - cur_x - 14.0f);
    if (GuiTextBox((Rectangle){ cur_x, ctrl_y + 1.0f, input_w, row_h - 2.0f },
                   session->test_input, MAX_TEST_INPUT, ui->test_input_edit_mode)) {
        ui->test_input_edit_mode = !ui->test_input_edit_mode;
        if (ui->test_input_edit_mode) {
            ui->has_focus = false;
        }
    }

    /* 7. Pedagogical Diagnostic Card (Strictly Scissored & Word-Wrapped) */
    float card_top = ctrl_y + row_h + 8.0f;
    Rectangle card_rect = {
        bottom_panel.x + 12.0f,
        card_top,
        bottom_panel.width - 24.0f,
        fmaxf(52.0f, (bottom_panel.y + bottom_panel.height) - card_top - 8.0f)
    };

    bool has_alert          = (session->diagnostic.error_type != ERR_NONE);
    bool is_garbage_alert   = (session->diagnostic.error_type == ERR_SIGSEGV_GARBAGE);
    bool has_uninit_note    = (!has_alert && session->diagnostic.has_uninit_ptr_warning && session->state != STATE_IDLE);
    bool has_auto_main_warn = (!has_alert && !has_uninit_note && session->diagnostic.auto_main_warning && session->state != STATE_IDLE);

    Color card_bg = has_alert
                    ? (is_garbage_alert ? (Color){ 76, 29, 12, 245 } : (Color){ 69, 10, 20, 245 })
                    : (has_uninit_note
                       ? (Color){ 58, 44, 14, 238 }
                       : (has_auto_main_warn
                          ? (Color){ 67, 36, 12, 240 }
                          : (session->state == STATE_FINISHED ? (Color){ 20, 42, 30, 240 } : (Color){ 24, 24, 27, 245 })));
    Color card_border = has_alert
                        ? (is_garbage_alert ? (Color){ 249, 115, 22, 255 } : (Color){ 244, 63, 94, 255 })
                        : (has_uninit_note
                           ? (Color){ 250, 204, 21, 235 }
                           : (has_auto_main_warn
                              ? (Color){ 251, 146, 60, 255 }
                              : (session->state == STATE_FINISHED ? (Color){ 74, 222, 128, 230 } : (Color){ 161, 161, 170, 220 })));
    Color title_col = has_alert
                      ? (is_garbage_alert ? (Color){ 253, 186, 116, 255 } : (Color){ 254, 205, 211, 255 })
                      : (has_uninit_note
                         ? (Color){ 254, 240, 138, 255 }
                         : (has_auto_main_warn
                            ? (Color){ 253, 186, 116, 255 }
                            : (session->state == STATE_FINISHED ? (Color){ 134, 239, 172, 255 } : (Color){ 244, 244, 245, 255 })));

    DrawRectangleRounded(card_rect, 0.08f, 6, card_bg);
    DrawRectangleRoundedLinesEx(card_rect, 0.08f, 6, (has_alert || has_uninit_note || has_auto_main_warn) ? 2.0f : 1.3f, card_border);

    BeginScissorMode((int)card_rect.x + 2, (int)card_rect.y + 2, (int)card_rect.width - 4, (int)card_rect.height - 4);
    DrawTextEx(
        font,
        session->diagnostic.pedagogical_title,
        (Vector2){ card_rect.x + 14.0f, card_rect.y + 8.0f },
        16.0f,
        1.0f,
        title_col
    );

    Rectangle expl_bounds = {
        card_rect.x + 14.0f,
        card_rect.y + 32.0f,
        card_rect.width - 28.0f,
        card_rect.height - 36.0f
    };
    draw_wrapped_text(
        font,
        session->diagnostic.pedagogical_explanation,
        expl_bounds,
        15.0f,
        (Color){ 228, 228, 231, 255 }
    );
    EndScissorMode();

    EndScissorMode();

    /* --- DRAW INTERACTIVE SPLITTER BARS (Monochrome Graphite / White Hover) --- */
    Color v_col = (hover_v || ui->dragging_v_split) ? (Color){ 244, 244, 245, 245 } : (Color){ 48, 48, 54, 200 };
    Color h_col = (hover_h || ui->dragging_h_split) ? (Color){ 244, 244, 245, 245 } : (Color){ 48, 48, 54, 200 };
    DrawRectangleRec(v_splitter, v_col);
    DrawRectangleRec(h_splitter, h_col);

    /* --- RENDER 16-TEMPLATE DROPDOWN LAST SO OPEN MENU DRAWS ON TOP --- */
    Rectangle drop_rect = {
        left_panel.x + 8.0f,
        left_panel.y + 32.0f,
        left_panel.width - 16.0f,
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
        "15. Armadilha 3: Segfault (Desreferenciar NULL)";

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
        }
    }
}
