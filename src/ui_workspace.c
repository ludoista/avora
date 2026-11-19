#define RAYGUI_IMPLEMENTATION
#include "raygui.h"
#include "visualizer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <unistd.h>

#define MENU_BAR_H     26.0f
#define SPLITTER_THICK 6.0f
#define FOOTER_BAR_H   44.0f

/* ============================================================================
 * 1. UNDO / REDO HISTORY ENGINE & WINDOW TITLE SYNCHRONIZATION
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

    if (ui->undo_index >= 0 && ui->undo_index < ui->undo_count &&
        ui->undo_stack[ui->undo_index].code_snapshot &&
        strcmp(ui->undo_stack[ui->undo_index].code_snapshot, code) == 0) {
        return;
    }

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
        ui->is_dirty       = (ui->undo_index > 0);
        ui_workspace_update_window_title(ui);
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
        ui->is_dirty       = true;
        ui_workspace_update_window_title(ui);
    }
}

static const char *path_basename_ptr(const char *path)
{
    if (!path || path[0] == '\0') return "";
    const char *slash = strrchr(path, '/');
    return slash ? (slash + 1) : path;
}

void ui_workspace_update_window_title(WorkspaceUI *ui)
{
    if (!ui) return;
    char title[256];
    if (ui->current_file_path[0] != '\0') {
        snprintf(title, sizeof(title), "AVORA — %.180s%s",
                 path_basename_ptr(ui->current_file_path),
                 ui->is_dirty ? " ●" : "");
    } else {
        snprintf(title, sizeof(title), "AVORA%s", ui->is_dirty ? " ●" : "");
    }
    SetWindowTitle(title);
}

static void show_toast(WorkspaceUI *ui, const char *msg)
{
    if (!ui || !msg) return;
    snprintf(ui->toast_message, sizeof(ui->toast_message), "%s", msg);
    ui->toast_timer = 3.2f;
}

/* ============================================================================
 * 2. FONT ATLAS LOADER & DYNAMIC 1:1 EDITOR FONT RELOADING
 * ============================================================================ */

static int build_avora_codepoints(int *out_cps, int max_cps)
{
    int cp_count = 0;
    for (int c = 0x0020; c <= 0x00FF && cp_count < max_cps; c++) {
        out_cps[cp_count++] = c;
    }
    static const int extra_cps[] = {
        0x2013, /* – */
        0x2014, /* — */
        0x2022, /* • */
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
        0x23F8, /* ⏸ */
        0x23F9, /* ⏹ */
        0x2328, /* ⌨ */
        0x266A, /* ♪ */
        0x2699, /* ⚙ */
        0x2705, /* ✅ */
        0x2713, /* ✓ */
        0x2715, /* ✕ */
        0x26A0  /* ⚠ */
    };
    for (size_t i = 0; i < sizeof(extra_cps) / sizeof(extra_cps[0]) && cp_count < max_cps; i++) {
        out_cps[cp_count++] = extra_cps[i];
    }
    return cp_count;
}

void ui_workspace_reload_editor_font(WorkspaceUI *ui)
{
    if (!ui || !ui->has_custom_font || ui->font_path[0] == '\0') return;

    int target_px = ui->config.editor_font_size;
    if (ui->config.presentation_mode) {
        target_px += 4;
    }
    if (target_px < 12) target_px = 12;
    if (target_px > 28) target_px = 28;

    if (ui->loaded_editor_font_px == target_px && ui->mono_font.texture.id != 0) {
        return;
    }

    int codepoints[560];
    int cp_count = build_avora_codepoints(codepoints, 560);

    Font new_mono = LoadFontEx(ui->font_path, target_px, codepoints, cp_count);
    if (new_mono.texture.id != 0) {
        SetTextureFilter(new_mono.texture, TEXTURE_FILTER_BILINEAR);

        unsigned int def_id = GetFontDefault().texture.id;
        unsigned int old_id = ui->mono_font.texture.id;
        if (old_id != 0 && old_id != def_id &&
            old_id != ui->font_ui.texture.id &&
            old_id != ui->font_sm.texture.id &&
            old_id != ui->font_canvas.texture.id) {
            UnloadFont(ui->mono_font);
        }
        ui->mono_font             = new_mono;
        ui->loaded_editor_font_px = target_px;
    }
}

/* ============================================================================
 * 3. FILE I/O, DYNAMIC TEMPLATES LOADING & UNSAVED CHANGES GUARD
 * ============================================================================ */

static bool load_file_into_editor(DebuggerSession *session, WorkspaceUI *ui, const char *file_path, bool track_as_open_file)
{
    if (!session || !ui || !file_path || file_path[0] == '\0') return false;
    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        show_toast(ui, "Erro ao abrir arquivo .c");
        return false;
    }

    gdb_stop_session(session);
    session->state = STATE_IDLE;
    diagnostics_clear(&session->diagnostic);
    memset(&session->snapshot, 0, sizeof(session->snapshot));

    size_t n = fread(session->code_buffer, 1, MAX_CODE_SIZE - 1, fp);
    session->code_buffer[n] = '\0';
    fclose(fp);
    sanitize_utf8_text(session->code_buffer);

    if (track_as_open_file) {
        snprintf(ui->current_file_path, sizeof(ui->current_file_path), "%s", file_path);
        snprintf(ui->config.last_open_file, sizeof(ui->config.last_open_file), "%s", file_path);
        config_add_recent_file(&ui->config, file_path);
    } else {
        ui->current_file_path[0] = '\0';
        ui->config.last_open_file[0] = '\0';
    }

    int len = (int)strlen(session->code_buffer);
    ui->sel_anchor_pos  = len;
    ui->sel_active_pos  = len;
    ui->scroll_y        = 0.0f;
    ui->scroll_x        = 0.0f;
    ui->target_scroll_y = 0.0f;
    ui->target_scroll_x = 0.0f;
    ui->is_dirty        = false;

    editor_clear_undo_history(ui);
    editor_push_undo(ui, session->code_buffer);
    ui_workspace_update_window_title(ui);
    return true;
}

static void apply_template_by_index(DebuggerSession *session, WorkspaceUI *ui, int tpl_idx)
{
    if (!session || !ui) return;
    if (ui->template_count <= 0) {
        templates_refresh_list(ui);
    }
    if (tpl_idx < 0 || tpl_idx >= ui->template_count) tpl_idx = 0;

    ui->active_template_idx           = tpl_idx;
    session->selected_preset          = tpl_idx;
    ui->config.last_selected_template = tpl_idx;

    const TemplateEntry *te = &ui->templates[tpl_idx];
    if (te->is_builtin) {
        gdb_stop_session(session);
        session->state = STATE_IDLE;
        load_code_preset(session, te->builtin_index);
        session->selected_preset = tpl_idx;
        diagnostics_clear(&session->diagnostic);
        memset(&session->snapshot, 0, sizeof(session->snapshot));

        ui->current_file_path[0]     = '\0';
        ui->config.last_open_file[0] = '\0';
        int len = (int)strlen(session->code_buffer);
        ui->sel_anchor_pos  = len;
        ui->sel_active_pos  = len;
        ui->scroll_y        = 0.0f;
        ui->scroll_x        = 0.0f;
        ui->target_scroll_y = 0.0f;
        ui->target_scroll_x = 0.0f;
        ui->is_dirty        = false;
        editor_clear_undo_history(ui);
        editor_push_undo(ui, session->code_buffer);
        ui_workspace_update_window_title(ui);
    } else {
        load_file_into_editor(session, ui, te->file_path, false);
    }
    config_save(&ui->config);
}

static bool save_editor_to_path(DebuggerSession *session, WorkspaceUI *ui, const char *path)
{
    if (!session || !ui || !path || path[0] == '\0') return false;
    FILE *fp = fopen(path, "w");
    if (!fp) {
        show_toast(ui, "Falha ao salvar arquivo no disco!");
        return false;
    }
    fputs(session->code_buffer, fp);
    fclose(fp);

    snprintf(ui->current_file_path, sizeof(ui->current_file_path), "%s", path);
    snprintf(ui->config.last_open_file, sizeof(ui->config.last_open_file), "%s", path);
    ui->is_dirty = false;
    config_add_recent_file(&ui->config, path);
    templates_refresh_list(ui);
    ui_workspace_update_window_title(ui);

    char msg[160];
    snprintf(msg, sizeof(msg), "Salvo com sucesso: %s", path_basename_ptr(path));
    show_toast(ui, msg);
    return true;
}

static void open_path_dialog_or_fallback(DebuggerSession *session, WorkspaceUI *ui, int mode, const char *suggested_path)
{
    char chosen[512] = { 0 };
    if (platform_native_file_dialog(mode, suggested_path, chosen, sizeof(chosen))) {
        if (mode == 0) {
            if (load_file_into_editor(session, ui, chosen, true)) {
                char msg[160];
                snprintf(msg, sizeof(msg), "Arquivo carregado: %s", path_basename_ptr(chosen));
                show_toast(ui, msg);
            }
        } else if (mode == 1) {
            save_editor_to_path(session, ui, chosen);
        } else if (mode == 2) {
            snprintf(ui->config.templates_dir, sizeof(ui->config.templates_dir), "%s", chosen);
            templates_refresh_list(ui);
            config_save(&ui->config);
            show_toast(ui, "Pasta de templates atualizada!");
        } else if (mode == 3) {
            snprintf(ui->pending_png_path, sizeof(ui->pending_png_path), "%s", chosen);
            ui->pending_png_capture = true;
        }
        return;
    }

    /* Fallback to clean internal RayGUI path modal if zenity/kdialog is unavailable or cancelled */
    if (access("/usr/bin/zenity", X_OK) != 0 && access("/usr/bin/kdialog", X_OK) != 0) {
        ui->modal_path_input_open = true;
        ui->path_input_mode       = mode;
        ui->path_textbox_edit     = true;
        snprintf(ui->path_input_buf, sizeof(ui->path_input_buf), "%s",
                 (suggested_path && suggested_path[0]) ? suggested_path : "");
    }
}

static void execute_pending_action(DebuggerSession *session, WorkspaceUI *ui, int action_type, int param)
{
    switch (action_type) {
        case 1: /* Novo Scratchpad */
            apply_template_by_index(session, ui, 0);
            show_toast(ui, "Novo Scratchpad iniciado");
            break;
        case 2: /* Abrir Arquivo .c */
            open_path_dialog_or_fallback(session, ui, 0, ui->current_file_path[0] ? ui->current_file_path : ".");
            break;
        case 3: /* Trocar Template */
            apply_template_by_index(session, ui, param);
            break;
        case 4: /* Resetar Template Atual */
            apply_template_by_index(session, ui, ui->active_template_idx);
            show_toast(ui, "Template resetado para o código original");
            break;
        case 5: /* Sair do Aplicativo */
            ui->request_exit = true;
            break;
        case 6: /* Abrir Arquivo Recente (param = índice em recent_files) */
            if (param >= 0 && param < ui->config.recent_count) {
                char path_copy[512];
                snprintf(path_copy, sizeof(path_copy), "%s", ui->config.recent_files[param]);
                if (load_file_into_editor(session, ui, path_copy, true)) {
                    char msg[160];
                    snprintf(msg, sizeof(msg), "Arquivo recente aberto: %.110s", path_basename_ptr(path_copy));
                    show_toast(ui, msg);
                }
            }
            break;
        default:
            break;
    }
}

static void request_guarded_action(DebuggerSession *session, WorkspaceUI *ui, int action_type, int param)
{
    if (ui->is_dirty && ui->config.confirm_unsaved_changes) {
        ui->modal_unsaved_confirm_open = true;
        ui->pending_action_type        = action_type;
        ui->pending_action_param       = param;
    } else {
        execute_pending_action(session, ui, action_type, param);
    }
}

/* ============================================================================
 * 4. WORKSPACE INITIALIZATION
 * ============================================================================ */

void ui_workspace_init(WorkspaceUI *ui, DebuggerSession *session)
{
    if (!ui) return;

    /* Preserve already-loaded AppConfig from main() */
    AppConfig loaded_cfg = ui->config;
    memset(ui, 0, sizeof(*ui));
    ui->config = loaded_cfg;

    GuiLoadStyleDark();

    ui->split_vertical_ratio   = (ui->config.remember_window_geometry &&
                                  ui->config.split_ratio >= 0.24f &&
                                  ui->config.split_ratio <= 0.68f)
                                 ? ui->config.split_ratio
                                 : 0.42f;
    ui->split_horizontal_ratio = 0.72f;
    ui->active_blob_topic      = BLOB_HINT_NONE;
    ui->displayed_blob_topic   = BLOB_HINT_MASCOT;
    ui->blob_awake_anim        = 1.0f;
    ui->blob_idle_timer        = 0.0f;
    ui->camera_auto_focus      = true;
    ui->undo_index             = -1;
    ui->active_menu            = -1;

    /* Load system monospace TTF font covering ASCII + Latin-1 + UI Symbols */
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

    int codepoints[560];
    int cp_count = build_avora_codepoints(codepoints, 560);

    Font def_font = GetFontDefault();
    ui->mono_font   = def_font;
    ui->font_ui     = def_font;
    ui->font_sm     = def_font;
    ui->font_canvas = def_font;

    int init_ed_px = ui->config.editor_font_size + (ui->config.presentation_mode ? 4 : 0);
    if (init_ed_px < 12) init_ed_px = 12;
    if (init_ed_px > 28) init_ed_px = 28;

    for (int i = 0; font_candidates[i] != NULL; i++) {
        if (access(font_candidates[i], R_OK) == 0) {
            Font f_ed = LoadFontEx(font_candidates[i], init_ed_px, codepoints, cp_count);
            if (f_ed.texture.id != 0) {
                snprintf(ui->font_path, sizeof(ui->font_path), "%s", font_candidates[i]);
                ui->loaded_editor_font_px = init_ed_px;

                Font f15 = LoadFontEx(font_candidates[i], 15, codepoints, cp_count);
                Font f13 = LoadFontEx(font_candidates[i], 13, codepoints, cp_count);
                Font f32 = LoadFontEx(font_candidates[i], 32, codepoints, cp_count);

                SetTextureFilter(f_ed.texture, TEXTURE_FILTER_BILINEAR);
                ui->mono_font = f_ed;

                if (f15.texture.id != 0) {
                    SetTextureFilter(f15.texture, TEXTURE_FILTER_BILINEAR);
                    ui->font_ui = f15;
                } else {
                    ui->font_ui = f_ed;
                }

                if (f13.texture.id != 0) {
                    SetTextureFilter(f13.texture, TEXTURE_FILTER_BILINEAR);
                    ui->font_sm = f13;
                } else {
                    ui->font_sm = ui->font_ui;
                }

                if (f32.texture.id != 0) {
                    GenTextureMipmaps(&f32.texture);
                    SetTextureFilter(f32.texture, TEXTURE_FILTER_TRILINEAR);
                    ui->font_canvas = f32;
                } else {
                    ui->font_canvas = f_ed;
                }

                GuiSetFont(ui->font_ui);
                ui->has_custom_font = true;
                break;
            }
        }
    }

    /* Apply active Monkeytype-inspired Theme Palette & i18n language */
    i18n_set_language((AppLanguage)ui->config.language);
    ui->theme = theme_get_palette(ui->config.theme_id);
    theme_apply_to_raygui(&ui->theme);

    /* Populate dynamic templates list (Built-in + User/Professor Folder) */
    templates_refresh_list(ui);

    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Rectangle initial_canvas = {
        sw * ui->split_vertical_ratio,
        MENU_BAR_H,
        sw * (1.0f - ui->split_vertical_ratio),
        sh - MENU_BAR_H - FOOTER_BAR_H
    };
    layout_init_camera(ui, initial_canvas);

    if (session) {
        session->user_asan_pref       = ui->config.asan_enabled;
        session->watchdog_timeout_sec = ui->config.watchdog_timeout_sec;
        session->speed_slider_t       = ui->config.default_speed_t;
        session->play_speed           = 0.05f * powf(2000.0f, session->speed_slider_t);

        bool restored = false;
        if (ui->config.restore_last_session) {
            if (ui->config.last_open_file[0] != '\0' && access(ui->config.last_open_file, R_OK) == 0) {
                restored = load_file_into_editor(session, ui, ui->config.last_open_file, true);
            } else if (ui->config.last_selected_template >= 0 &&
                       ui->config.last_selected_template < ui->template_count) {
                apply_template_by_index(session, ui, ui->config.last_selected_template);
                restored = true;
            }
        }
        if (!restored) {
            int def_idx = (ui->template_count > 1) ? 1 : 0;
            apply_template_by_index(session, ui, def_idx);
        }
    }

    ui_workspace_update_window_title(ui);
}

void ui_workspace_Unload(WorkspaceUI *ui)
{
    if (!ui) return;
    editor_clear_undo_history(ui);
    if (ui->has_custom_font) {
        unsigned int def_id = GetFontDefault().texture.id;
        unsigned int m_id   = ui->mono_font.texture.id;
        unsigned int u_id   = ui->font_ui.texture.id;
        unsigned int s_id   = ui->font_sm.texture.id;
        unsigned int c_id   = ui->font_canvas.texture.id;

        if (c_id != 0 && c_id != def_id && c_id != m_id && c_id != u_id && c_id != s_id) {
            UnloadFont(ui->font_canvas);
        }
        if (s_id != 0 && s_id != def_id && s_id != m_id && s_id != u_id) {
            UnloadFont(ui->font_sm);
        }
        if (u_id != 0 && u_id != def_id && u_id != m_id) {
            UnloadFont(ui->font_ui);
        }
        if (m_id != 0 && m_id != def_id) {
            UnloadFont(ui->mono_font);
        }
        ui->has_custom_font = false;
    }
}

/* ============================================================================
 * 5. UTF-8 NAVIGATION, WORD JUMPING, EXACT MEASUREMENT & SELECTION ENGINE
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
    float       scroll_y,
    float       font_size,
    float       line_h
)
{
    int code_len = (int)strlen(code);
    int target_line = (int)floorf((mouse.y - text_start_y + scroll_y) / line_h) + 1;
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
        float w = measure_line_prefix_width(font, code + line_start, next_col, font_size);
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

static void editor_ensure_cursor_visible(
    Font         font,
    const char  *code,
    WorkspaceUI *ui,
    Rectangle    editor_bounds,
    float        gutter_w,
    float        font_size,
    float        line_h
)
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

    float cursor_y = (float)(line_num - 1) * line_h;
    float view_h   = editor_bounds.height - 20.0f;
    if (cursor_y < ui->target_scroll_y) {
        ui->target_scroll_y = cursor_y;
    } else if (cursor_y + line_h > ui->target_scroll_y + view_h) {
        ui->target_scroll_y = fmaxf(0.0f, cursor_y + line_h - view_h);
    }

    float cursor_x = measure_line_prefix_width(font, code + line_start, cur - line_start, font_size);
    float view_w   = editor_bounds.width - gutter_w - 28.0f;
    if (cursor_x < ui->target_scroll_x) {
        ui->target_scroll_x = fmaxf(0.0f, cursor_x - 24.0f);
    } else if (cursor_x > ui->target_scroll_x + view_w) {
        ui->target_scroll_x = cursor_x - view_w + 24.0f;
    }
}

/* ============================================================================
 * 6. C SYNTAX HIGHLIGHTER & CONFIGURABLE CODE EDITOR
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

static void draw_syntax_colored_line(
    Font                font,
    const char         *line_str,
    Vector2             pos,
    float               font_size,
    const ThemePalette *pal
)
{
    const char *p = line_str;
    float y = pos.y;

    while (*p) {
        int byte_offset = (int)(p - line_str);
        float exact_x   = pos.x + measure_line_prefix_width(font, line_str, byte_offset, font_size);

        if (p[0] == '/' && p[1] == '/') {
            DrawTextEx(font, p, (Vector2){ exact_x, y }, font_size, 1.0f, pal->syntax_comment);
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
            DrawTextEx(font, tok, (Vector2){ exact_x, y }, font_size, 1.0f, pal->syntax_string);
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

            Color col = pal->text_primary;
            if (is_c_keyword(tok, len)) {
                col = pal->syntax_keyword;
            } else if (is_c_builtin_or_type(tok, len)) {
                col = pal->syntax_type;
            } else if (*p == '(') {
                col = pal->syntax_func;
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

            DrawTextEx(font, tok, (Vector2){ exact_x, y }, font_size, 1.0f, pal->syntax_number);
        } else {
            int next_off = utf8_next_char_pos(p, 0, (int)strlen(p));
            char ch[8];
            if (next_off >= (int)sizeof(ch)) next_off = (int)sizeof(ch) - 1;
            memcpy(ch, p, (size_t)next_off);
            ch[next_off] = '\0';

            Color col = (next_off == 1 && strchr("{}()[];->=<>+-*/&|!", *p) != NULL)
                        ? pal->text_secondary
                        : pal->text_primary;
            DrawTextEx(font, ch, (Vector2){ exact_x, y }, font_size, 1.0f, col);
            p += next_off;
        }
    }
}

static void update_and_draw_code_editor(
    DebuggerSession *session,
    WorkspaceUI     *ui,
    Rectangle        editor_bounds,
    bool             overlay_blocking
)
{
    ui_workspace_reload_editor_font(ui);

    Font font    = ui->mono_font;
    Font ui_font = ui->font_ui;
    const ThemePalette *pal = &ui->theme;
    const AppConfig    *cfg = &ui->config;

    float font_size = (float)(ui->loaded_editor_font_px > 0 ? ui->loaded_editor_font_px : 16);
    float line_h    = floorf(font_size * 1.32f);
    float gutter_w  = cfg->show_line_numbers ? 52.0f : 14.0f;

    char *code = session->code_buffer;
    int code_len = (int)strlen(code);

    if (ui->sel_anchor_pos > code_len) ui->sel_anchor_pos = code_len;
    if (ui->sel_anchor_pos < 0)        ui->sel_anchor_pos = 0;
    if (ui->sel_active_pos > code_len) ui->sel_active_pos = code_len;
    if (ui->sel_active_pos < 0)        ui->sel_active_pos = 0;

    Vector2 mouse = GetMousePosition();
    float terminal_top_y = (float)GetScreenHeight() - FOOTER_BAR_H - ui->terminal_anim_h;
    bool over_terminal   = (ui->terminal_anim_h > 1.5f) && (mouse.y >= terminal_top_y);
    bool hovered = !overlay_blocking &&
                   CheckCollisionPointRec(mouse, editor_bounds) &&
                   !over_terminal &&
                   !ui->dropdown_edit_mode &&
                   !ui->dragging_v_split;

    if (!overlay_blocking && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui->has_focus = hovered;
        if (hovered) {
            ui->stdin_textbox_focused = false;
        } else {
            ui->is_selecting_with_mouse = false;
        }
    }

    /* Smooth Mouse Wheel Scrolling (vertical, native horizontal, & Shift+wheel horizontal) */
    if (hovered) {
        Vector2 wheel_v = GetMouseWheelMoveV();
        bool ctrl_held = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        if (!ctrl_held && (wheel_v.y != 0.0f || wheel_v.x != 0.0f)) {
            if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) {
                ui->target_scroll_x = fmaxf(0.0f, ui->target_scroll_x - wheel_v.y * 56.0f);
            } else {
                if (wheel_v.y != 0.0f) {
                    ui->target_scroll_y = fmaxf(0.0f, ui->target_scroll_y - wheel_v.y * line_h * 3.5f);
                }
                if (wheel_v.x != 0.0f) {
                    ui->target_scroll_x = fmaxf(0.0f, ui->target_scroll_x - wheel_v.x * 56.0f);
                }
            }
        }
    }

    float text_start_x = editor_bounds.x + gutter_w + 10.0f;
    float text_start_y = editor_bounds.y + 8.0f;

    /* Mouse Click in Gutter (Toggle Breakpoint) or Code Area (Click, Double-Click, Triple-Click, Drag) */
    bool shift_down = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (hovered && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        if (mouse.x <= editor_bounds.x + gutter_w) {
            /* Clicked inside the line-number gutter -> Toggle line breakpoint! */
            int total_lines = 1;
            for (int i = 0; i < code_len; i++) {
                if (code[i] == '\n') total_lines++;
            }
            int clicked_line = (int)floorf((mouse.y - text_start_y + ui->scroll_y) / line_h) + 1;
            if (clicked_line >= 1 && clicked_line <= total_lines) {
                session_toggle_breakpoint(session, clicked_line);
                audio_play_sfx(SFX_UI_CLICK, cfg, 0.08f);
            }
            ui->is_selecting_with_mouse = false;
        } else {
            int hit = hit_test_editor_index(font, code, mouse, text_start_x, text_start_y, ui->scroll_x, ui->scroll_y, font_size, line_h);
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
                int ws = hit;
                int we = hit;
                while (ws > 0 && is_word_char(code[ws - 1])) ws--;
                while (we < code_len && is_word_char(code[we])) we++;
                ui->sel_anchor_pos          = ws;
                ui->sel_active_pos          = we;
                ui->is_selecting_with_mouse = false;
            } else if (ui->click_streak >= 3) {
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
    }

    if (ui->is_selecting_with_mouse) {
        if (!overlay_blocking && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            int hit = hit_test_editor_index(font, code, mouse, text_start_x, text_start_y, ui->scroll_x, ui->scroll_y, font_size, line_h);
            ui->sel_active_pos = hit;
            ui->cursor_blink_timer = 0.0f;

            if (mouse.y < editor_bounds.y + 18.0f) {
                ui->target_scroll_y = fmaxf(0.0f, ui->target_scroll_y - line_h * 0.45f);
            } else if (mouse.y > editor_bounds.y + editor_bounds.height - 18.0f) {
                ui->target_scroll_y += line_h * 0.45f;
            }
        } else {
            ui->is_selecting_with_mouse = false;
        }
    }

    /* Keyboard Editing, Fast Hold-to-Repeat Undo/Redo, PageUp/PageDown & Shortcuts */
    if (ui->has_focus && !overlay_blocking) {
        float dt_ed = GetFrameTime();
        ui->cursor_blink_timer += dt_ed;
        bool ctrl   = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
        bool moved  = false;
        bool edited = false;

        if (IsKeyPressed(KEY_PAGE_UP) || IsKeyPressedRepeat(KEY_PAGE_UP)) {
            ui->target_scroll_y = fmaxf(0.0f, ui->target_scroll_y - editor_bounds.height * 0.85f);
        } else if (IsKeyPressed(KEY_PAGE_DOWN) || IsKeyPressedRepeat(KEY_PAGE_DOWN)) {
            ui->target_scroll_y += editor_bounds.height * 0.85f;
        }

        /* Fast hold-to-repeat Ctrl+Z (Undo) and Ctrl+Y / Ctrl+Shift+Z (Redo) */
        bool z_down = ctrl && IsKeyDown(KEY_Z);
        bool y_down = ctrl && IsKeyDown(KEY_Y);
        if (z_down || y_down) {
            bool do_redo = y_down || shift_down;
            if ((z_down && IsKeyPressed(KEY_Z)) || (y_down && IsKeyPressed(KEY_Y))) {
                if (do_redo) editor_perform_redo(ui, code, MAX_CODE_SIZE);
                else         editor_perform_undo(ui, code, MAX_CODE_SIZE);
                ui->undo_hold_timer   = 0.0f;
                ui->undo_repeat_accum = 0.0f;
                moved = true;
            } else {
                ui->undo_hold_timer += dt_ed;
                if (ui->undo_hold_timer >= 0.25f) {
                    float repeat_interval = (ui->undo_hold_timer >= 0.90f) ? 0.025f : 0.050f;
                    ui->undo_repeat_accum += dt_ed;
                    while (ui->undo_repeat_accum >= repeat_interval) {
                        ui->undo_repeat_accum -= repeat_interval;
                        if (do_redo) editor_perform_redo(ui, code, MAX_CODE_SIZE);
                        else         editor_perform_undo(ui, code, MAX_CODE_SIZE);
                        moved = true;
                    }
                }
            }
        } else {
            ui->undo_hold_timer   = 0.0f;
            ui->undo_repeat_accum = 0.0f;
        }

        if (ctrl && IsKeyPressed(KEY_A)) {
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

        int tab_spaces = (cfg->tab_size == 2) ? 2 : 4;
        if (IsKeyPressed(KEY_TAB)) {
            if (shift_down) {
                int ls = ui->sel_active_pos;
                while (ls > 0 && code[ls - 1] != '\n') ls--;
                int rem = 0;
                while (rem < tab_spaces && code[ls + rem] == ' ') rem++;
                if (rem > 0) {
                    int len = (int)strlen(code);
                    memmove(code + ls, code + ls + rem, (size_t)(len - (ls + rem) + 1));
                    ui->sel_active_pos = (ui->sel_active_pos - rem >= ls) ? (ui->sel_active_pos - rem) : ls;
                    ui->sel_anchor_pos = ui->sel_active_pos;
                    edited = true;
                }
            } else {
                editor_insert_text(code, MAX_CODE_SIZE, ui, (tab_spaces == 2) ? "  " : "    ");
                edited = true;
            }
        }

        if (IsKeyPressed(KEY_ENTER) || IsKeyPressedRepeat(KEY_ENTER)) {
            editor_delete_selection(code, ui);
            if (cfg->auto_indent) {
                int cur = ui->sel_active_pos;
                int line_start = cur;
                while (line_start > 0 && code[line_start - 1] != '\n') line_start--;
                int spaces = 0;
                while (code[line_start + spaces] == ' ' && spaces < 24) spaces++;
                if (cur > 0 && code[cur - 1] == '{') spaces += tab_spaces;

                char nl_indent[32];
                nl_indent[0] = '\n';
                for (int i = 0; i < spaces && i < 28; i++) nl_indent[1 + i] = ' ';
                nl_indent[1 + spaces] = '\0';
                editor_insert_text(code, MAX_CODE_SIZE, ui, nl_indent);
            } else {
                editor_insert_text(code, MAX_CODE_SIZE, ui, "\n");
            }
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
            float cur_px = measure_line_prefix_width(font, code + ls, ui->sel_active_pos - ls, font_size);
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
                    float w = measure_line_prefix_width(font, code + prev_ls, nc, font_size);
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
            float cur_px = measure_line_prefix_width(font, code + ls, ui->sel_active_pos - ls, font_size);
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
                    float w = measure_line_prefix_width(font, code + next_ls, nc, font_size);
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
            if (!ui->is_dirty) {
                ui->is_dirty = true;
                ui_workspace_update_window_title(ui);
            }
        }
        if (edited || moved) {
            ui->cursor_blink_timer = 0.0f;
            editor_ensure_cursor_visible(font, code, ui, editor_bounds, gutter_w, font_size, line_h);
        }
    }

    /* Render Theme Editor Background & Line Gutter */
    DrawRectangleRec(editor_bounds, pal->bg_panel);
    if (cfg->show_line_numbers) {
        Rectangle gutter_rect = { editor_bounds.x, editor_bounds.y, gutter_w, editor_bounds.height };
        DrawRectangleRec(gutter_rect, pal->bg_gutter);
        DrawLineEx(
            (Vector2){ editor_bounds.x + gutter_w, editor_bounds.y },
            (Vector2){ editor_bounds.x + gutter_w, editor_bounds.y + editor_bounds.height },
            1.0f,
            pal->border
        );
    }

    BeginScissorMode((int)editor_bounds.x, (int)editor_bounds.y, (int)editor_bounds.width, (int)editor_bounds.height);

    int active_line = (session->diagnostic.error_type == ERR_COMPILE)
                      ? session->diagnostic.fault_line
                      : session->snapshot.current_executing_line;
    bool is_error_line = (session->diagnostic.error_type != ERR_NONE);

    /* Automatically keep the executing or fault line smoothly in view when stepping */
    if (session->state != STATE_IDLE && active_line > 0) {
        if (ui->last_followed_exec_line != active_line) {
            ui->last_followed_exec_line = active_line;
            float line_top = (float)(active_line - 1) * line_h;
            float view_h   = editor_bounds.height - 20.0f;
            if (cfg->presentation_mode || line_top < ui->target_scroll_y + line_h ||
                line_top + line_h * 2.0f > ui->target_scroll_y + view_h) {
                ui->target_scroll_y = fmaxf(0.0f, line_top - view_h * 0.36f);
            }
        }
    } else {
        ui->last_followed_exec_line = 0;
    }

    float scroll_lerp = avora_exp_decay(20.0f, GetFrameTime(), cfg->reduce_animations);
    if (ui->target_scroll_y < 0.0f) ui->target_scroll_y = 0.0f;
    if (ui->target_scroll_x < 0.0f) ui->target_scroll_x = 0.0f;
    ui->scroll_y += (ui->target_scroll_y - ui->scroll_y) * scroll_lerp;
    ui->scroll_x += (ui->target_scroll_x - ui->scroll_x) * scroll_lerp;
    if (fabsf(ui->target_scroll_y - ui->scroll_y) < 0.35f) ui->scroll_y = ui->target_scroll_y;
    if (fabsf(ui->target_scroll_x - ui->scroll_x) < 0.35f) ui->scroll_x = ui->target_scroll_x;

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
        float line_w = measure_line_prefix_width(font, code + idx, llen, font_size);
        if (line_w > max_line_w) max_line_w = line_w;

        float y = text_start_y + (float)(line_num - 1) * line_h - ui->scroll_y;
        bool caret_on_this_line = (ui->sel_active_pos >= idx && ui->sel_active_pos <= line_end);

        if (y + line_h >= editor_bounds.y && y <= editor_bounds.y + editor_bounds.height) {
            /* 1a. Highlight current caret line in editor if configured */
            if (cfg->highlight_current_line && ui->has_focus && caret_on_this_line &&
                !(session->state != STATE_IDLE && line_num == active_line)) {
                DrawRectangle(
                    (int)(editor_bounds.x + gutter_w + 1.0f),
                    (int)y,
                    (int)(editor_bounds.width - gutter_w - 2.0f),
                    (int)line_h,
                    Fade(pal->bg_hover, 0.45f)
                );
            }

            /* 1b. Highlight active debugger line or fault line */
            if (session->state != STATE_IDLE && line_num == active_line) {
                Color hl  = is_error_line ? Fade(pal->accent_error, 0.22f)   : Fade(pal->accent_warning, 0.20f);
                Color bar = is_error_line ? pal->accent_error                : pal->accent_warning;
                DrawRectangle(
                    (int)(editor_bounds.x + gutter_w + 1.0f),
                    (int)y,
                    (int)(editor_bounds.width - gutter_w - 2.0f),
                    (int)line_h,
                    hl
                );
                DrawRectangle((int)(editor_bounds.x + gutter_w + 1.0f), (int)y, 4, (int)line_h, bar);
            }

            /* 2. Breakpoint badge & line number in gutter */
            bool has_bp = session_has_breakpoint(session, line_num);
            bool gutter_hov = hovered && (mouse.x >= editor_bounds.x && mouse.x <= editor_bounds.x + gutter_w) &&
                              (mouse.y >= y && mouse.y < y + line_h);
            if (has_bp || gutter_hov) {
                float bp_cx = editor_bounds.x + (cfg->show_line_numbers ? 9.5f : 7.0f);
                float bp_cy = floorf(y + line_h * 0.5f + 0.5f);
                DrawCircle((int)bp_cx, (int)bp_cy, has_bp ? 4.8f : 4.2f,
                           has_bp ? pal->accent_error : Fade(pal->accent_error, 0.38f));
                if (has_bp) {
                    DrawCircleLines((int)bp_cx, (int)bp_cy, 4.8f, Fade(WHITE, 0.45f));
                }
            }

            if (cfg->show_line_numbers) {
                char lnum_str[16];
                snprintf(lnum_str, sizeof(lnum_str), "%2d", line_num);
                Color lnum_col = has_bp
                                 ? pal->accent_error
                                 : ((session->state != STATE_IDLE && line_num == active_line)
                                     ? pal->accent_warning
                                     : (caret_on_this_line ? pal->text_primary : pal->text_secondary));
                DrawTextEx(
                    ui_font,
                    lnum_str,
                    (Vector2){ floorf(editor_bounds.x + 17.0f + 0.5f), floorf(y + (line_h - 15.0f) * 0.5f + 0.5f) },
                    15.0f,
                    1.0f,
                    lnum_col
                );
            }

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

            float draw_x = floorf(text_start_x - ui->scroll_x + 0.5f);

            /* 4. Visual Selection Highlight Rectangle */
            if (sel_max > sel_min && sel_max > idx && sel_min <= line_end) {
                int local_s = (sel_min > idx) ? (sel_min - idx) : 0;
                int local_e = (sel_max < line_end) ? (sel_max - idx) : llen;
                if (local_s > llen) local_s = llen;
                if (local_e > llen) local_e = llen;

                float sx = draw_x + measure_line_prefix_width(font, line_buf, local_s, font_size);
                float ex = draw_x + measure_line_prefix_width(font, line_buf, local_e, font_size);
                if (sel_max > line_end) ex += 8.0f;
                if (ex > sx) {
                    DrawRectangleRec(
                        (Rectangle){ sx, y + 1.0f, ex - sx, line_h - 1.0f },
                        Fade(pal->accent_secondary, 0.25f)
                    );
                }
            }

            /* 5. Syntax-colored characters */
            draw_syntax_colored_line(font, line_buf, (Vector2){ draw_x, floorf(y + (line_h - font_size) * 0.5f + 0.5f) }, font_size, pal);

            /* 6. Blinking caret at sel_active_pos */
            if (ui->has_focus && caret_on_this_line) {
                if (((int)(ui->cursor_blink_timer * 2.2f) % 2) == 0) {
                    int col_bytes = ui->sel_active_pos - idx;
                    if (col_bytes > llen) col_bytes = llen;
                    float cx = draw_x + measure_line_prefix_width(font, line_buf, col_bytes, font_size);
                    DrawRectangle((int)cx, (int)(y + 2.0f), 2, (int)(line_h - 3.0f), pal->accent_secondary);
                }
            }
            EndScissorMode();
        }

        if (line_end == total_len) break;
        idx = line_end + 1;
        line_num++;
    }

    /* Clamp scroll targets & draw subtle Horizontal & Vertical Scrollbars */
    float content_h    = (float)line_num * line_h + 24.0f;
    float max_scroll_y = fmaxf(0.0f, content_h - editor_bounds.height);
    if (ui->target_scroll_y > max_scroll_y) ui->target_scroll_y = max_scroll_y;
    if (ui->scroll_y > max_scroll_y)        ui->scroll_y        = max_scroll_y;

    if (content_h > editor_bounds.height) {
        float sb_h = fmaxf(24.0f, (editor_bounds.height / content_h) * editor_bounds.height);
        float ratio_y = (max_scroll_y > 0.0f) ? (ui->scroll_y / max_scroll_y) : 0.0f;
        float sb_y = editor_bounds.y + ratio_y * (editor_bounds.height - sb_h);
        DrawRectangleRounded(
            (Rectangle){ editor_bounds.x + editor_bounds.width - 6.0f, sb_y, 4.0f, sb_h },
            0.5f, 4, Fade(pal->text_secondary, 0.55f)
        );
    }
    float view_w       = editor_bounds.width - gutter_w - 16.0f;
    float max_scroll_x = fmaxf(0.0f, max_line_w + 40.0f - view_w);
    if (ui->target_scroll_x > max_scroll_x) ui->target_scroll_x = max_scroll_x;
    if (ui->scroll_x > max_scroll_x)        ui->scroll_x        = max_scroll_x;

    if (max_line_w > view_w) {
        float sb_w = fmaxf(28.0f, (view_w / (max_line_w + 40.0f)) * view_w);
        float ratio_x = (max_scroll_x > 0.0f) ? (ui->scroll_x / max_scroll_x) : 0.0f;
        float sb_x = editor_bounds.x + gutter_w + ratio_x * (view_w - sb_w);
        DrawRectangleRounded(
            (Rectangle){ sb_x, editor_bounds.y + editor_bounds.height - 6.0f, sb_w, 4.0f },
            0.5f, 4, Fade(pal->text_secondary, 0.55f)
        );
    }

    EndScissorMode();
    DrawRectangleLinesEx(editor_bounds, 1.0f, ui->has_focus ? Fade(pal->accent_secondary, 0.75f) : pal->border);
}

/* ============================================================================
 * 7. RETRACTABLE TERMINAL DRAWER (GCC / ASAN LOGS & REAL-TIME STDOUT)
 * ============================================================================ */

static void draw_multiline_log(
    Font                font,
    const char         *text,
    Rectangle           bounds,
    float               scroll_y,
    float               font_size,
    Color               default_col,
    const ThemePalette *pal
)
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
                lcol = pal->accent_error;
            } else if (strstr(line, "warning:") != NULL || strstr(line, "Nota:") != NULL) {
                lcol = pal->accent_warning;
            }
            DrawTextEx(font, line, (Vector2){ floorf(bounds.x + 0.5f), floorf(y + 0.5f) }, font_size, 1.0f, lcol);
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
    Font             font_ui,
    Font             font_sm,
    bool             overlay_blocking
)
{
    const ThemePalette *pal = &ui->theme;
    float dt = GetFrameTime();
    float target_h = ui->terminal_open ? 215.0f : 0.0f;
    float anim_rate = avora_exp_decay(16.0f, dt, ui->config.reduce_animations);
    ui->terminal_anim_h += (target_h - ui->terminal_anim_h) * anim_rate;
    if (ui->terminal_anim_h < 1.5f) {
        if (!ui->terminal_open) ui->terminal_anim_h = 0.0f;
        return;
    }

    float anim_h = floorf(ui->terminal_anim_h + 0.5f);
    Rectangle drawer = {
        0.0f,
        footer_top_y - anim_h,
        sw,
        anim_h
    };
    Vector2 mouse = GetMousePosition();
    if (!overlay_blocking && CheckCollisionPointRec(mouse, drawer)) {
        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) {
            ui->terminal_target_scroll_y = fmaxf(0.0f, ui->terminal_target_scroll_y - wheel * 42.0f);
        }
    }
    float t_lerp = avora_exp_decay(20.0f, dt, ui->config.reduce_animations);
    ui->terminal_scroll_y += (ui->terminal_target_scroll_y - ui->terminal_scroll_y) * t_lerp;
    if (fabsf(ui->terminal_target_scroll_y - ui->terminal_scroll_y) < 0.35f) {
        ui->terminal_scroll_y = ui->terminal_target_scroll_y;
    }

    /* Drawer Surface & Top Border */
    DrawRectangleRec(drawer, Fade(pal->bg_gutter, 0.98f));
    DrawLineEx((Vector2){ 0.0f, drawer.y }, (Vector2){ sw, drawer.y }, 1.5f, pal->accent_secondary);

    /* Drawer Header Bar with Dynamically Sized Tabs */
    Rectangle hdr = { drawer.x, drawer.y, drawer.width, 32.0f };
    DrawRectangleRec(hdr, pal->bg_panel);
    DrawLineEx((Vector2){ 0.0f, hdr.y + hdr.height }, (Vector2){ sw, hdr.y + hdr.height }, 1.0f, pal->border);

    const float tab_fs  = 13.0f;
    const float tab_pad = 24.0f;

    /* Tab 0: Diagnóstico & Compilador (GCC/ASan) */
    const char *t0_lbl = tr(STR_TAB_DIAGNOSTICS);
    Vector2 t0_sz = MeasureTextEx(font_sm, t0_lbl, tab_fs, 1.0f);
    Rectangle tab0 = { 12.0f, hdr.y + 4.0f, floorf(t0_sz.x + tab_pad), 24.0f };
    bool hov0 = !overlay_blocking && CheckCollisionPointRec(mouse, tab0);
    DrawRectangleRounded(tab0, 0.25f, 4, (ui->terminal_active_tab == 0) ? pal->bg_elevated : pal->bg_panel);
    DrawRectangleRoundedLinesEx(tab0, 0.25f, 4, 1.0f, (ui->terminal_active_tab == 0) ? pal->accent_secondary : pal->border);
    DrawTextEx(
        font_sm,
        t0_lbl,
        (Vector2){ floorf(tab0.x + (tab0.width - t0_sz.x) * 0.5f + 0.5f), floorf(tab0.y + (tab0.height - t0_sz.y) * 0.5f + 0.5f) },
        tab_fs,
        1.0f,
        (ui->terminal_active_tab == 0 || hov0) ? pal->text_primary : pal->text_secondary
    );
    if (hov0 && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->terminal_active_tab      = 0;
        ui->terminal_scroll_y        = 0.0f;
        ui->terminal_target_scroll_y = 0.0f;
    }

    /* Tab 1: Terminal (stdout + optional stdin split) */
    const char *t1_max_lbl = tr(STR_TAB_TERMINAL_NEW);
    const char *t1_lbl     = session->has_new_stdout ? t1_max_lbl : tr(STR_TAB_TERMINAL);
    Vector2 t1_max_sz = MeasureTextEx(font_sm, t1_max_lbl, tab_fs, 1.0f);
    Vector2 t1_sz     = MeasureTextEx(font_sm, t1_lbl, tab_fs, 1.0f);
    Rectangle tab1 = { tab0.x + tab0.width + 8.0f, hdr.y + 4.0f, floorf(t1_max_sz.x + tab_pad + 10.0f), 24.0f };
    bool hov1 = !overlay_blocking && CheckCollisionPointRec(mouse, tab1);
    DrawRectangleRounded(tab1, 0.25f, 4, (ui->terminal_active_tab == 1) ? pal->bg_elevated : pal->bg_panel);
    DrawRectangleRoundedLinesEx(tab1, 0.25f, 4, 1.0f, (ui->terminal_active_tab == 1) ? pal->accent_primary : pal->border);
    DrawTextEx(
        font_sm,
        t1_lbl,
        (Vector2){ floorf(tab1.x + (tab1.width - t1_sz.x) * 0.5f + 0.5f), floorf(tab1.y + (tab1.height - t1_sz.y) * 0.5f + 0.5f) },
        tab_fs,
        1.0f,
        (ui->terminal_active_tab == 1 || hov1) ? pal->text_primary :
        (session->has_new_stdout ? pal->accent_primary : pal->text_secondary)
    );
    if (hov1 && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->terminal_active_tab      = 1;
        ui->terminal_scroll_y        = 0.0f;
        ui->terminal_target_scroll_y = 0.0f;
        session->has_new_stdout      = false;
    }

    /* Close Button [ ✕ Ocultar ▼ ] */
    const char *close_lbl = tr(STR_BTN_HIDE_TERMINAL);
    Vector2 close_sz = MeasureTextEx(font_sm, close_lbl, tab_fs, 1.0f);
    float close_w = floorf(close_sz.x + 20.0f);
    Rectangle btn_close = { floorf(sw - close_w - 12.0f), hdr.y + 4.0f, close_w, 24.0f };
    bool hov_close = !overlay_blocking && CheckCollisionPointRec(mouse, btn_close);
    DrawRectangleRounded(btn_close, 0.25f, 4, hov_close ? pal->bg_hover : pal->bg_elevated);
    DrawTextEx(
        font_sm,
        close_lbl,
        (Vector2){ floorf(btn_close.x + (btn_close.width - close_sz.x) * 0.5f + 0.5f), floorf(btn_close.y + (btn_close.height - close_sz.y) * 0.5f + 0.5f) },
        tab_fs,
        1.0f,
        pal->text_primary
    );
    if (hov_close && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->terminal_open         = false;
        ui->stdin_textbox_focused = false;
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
                     ? pal->accent_error
                     : pal->accent_secondary;
        DrawTextEx(
            font_ui,
            session->diagnostic.pedagogical_title,
            (Vector2){ floorf(content.x + 0.5f), floorf(content.y - ui->terminal_scroll_y + 0.5f) },
            15.0f,
            1.0f,
            tcol
        );
        DrawTextEx(
            font_sm,
            session->diagnostic.pedagogical_explanation,
            (Vector2){ floorf(content.x + 0.5f), floorf(content.y + 22.0f - ui->terminal_scroll_y + 0.5f) },
            13.0f,
            1.0f,
            pal->text_primary
        );

        const char *raw_log = session->diagnostic.raw_compiler_log[0]
                              ? session->diagnostic.raw_compiler_log
                              : (session->stderr_buf[0] ? session->stderr_buf : "Nenhum erro ou warning emitido pelo GCC / AddressSanitizer.");
        Rectangle raw_rect = { content.x, content.y + 48.0f, content.width, content.height - 48.0f };
        draw_multiline_log(font_sm, raw_log, raw_rect, ui->terminal_scroll_y, 13.0f, pal->text_secondary, pal);
    } else {
        session->has_new_stdout = false;
        const char *out_txt = session->program_stdout[0]
                              ? session->program_stdout
                              : "(Nenhuma saída impressa em stdout ainda. Use printf(\"Valor = %d\\n\", x); no seu código C!)";

        if (!ui->config.enable_stdin) {
            ui->stdin_textbox_focused = false;
            draw_multiline_log(
                font_sm,
                out_txt,
                content,
                ui->terminal_scroll_y,
                13.0f,
                session->program_stdout[0] ? pal->accent_primary : pal->text_secondary,
                pal
            );
        } else {
            /* Clean Split View: Left = Entrada (stdin), Right = Saída (stdout) */
            float left_in_w = floorf(content.width * 0.34f);
            Rectangle in_box  = { content.x, content.y + 22.0f, left_in_w - 12.0f, fminf(34.0f, content.height - 26.0f) };
            Rectangle out_box = { content.x + left_in_w + 10.0f, content.y + 20.0f, content.width - left_in_w - 10.0f, content.height - 20.0f };

            DrawTextEx(font_sm, "Entrada (stdin — tokens separados por espaço):",
                       (Vector2){ content.x, content.y + 2.0f }, 13.0f, 1.0f, pal->accent_secondary);

            if (GuiTextBox(in_box, session->stdin_buffer, sizeof(session->stdin_buffer), ui->stdin_textbox_focused)) {
                ui->stdin_textbox_focused = !ui->stdin_textbox_focused;
                if (ui->stdin_textbox_focused) {
                    ui->has_focus = false;
                }
            }

            if (content.height > 85.0f) {
                DrawTextEx(font_sm,
                           "Dica: separe múltiplos valores com espaço (ex: 5 10 20 30).\nSe vazio, scanf recebe EOF imediato sem travar.",
                           (Vector2){ content.x, in_box.y + in_box.height + 8.0f }, 13.0f, 1.0f, pal->text_secondary);
            }

            DrawLineEx((Vector2){ content.x + left_in_w, content.y },
                       (Vector2){ content.x + left_in_w, content.y + content.height },
                       1.0f, pal->border);

            DrawTextEx(font_sm, "Saída do Programa (stdout / printf):",
                       (Vector2){ out_box.x, content.y + 2.0f }, 13.0f, 1.0f, pal->accent_primary);
            draw_multiline_log(
                font_sm,
                out_txt,
                out_box,
                ui->terminal_scroll_y,
                13.0f,
                session->program_stdout[0] ? pal->accent_primary : pal->text_secondary,
                pal
            );
        }
    }
    EndScissorMode();
}

/* ============================================================================
 * 8. DEBUGGER EXECUTION ACTIONS & GLOBAL SHORTCUTS
 * ============================================================================ */

static bool code_uses_stdin_functions(const char *code)
{
    if (!code) return false;
    const char *p = code;
    while (*p) {
        if (p[0] == '/' && p[1] == '/') {
            while (*p && *p != '\n') p++;
            continue;
        }
        if (p[0] == '/' && p[1] == '*') {
            p += 2;
            while (*p && !(p[0] == '*' && p[1] == '/')) p++;
            if (*p) p += 2;
            continue;
        }
        if (p[0] == '"') {
            p++;
            while (*p && *p != '"') {
                if (*p == '\\' && p[1]) p += 2;
                else p++;
            }
            if (*p == '"') p++;
            continue;
        }
        if (isalpha((unsigned char)*p) || *p == '_') {
            const char *start = p;
            while (isalnum((unsigned char)*p) || *p == '_') p++;
            size_t len = (size_t)(p - start);
            const char *after = p;
            while (*after && isspace((unsigned char)*after)) after++;
            if (*after == '(') {
                if ((len == 5 && strncmp(start, "scanf", 5) == 0)   ||
                    (len == 5 && strncmp(start, "fgets", 5) == 0)   ||
                    (len == 7 && strncmp(start, "getchar", 7) == 0) ||
                    (len == 7 && strncmp(start, "getline", 7) == 0) ||
                    (len == 4 && strncmp(start, "getc", 4) == 0)) {
                    return true;
                }
            }
            continue;
        }
        p++;
    }
    return false;
}

static void trigger_compile_and_start(DebuggerSession *session, WorkspaceUI *ui)
{
    ui->blob_idle_timer           = 0.0f;
    ui->camera_auto_focus         = true;
    session->user_asan_pref       = ui->config.asan_enabled;
    session->watchdog_timeout_sec = ui->config.watchdog_timeout_sec;
    session->enable_stdin         = ui->config.enable_stdin;

    /* Smart Blob Contextual Detection: prompt user if code uses scanf/fgets while stdin is disabled */
    if (!ui->config.enable_stdin && code_uses_stdin_functions(session->code_buffer)) {
        ui->config.show_blob   = true;
        ui->active_blob_topic  = BLOB_HINT_STDIN_DETECTED;
        ui->blob_anim_progress = 0.08f;
    }

    if (compile_student_code(session)) {
        ui->terminal_open = false;
        audio_play_sfx(SFX_COMPILE_CHIME, &ui->config, 0.0f);
        if (gdb_start_session(session)) {
            if (!ui->config.start_paused_on_main) {
                session->auto_play = true;
                session->state     = STATE_RUNNING;
            }
        }
    } else {
        ui->terminal_open       = true;
        ui->terminal_active_tab = 0;
        ui->error_shake_timer   = 0.16f;
        ui->error_flash_timer   = 0.16f;
        audio_play_sfx(SFX_ERROR_THUD, &ui->config, 0.0f);
    }
}

static void handle_global_shortcuts(DebuggerSession *session, WorkspaceUI *ui, float step_interval_sec)
{
    /* ESC closes modals, open menus or open dropdowns */
    if (IsKeyPressed(KEY_ESCAPE)) {
        if (ui->modal_path_input_open) {
            ui->modal_path_input_open = false;
            return;
        }
        if (ui->modal_unsaved_confirm_open) {
            ui->modal_unsaved_confirm_open = false;
            ui->pending_action_type        = 0;
            return;
        }
        if (ui->modal_shortcuts_open) {
            ui->modal_shortcuts_open = false;
            return;
        }
        if (ui->modal_about_open) {
            ui->modal_about_open = false;
            return;
        }
        if (ui->modal_sysdiag_open) {
            ui->modal_sysdiag_open = false;
            return;
        }
        if (ui->modal_settings_open) {
            ui->modal_settings_open = false;
            config_save(&ui->config);
            return;
        }
        if (ui->active_menu >= 0) {
            ui->active_menu         = -1;
            ui->show_recent_submenu = false;
            return;
        }
        if (ui->dropdown_edit_mode) {
            ui->dropdown_edit_mode = false;
            return;
        }
        if (ui->config.presentation_mode) {
            if (ui->has_focus) {
                ui->has_focus = false;
                show_toast(ui, "Palco: Controles de seta / passador Bluetooth ativos");
            } else {
                ui->config.presentation_mode = false;
                ui_workspace_reload_editor_font(ui);
                config_save(&ui->config);
                show_toast(ui, "Modo Apresentação DESATIVADO");
            }
            return;
        }
    }

    if (ui->modal_path_input_open || ui->modal_unsaved_confirm_open ||
        ui->modal_settings_open   || ui->modal_shortcuts_open ||
        ui->modal_about_open      || ui->modal_sysdiag_open) return;

    bool ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    bool shift = IsKeyDown(KEY_LEFT_SHIFT)   || IsKeyDown(KEY_RIGHT_SHIFT);
    bool is_running_session = (session->gdb_pid > 0) &&
                              (session->state == STATE_PAUSED ||
                               session->state == STATE_RUNNING ||
                               session->state == STATE_STEPPING);

    /* Presentation / Stage Mode (Slide Clicker & Arrow-Key Navigation State Machine) */
    if (ui->config.presentation_mode && !ctrl && !ui->dropdown_edit_mode && ui->active_menu < 0) {
        bool stage_keys_free = (!ui->has_focus && !ui->stdin_textbox_focused);

        /* Slide Presenter Next (PageDown) OR Right Arrow / Space when not typing in editor */
        if (IsKeyPressed(KEY_PAGE_DOWN) ||
            (stage_keys_free && (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_SPACE)))) {
            ui->blob_idle_timer   = 0.0f;
            ui->camera_auto_focus = true;
            if (!is_running_session) {
                trigger_compile_and_start(session, ui);
                ui->has_focus     = false;
                ui->terminal_open = false;
            } else {
                session->auto_play = false;
                int prev_cur = session->history_cursor;
                session_step_forward_or_gdb(session);
                if (session->history_cursor != prev_cur) {
                    audio_play_sfx(SFX_STEP_TICK, &ui->config, 0.05f);
                }
            }
            return;
        }

        /* Slide Presenter Prev (PageUp) OR Left Arrow when not typing in editor */
        if (IsKeyPressed(KEY_PAGE_UP) || (stage_keys_free && IsKeyPressed(KEY_LEFT))) {
            ui->blob_idle_timer   = 0.0f;
            ui->camera_auto_focus = true;
            if (is_running_session) {
                session->auto_play = false;
                if (session_step_back(session)) {
                    audio_play_sfx(SFX_STEP_TICK, &ui->config, -0.10f);
                }
            }
            return;
        }

        /* Smooth Code Scrolling via Up / Down Arrows in Stage Mode */
        if (stage_keys_free) {
            if (IsKeyPressed(KEY_UP) || IsKeyPressedRepeat(KEY_UP)) {
                ui->target_scroll_y = fmaxf(0.0f, ui->target_scroll_y - 54.0f);
                return;
            }
            if (IsKeyPressed(KEY_DOWN) || IsKeyPressedRepeat(KEY_DOWN)) {
                ui->target_scroll_y += 54.0f;
                return;
            }
            /* Quick Template Switching via [ and ] */
            if (IsKeyPressed(KEY_LEFT_BRACKET) && ui->template_count > 0) {
                int prev_t = (ui->active_template_idx - 1 + ui->template_count) % ui->template_count;
                request_guarded_action(session, ui, 3, prev_t);
                ui->has_focus = false;
                return;
            }
            if (IsKeyPressed(KEY_RIGHT_BRACKET) && ui->template_count > 0) {
                int next_t = (ui->active_template_idx + 1) % ui->template_count;
                request_guarded_action(session, ui, 3, next_t);
                ui->has_focus = false;
                return;
            }
        }
    }

    /* F1: Atalhos de Teclado */
    if (IsKeyPressed(KEY_F1)) {
        ui->modal_shortcuts_open = !ui->modal_shortcuts_open;
        ui->active_menu          = -1;
    }

    /* F5 / Shift+F5: Compilar & Iniciar / Parar */
    if (IsKeyPressed(KEY_F5)) {
        if (shift && is_running_session) {
            gdb_stop_session(session);
            session->state = STATE_IDLE;
            diagnostics_clear(&session->diagnostic);
        } else if (!shift) {
            trigger_compile_and_start(session, ui);
        }
    }

    /* F9: Continuar até Breakpoint (Continue) */
    if (IsKeyPressed(KEY_F9) && is_running_session) {
        ui->blob_idle_timer = 0.0f;
        session->auto_play  = false;
        gdb_request_continue(session);
    }

    /* F10 / Shift+F10: Próximo Passo (Step) / Voltar Passo (Time-Travel Step-Back) */
    if (IsKeyPressed(KEY_F10) && is_running_session) {
        ui->blob_idle_timer = 0.0f;
        session->auto_play  = false;
        if (shift) {
            if (session_step_back(session)) {
                audio_play_sfx(SFX_STEP_TICK, &ui->config, -0.10f);
            }
        } else {
            int prev_cur = session->history_cursor;
            session_step_forward_or_gdb(session);
            if (session->history_cursor != prev_cur) {
                audio_play_sfx(SFX_STEP_TICK, &ui->config, 0.05f);
            }
        }
    }

    /* F8: Auto-Play / Pausar */
    if (IsKeyPressed(KEY_F8) && is_running_session) {
        ui->blob_idle_timer = 0.0f;
        session->auto_play  = !session->auto_play;
        if (session->auto_play && session->state == STATE_PAUSED) {
            session->state            = STATE_RUNNING;
            session->step_accumulator = step_interval_sec;
        } else if (!session->auto_play && session->state == STATE_RUNNING) {
            session->state = STATE_PAUSED;
        }
    }

    /* F11: Tela Cheia */
    if (IsKeyPressed(KEY_F11)) {
        ToggleFullscreen();
    }

    if (!ctrl) return;

    /* Ctrl+, : Configurações */
    if (IsKeyPressed(KEY_COMMA)) {
        ui->modal_settings_open = !ui->modal_settings_open;
        ui->active_menu         = -1;
        if (!ui->modal_settings_open) config_save(&ui->config);
    }

    /* Ctrl+N : Novo Scratchpad */
    if (IsKeyPressed(KEY_N)) {
        ui->active_menu = -1;
        request_guarded_action(session, ui, 1, 0);
    }

    /* Ctrl+O : Abrir Arquivo .c */
    if (IsKeyPressed(KEY_O)) {
        ui->active_menu = -1;
        request_guarded_action(session, ui, 2, 0);
    }

    /* Ctrl+S / Ctrl+Shift+S : Salvar / Salvar Como */
    if (IsKeyPressed(KEY_S)) {
        ui->active_menu = -1;
        if (!shift && ui->current_file_path[0] != '\0') {
            save_editor_to_path(session, ui, ui->current_file_path);
        } else {
            open_path_dialog_or_fallback(
                session, ui, 1,
                ui->current_file_path[0] ? ui->current_file_path : "algoritmo.c"
            );
        }
    }

    /* Ctrl+E : Exportar Canvas como PNG */
    if (IsKeyPressed(KEY_E)) {
        ui->active_menu = -1;
        open_path_dialog_or_fallback(session, ui, 3, "avora_canvas.png");
    }

    /* Ctrl+R : Resetar Template Atual */
    if (IsKeyPressed(KEY_R)) {
        ui->active_menu = -1;
        request_guarded_action(session, ui, 4, 0);
    }

    /* Ctrl+Q : Sair */
    if (IsKeyPressed(KEY_Q)) {
        ui->active_menu = -1;
        request_guarded_action(session, ui, 5, 0);
    }

    /* Ctrl+Shift+P : Modo Apresentação / Palco */
    if (shift && IsKeyPressed(KEY_P)) {
        ui->config.presentation_mode = !ui->config.presentation_mode;
        if (ui->config.presentation_mode) {
            ui->has_focus         = false;
            ui->terminal_open     = false;
            ui->camera_auto_focus = true;
        }
        ui_workspace_reload_editor_font(ui);
        config_save(&ui->config);
        show_toast(ui, ui->config.presentation_mode
                       ? "Modo Apresentação ATIVO (Use ← → ↑ ↓ ou Passador Bluetooth)"
                       : "Modo Apresentação DESATIVADO");
    }

    /* Ctrl+` ou Ctrl+J : Mostrar/Ocultar Terminal Retrátil */
    if (IsKeyPressed(KEY_GRAVE) || IsKeyPressed(KEY_J)) {
        ui->terminal_open = !ui->terminal_open;
    }

    /* Ctrl++ / Ctrl+- / Ctrl+0 : Zoom da Fonte do Editor */
    if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
        if (ui->config.editor_font_size < 24) {
            ui->config.editor_font_size++;
            ui_workspace_reload_editor_font(ui);
            config_save(&ui->config);
        }
    } else if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
        if (ui->config.editor_font_size > 12) {
            ui->config.editor_font_size--;
            ui_workspace_reload_editor_font(ui);
            config_save(&ui->config);
        }
    } else if (IsKeyPressed(KEY_ZERO) || IsKeyPressed(KEY_KP_0)) {
        ui->config.editor_font_size = 16;
        ui_workspace_reload_editor_font(ui);
        config_save(&ui->config);
    }
}

/* ============================================================================
 * 9. TOP MENU BAR & DROPDOWN MENUS (ARQUIVO, EDITAR, EXECUTAR, JANELA, AJUDA)
 * ============================================================================ */

static bool draw_menu_dropdown_item(
    Rectangle           rect,
    const char         *label,
    const char         *shortcut,
    bool                checked,
    bool                has_check_slot,
    Font                font,
    const ThemePalette *pal
)
{
    Vector2 mouse = GetMousePosition();
    bool hov = CheckCollisionPointRec(mouse, rect);
    if (hov) {
        DrawRectangleRounded(rect, 0.18f, 4, pal->bg_hover);
    }

    float text_x = rect.x + 10.0f;
    if (has_check_slot) {
        if (checked) {
            DrawTextEx(font, "✓", (Vector2){ floorf(rect.x + 8.0f + 0.5f), floorf(rect.y + 4.0f + 0.5f) },
                       13.0f, 1.0f, pal->accent_primary);
        }
        text_x = rect.x + 26.0f;
    }

    DrawTextEx(
        font,
        label,
        (Vector2){ floorf(text_x + 0.5f), floorf(rect.y + 5.0f + 0.5f) },
        13.0f,
        1.0f,
        hov ? pal->text_primary : pal->text_primary
    );

    if (shortcut && shortcut[0] != '\0') {
        Vector2 ssz = MeasureTextEx(font, shortcut, 12.0f, 1.0f);
        DrawTextEx(
            font,
            shortcut,
            (Vector2){ floorf(rect.x + rect.width - ssz.x - 10.0f + 0.5f), floorf(rect.y + 5.5f + 0.5f) },
            12.0f,
            1.0f,
            pal->text_secondary
        );
    }

    return (hov && IsMouseButtonReleased(MOUSE_LEFT_BUTTON));
}

static void draw_menu_separator(float x, float *inout_y, float w, const ThemePalette *pal)
{
    *inout_y += 3.0f;
    DrawLineEx((Vector2){ x + 8.0f, *inout_y }, (Vector2){ x + w - 8.0f, *inout_y }, 1.0f, pal->border);
    *inout_y += 4.0f;
}

static void draw_top_menu_bar_and_dropdowns(
    DebuggerSession *session,
    WorkspaceUI     *ui,
    float            sw,
    float            step_interval_sec
)
{
    const ThemePalette *pal = &ui->theme;
    Font font_sm = ui->font_sm;
    Vector2 mouse = GetMousePosition();

    Rectangle bar_rect = { 0.0f, 0.0f, sw, MENU_BAR_H };
    DrawRectangleRec(bar_rect, pal->bg_panel);
    DrawLineEx((Vector2){ 0.0f, MENU_BAR_H }, (Vector2){ sw, MENU_BAR_H }, 1.0f, pal->border);

    const char *menu_titles[5] = {
        tr(STR_MENU_FILE),
        tr(STR_MENU_EDIT),
        tr(STR_MENU_RUN),
        tr(STR_MENU_WINDOW),
        tr(STR_MENU_HELP)
    };
    Rectangle tab_rects[5];
    float cur_x = 8.0f;

    bool modal_blocking = ui->modal_settings_open ||
                          ui->modal_shortcuts_open ||
                          ui->modal_about_open ||
                          ui->modal_sysdiag_open ||
                          ui->modal_path_input_open ||
                          ui->modal_unsaved_confirm_open;

    for (int i = 0; i < 5; i++) {
        Vector2 tsz = MeasureTextEx(font_sm, menu_titles[i], 13.0f, 1.0f);
        float tw = floorf(tsz.x + 22.0f);
        tab_rects[i] = (Rectangle){ cur_x, 2.0f, tw, MENU_BAR_H - 4.0f };
        cur_x += tw + 4.0f;

        bool hov = !modal_blocking && CheckCollisionPointRec(mouse, tab_rects[i]);
        bool act = (ui->active_menu == i);

        if (act || hov) {
            DrawRectangleRounded(tab_rects[i], 0.25f, 4, act ? pal->bg_elevated : pal->bg_hover);
            if (act) {
                DrawRectangleRoundedLinesEx(tab_rects[i], 0.25f, 4, 1.0f, pal->accent_secondary);
            }
        }
        DrawTextEx(
            font_sm,
            menu_titles[i],
            (Vector2){ floorf(tab_rects[i].x + 11.0f + 0.5f), floorf(tab_rects[i].y + 4.5f + 0.5f) },
            13.0f,
            1.0f,
            (act || hov) ? pal->text_primary : pal->text_secondary
        );

        if (hov) {
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                ui->active_menu         = (ui->active_menu == i) ? -1 : i;
                ui->show_recent_submenu = false;
                ui->dropdown_edit_mode  = false;
            } else if (ui->active_menu >= 0 && ui->active_menu != i) {
                ui->active_menu         = i;
                ui->show_recent_submenu = false;
            }
        }
    }

    /* Right side of Menu Bar: Quick Settings Button [ ⚙ Configurações ], Mute Toggle & Context Pill */
    const char *cfg_lbl = tr(STR_BTN_SETTINGS);
    Vector2 cfg_sz = MeasureTextEx(font_sm, cfg_lbl, 13.0f, 1.0f);
    float cfg_w = floorf(cfg_sz.x + 20.0f);
    Rectangle cfg_btn = { sw - cfg_w - 8.0f, 2.0f, cfg_w, MENU_BAR_H - 4.0f };
    bool hov_cfg = !modal_blocking && CheckCollisionPointRec(mouse, cfg_btn);
    DrawRectangleRounded(cfg_btn, 0.25f, 4, (hov_cfg || ui->modal_settings_open) ? pal->bg_hover : pal->bg_elevated);
    DrawRectangleRoundedLinesEx(cfg_btn, 0.25f, 4, 1.0f, hov_cfg ? pal->accent_secondary : pal->border);
    DrawTextEx(
        font_sm,
        cfg_lbl,
        (Vector2){ floorf(cfg_btn.x + 10.0f + 0.5f), floorf(cfg_btn.y + 4.5f + 0.5f) },
        13.0f,
        1.0f,
        pal->text_primary
    );
    if (hov_cfg && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        audio_play_sfx(SFX_UI_CLICK, &ui->config, 0.0f);
        ui->modal_settings_open = !ui->modal_settings_open;
        ui->active_menu         = -1;
    }

    /* Quick Mute Toggle Button */
    const char *mute_lbl = ui->config.audio_enabled ? "♪ SFX" : "✕ Mudo";
    Vector2 mute_sz = MeasureTextEx(font_sm, mute_lbl, 13.0f, 1.0f);
    float mute_w = floorf(mute_sz.x + 16.0f);
    Rectangle mute_btn = { cfg_btn.x - mute_w - 6.0f, 2.0f, mute_w, MENU_BAR_H - 4.0f };
    bool hov_mute = !modal_blocking && CheckCollisionPointRec(mouse, mute_btn);
    DrawRectangleRounded(mute_btn, 0.25f, 4, hov_mute ? pal->bg_hover : pal->bg_elevated);
    DrawRectangleRoundedLinesEx(mute_btn, 0.25f, 4, 1.0f,
                                ui->config.audio_enabled ? pal->accent_primary : pal->border);
    DrawTextEx(
        font_sm,
        mute_lbl,
        (Vector2){ floorf(mute_btn.x + 8.0f + 0.5f), floorf(mute_btn.y + 4.5f + 0.5f) },
        13.0f,
        1.0f,
        ui->config.audio_enabled ? pal->accent_primary : pal->text_secondary
    );
    if (hov_mute && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->config.audio_enabled = !ui->config.audio_enabled;
        config_save(&ui->config);
        if (ui->config.audio_enabled) {
            audio_play_sfx(SFX_UI_CLICK, &ui->config, 0.0f);
        }
    }

    /* Context Pill (Active File + Theme + Presentation Mode Badge) */
    char ctx_info[256];
    snprintf(
        ctx_info,
        sizeof(ctx_info),
        "%.96s%s  •  Tema: %.48s%s",
        ui->current_file_path[0] ? path_basename_ptr(ui->current_file_path) : "Scratchpad / Template",
        ui->is_dirty ? " ●" : "",
        pal->name,
        ui->config.presentation_mode ? "  •  [PROJETOR]" : ""
    );
    Vector2 ctx_sz = MeasureTextEx(font_sm, ctx_info, 13.0f, 1.0f);
    float ctx_x = mute_btn.x - ctx_sz.x - 14.0f;
    if (ctx_x > cur_x + 12.0f) {
        DrawTextEx(
            font_sm,
            ctx_info,
            (Vector2){ floorf(ctx_x + 0.5f), floorf(6.5f + 0.5f) },
            13.0f,
            1.0f,
            ui->is_dirty ? pal->accent_warning : pal->text_secondary
        );
    }

    if (ui->active_menu < 0 || modal_blocking) return;

    /* Render Active Dropdown Menu Box */
    float menu_x = tab_rects[ui->active_menu].x;
    float menu_y = MENU_BAR_H + 2.0f;
    float menu_w = fminf(380.0f, sw - menu_x - 8.0f);
    float item_h = 24.0f;

    int   item_rows = 0;
    int   sep_rows  = 0;
    if (ui->active_menu == 0)      { item_rows = 9;  sep_rows = 3; }
    else if (ui->active_menu == 1) { item_rows = 8;  sep_rows = 3; }
    else if (ui->active_menu == 2) { item_rows = 9;  sep_rows = 2; }
    else if (ui->active_menu == 3) { item_rows = 11; sep_rows = 3; }
    else if (ui->active_menu == 4) { item_rows = 5;  sep_rows = 1; }

    float menu_h = 8.0f + (float)item_rows * item_h + (float)sep_rows * 7.0f;
    Rectangle menu_box = { menu_x, menu_y, menu_w, menu_h };

    DrawRectangleRounded(
        (Rectangle){ menu_box.x + 4.0f, menu_box.y + 5.0f, menu_box.width, menu_box.height },
        0.05f, 6, (Color){ 0, 0, 0, pal->is_light ? 55 : 170 }
    );
    DrawRectangleRounded(menu_box, 0.05f, 6, pal->bg_panel);
    DrawRectangleRoundedLinesEx(menu_box, 0.05f, 6, 1.2f, pal->accent_secondary);

    float iy = menu_box.y + 4.0f;
    bool close_menu = false;
    Rectangle sub_box = { 0, 0, 0, 0 };

    if (ui->active_menu == 0) {
        /* 0: ARQUIVO */
        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_FILE_NEW), "Ctrl+N", false, false, font_sm, pal)) {
            request_guarded_action(session, ui, 1, 0);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_FILE_OPEN), "Ctrl+O", false, false, font_sm, pal)) {
            request_guarded_action(session, ui, 2, 0);
            close_menu = true;
        }
        iy += item_h;

        Rectangle rec_item = { menu_x + 4, iy, menu_w - 8, item_h };
        if (CheckCollisionPointRec(mouse, rec_item)) {
            ui->show_recent_submenu = true;
        }
        if (draw_menu_dropdown_item(rec_item, tr(STR_FILE_RECENT), "▸", false, false, font_sm, pal)) {
            ui->show_recent_submenu = !ui->show_recent_submenu;
        }
        float rec_sub_y = iy;
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_FILE_SAVE), "Ctrl+S", false, false, font_sm, pal)) {
            if (ui->current_file_path[0] != '\0') {
                save_editor_to_path(session, ui, ui->current_file_path);
            } else {
                open_path_dialog_or_fallback(session, ui, 1, "algoritmo.c");
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_FILE_SAVE_AS), "Ctrl+Shift+S", false, false, font_sm, pal)) {
            open_path_dialog_or_fallback(session, ui, 1, ui->current_file_path[0] ? ui->current_file_path : "algoritmo.c");
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_FILE_SAVE_TPL), "", false, false, font_sm, pal)) {
            char sug[600];
            snprintf(sug, sizeof(sug), "%s/meu_exemplo.c", ui->config.templates_dir);
            open_path_dialog_or_fallback(session, ui, 1, sug);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_FILE_EXPORT_PNG), "Ctrl+E", false, false, font_sm, pal)) {
            open_path_dialog_or_fallback(session, ui, 3, "avora_canvas.png");
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_FILE_OPEN_TPL_DIR), "", false, false, font_sm, pal)) {
            platform_open_folder_in_file_manager(ui->config.templates_dir);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_FILE_EXIT), "Ctrl+Q", false, false, font_sm, pal)) {
            request_guarded_action(session, ui, 5, 0);
            close_menu = true;
        }

        /* Recent Files Submenu */
        if (ui->show_recent_submenu) {
            int rcnt = (ui->config.recent_count > 0) ? ui->config.recent_count : 1;
            sub_box = (Rectangle){ menu_x + menu_w + 2.0f, rec_sub_y, 260.0f, 8.0f + (float)rcnt * item_h };
            DrawRectangleRounded((Rectangle){ sub_box.x + 3, sub_box.y + 4, sub_box.width, sub_box.height },
                                 0.05f, 6, (Color){ 0, 0, 0, pal->is_light ? 50 : 160 });
            DrawRectangleRounded(sub_box, 0.05f, 6, pal->bg_panel);
            DrawRectangleRoundedLinesEx(sub_box, 0.05f, 6, 1.2f, pal->accent_secondary);

            if (ui->config.recent_count == 0) {
                DrawTextEx(font_sm, "(Nenhum arquivo recente)", (Vector2){ sub_box.x + 12.0f, sub_box.y + 9.0f },
                           13.0f, 1.0f, pal->text_secondary);
            } else {
                for (int r = 0; r < ui->config.recent_count; r++) {
                    Rectangle r_rect = { sub_box.x + 4.0f, sub_box.y + 4.0f + (float)r * item_h, sub_box.width - 8.0f, item_h };
                    if (draw_menu_dropdown_item(r_rect, path_basename_ptr(ui->config.recent_files[r]), "", false, false, font_sm, pal)) {
                        request_guarded_action(session, ui, 6, r);
                        close_menu = true;
                    }
                }
            }
        }
    } else if (ui->active_menu == 1) {
        /* 1: EDITAR */
        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_EDIT_UNDO), "Ctrl+Z", false, false, font_sm, pal)) {
            editor_perform_undo(ui, session->code_buffer, MAX_CODE_SIZE);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_EDIT_REDO), "Ctrl+Y", false, false, font_sm, pal)) {
            editor_perform_redo(ui, session->code_buffer, MAX_CODE_SIZE);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_EDIT_CUT), "Ctrl+X", false, false, font_sm, pal)) {
            editor_copy_selection_to_clipboard(session->code_buffer, ui);
            if (editor_delete_selection(session->code_buffer, ui)) {
                editor_push_undo(ui, session->code_buffer);
                ui->is_dirty = true;
                ui_workspace_update_window_title(ui);
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_EDIT_COPY), "Ctrl+C", false, false, font_sm, pal)) {
            editor_copy_selection_to_clipboard(session->code_buffer, ui);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_EDIT_PASTE), "Ctrl+V", false, false, font_sm, pal)) {
            const char *clip = GetClipboardText();
            if (clip && clip[0]) {
                editor_insert_text(session->code_buffer, MAX_CODE_SIZE, ui, clip);
                editor_push_undo(ui, session->code_buffer);
                ui->is_dirty = true;
                ui_workspace_update_window_title(ui);
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_EDIT_SELECT_ALL), "Ctrl+A", false, false, font_sm, pal)) {
            ui->sel_anchor_pos = 0;
            ui->sel_active_pos = (int)strlen(session->code_buffer);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_EDIT_RESET_TPL), "Ctrl+R", false, false, font_sm, pal)) {
            request_guarded_action(session, ui, 4, 0);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_EDIT_PREFERENCES), "Ctrl+,", false, false, font_sm, pal)) {
            ui->modal_settings_open = true;
            close_menu = true;
        }
    } else if (ui->active_menu == 2) {
        /* 2: EXECUTAR */
        bool is_running = (session->gdb_pid > 0) &&
                          (session->state == STATE_PAUSED ||
                           session->state == STATE_RUNNING ||
                           session->state == STATE_STEPPING);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "▶ Compilar & Iniciar", "F5", false, false, font_sm, pal)) {
            trigger_compile_and_start(session, ui);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "⏮ Voltar Passo (Time-Travel)", "Shift+F10", false, false, font_sm, pal)) {
            if (is_running && session_step_back(session)) {
                audio_play_sfx(SFX_STEP_TICK, &ui->config, -0.10f);
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "⏭ Próximo Passo (Step)", "F10", false, false, font_sm, pal)) {
            if (is_running) {
                session->auto_play = false;
                session_step_forward_or_gdb(session);
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "▶ Continuar até Breakpoint", "F9", false, false, font_sm, pal)) {
            if (is_running) {
                session->auto_play = false;
                gdb_request_continue(session);
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h },
                                    session->auto_play ? "⏸ Pausar Auto-Play" : "⏯ Iniciar Auto-Play", "F8", false, false, font_sm, pal)) {
            if (is_running) {
                session->auto_play = !session->auto_play;
                if (session->auto_play && session->state == STATE_PAUSED) {
                    session->state            = STATE_RUNNING;
                    session->step_accumulator = step_interval_sec;
                } else if (!session->auto_play && session->state == STATE_RUNNING) {
                    session->state = STATE_PAUSED;
                }
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "■ Parar Depuração", "Shift+F5", false, false, font_sm, pal)) {
            gdb_stop_session(session);
            session->state = STATE_IDLE;
            diagnostics_clear(&session->diagnostic);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "✕ Limpar Todos os Breakpoints", "", false, false, font_sm, pal)) {
            session_clear_breakpoints(session);
            audio_play_sfx(SFX_UI_CLICK, &ui->config, 0.0f);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "AddressSanitizer (ASan)", "", ui->config.asan_enabled, true, font_sm, pal)) {
            ui->config.asan_enabled = !ui->config.asan_enabled;
            session->user_asan_pref = ui->config.asan_enabled;
            config_save(&ui->config);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Iniciar Pausado na main()", "", ui->config.start_paused_on_main, true, font_sm, pal)) {
            ui->config.start_paused_on_main = !ui->config.start_paused_on_main;
            config_save(&ui->config);
            close_menu = true;
        }
    } else if (ui->active_menu == 3) {
        /* 3: JANELA */
        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Mostrar HUD (Pilha & Variáveis)", "", ui->config.show_hud, true, font_sm, pal)) {
            ui->config.show_hud = !ui->config.show_hud;
            config_save(&ui->config);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Mostrar Terminal Retrátil", "Ctrl+`", ui->terminal_open, true, font_sm, pal)) {
            ui->terminal_open = !ui->terminal_open;
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Mostrar Mascote Blob (?)", "", ui->config.show_blob, true, font_sm, pal)) {
            ui->config.show_blob = !ui->config.show_blob;
            config_save(&ui->config);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Mostrar Endereços (0x...)", "", ui->config.show_memory_addresses, true, font_sm, pal)) {
            ui->config.show_memory_addresses = !ui->config.show_memory_addresses;
            config_save(&ui->config);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Auto-Foco da Câmera", "", ui->camera_auto_focus, true, font_sm, pal)) {
            ui->camera_auto_focus = !ui->camera_auto_focus;
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Modo Apresentação (Projetor)", "Ctrl+Shift+P", ui->config.presentation_mode, true, font_sm, pal)) {
            ui->config.presentation_mode = !ui->config.presentation_mode;
            ui_workspace_reload_editor_font(ui);
            config_save(&ui->config);
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Tela Cheia", "F11", IsWindowFullscreen(), true, font_sm, pal)) {
            ToggleFullscreen();
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Aumentar Fonte do Editor", "Ctrl++", false, true, font_sm, pal)) {
            if (ui->config.editor_font_size < 24) {
                ui->config.editor_font_size++;
                ui_workspace_reload_editor_font(ui);
                config_save(&ui->config);
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Diminuir Fonte do Editor", "Ctrl+-", false, true, font_sm, pal)) {
            if (ui->config.editor_font_size > 12) {
                ui->config.editor_font_size--;
                ui_workspace_reload_editor_font(ui);
                config_save(&ui->config);
            }
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Resetar Fonte (16px)", "Ctrl+0", false, true, font_sm, pal)) {
            ui->config.editor_font_size = 16;
            ui_workspace_reload_editor_font(ui);
            config_save(&ui->config);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, "Resetar Layout & Câmera", "", false, true, font_sm, pal)) {
            ui->split_vertical_ratio = 0.42f;
            ui->camera_auto_focus    = true;
            float sh = (float)GetScreenHeight();
            Rectangle c_rect = { sw * 0.42f, MENU_BAR_H, sw * 0.58f, sh - MENU_BAR_H - FOOTER_BAR_H };
            layout_center_camera(ui, &session->snapshot, c_rect);
            close_menu = true;
        }
    } else if (ui->active_menu == 4) {
        /* 4: AJUDA */
        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_HELP_SHORTCUTS), "F1", false, false, font_sm, pal)) {
            ui->modal_shortcuts_open = true;
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_HELP_BLOB_GUIDE), "", false, false, font_sm, pal)) {
            ui->config.show_blob   = true;
            ui->active_blob_topic  = BLOB_HINT_MASCOT;
            ui->blob_anim_progress = 0.08f;
            ui->blob_idle_timer    = 0.0f;
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_HELP_SYSDIAG), "", false, false, font_sm, pal)) {
            ui->modal_sysdiag_open = true;
            close_menu = true;
        }
        iy += item_h;

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_HELP_OPEN_CFG_DIR), "", false, false, font_sm, pal)) {
            char cfg_dir[512];
            config_get_dir_path(cfg_dir, sizeof(cfg_dir));
            platform_open_folder_in_file_manager(cfg_dir);
            close_menu = true;
        }
        iy += item_h;

        draw_menu_separator(menu_x, &iy, menu_w, pal);

        if (draw_menu_dropdown_item((Rectangle){ menu_x + 4, iy, menu_w - 8, item_h }, tr(STR_HELP_ABOUT), "", false, false, font_sm, pal)) {
            ui->modal_about_open = true;
            close_menu = true;
        }
    }

    if (close_menu) {
        ui->active_menu         = -1;
        ui->show_recent_submenu = false;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        bool in_bar = (mouse.y <= MENU_BAR_H);
        bool in_box = CheckCollisionPointRec(mouse, menu_box);
        bool in_sub = ui->show_recent_submenu && CheckCollisionPointRec(mouse, sub_box);
        if (!in_bar && !in_box && !in_sub) {
            ui->active_menu         = -1;
            ui->show_recent_submenu = false;
        }
    }
}

/* ============================================================================
 * 10. SETTINGS MODAL (Ctrl+,) WITH 4 TABS & MONKEYTYPE THEME SWATCHES
 * ============================================================================ */

static bool draw_checkbox_row(
    Rectangle           bounds,
    const char         *label,
    bool               *inout_val,
    Font                font,
    const ThemePalette *pal
)
{
    Vector2 mouse = GetMousePosition();
    bool hov = CheckCollisionPointRec(mouse, bounds);
    Rectangle box = { bounds.x, floorf(bounds.y + (bounds.height - 18.0f) * 0.5f), 18.0f, 18.0f };

    DrawRectangleRounded(box, 0.25f, 4, *inout_val ? pal->accent_primary : pal->bg_elevated);
    DrawRectangleRoundedLinesEx(box, 0.25f, 4, 1.2f, (*inout_val || hov) ? pal->accent_secondary : pal->border);
    if (*inout_val) {
        DrawTextEx(font, "✓", (Vector2){ floorf(box.x + 3.5f + 0.5f), floorf(box.y + 1.5f + 0.5f) },
                   14.0f, 1.0f, pal->accent_primary_fg);
    }

    DrawTextEx(
        font,
        label,
        (Vector2){ floorf(bounds.x + 28.0f + 0.5f), floorf(bounds.y + (bounds.height - 14.0f) * 0.5f + 0.5f) },
        14.0f,
        1.0f,
        hov ? pal->accent_secondary : pal->text_primary
    );

    if (hov && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        *inout_val = !(*inout_val);
        return true;
    }
    return false;
}

static bool draw_segmented_option_btn(
    Rectangle           bounds,
    const char         *label,
    bool                is_active,
    Font                font,
    const ThemePalette *pal
)
{
    Vector2 mouse = GetMousePosition();
    bool hov = CheckCollisionPointRec(mouse, bounds);
    Color bg = is_active ? pal->accent_primary : (hov ? pal->bg_hover : pal->bg_elevated);
    Color fg = is_active ? pal->accent_primary_fg : pal->text_primary;
    Color bd = is_active ? pal->accent_primary : (hov ? pal->accent_secondary : pal->border);

    DrawRectangleRounded(bounds, 0.25f, 6, bg);
    DrawRectangleRoundedLinesEx(bounds, 0.25f, 6, 1.2f, bd);
    Vector2 tsz = MeasureTextEx(font, label, 13.0f, 1.0f);
    BeginScissorMode((int)bounds.x + 2, (int)bounds.y, (int)bounds.width - 4, (int)bounds.height);
    DrawTextEx(
        font,
        label,
        (Vector2){ floorf(bounds.x + (bounds.width - tsz.x) * 0.5f), floorf(bounds.y + (bounds.height - tsz.y) * 0.5f) },
        13.0f,
        1.0f,
        fg
    );
    EndScissorMode();
    return (hov && IsMouseButtonReleased(MOUSE_LEFT_BUTTON));
}

static void draw_settings_modal(DebuggerSession *session, WorkspaceUI *ui, float sw, float sh)
{
    if (!ui->modal_settings_open) return;

    const ThemePalette *pal = &ui->theme;
    AppConfig          *cfg = &ui->config;
    Font font_ui = ui->font_ui;
    Font font_sm = ui->font_sm;
    Vector2 mouse = GetMousePosition();

    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, pal->is_light ? 0.35f : 0.60f));

    float mw = fminf(900.0f, sw - 32.0f);
    float mh = fminf(560.0f, sh - 32.0f);
    Rectangle modal = { floorf((sw - mw) * 0.5f), floorf((sh - mh) * 0.5f), mw, mh };

    DrawRectangleRounded(
        (Rectangle){ modal.x + 5.0f, modal.y + 6.0f, modal.width, modal.height },
        0.04f, 8, (Color){ 0, 0, 0, pal->is_light ? 65 : 185 }
    );
    DrawRectangleRounded(modal, 0.04f, 8, pal->bg_panel);
    DrawRectangleRoundedLinesEx(modal, 0.04f, 8, 1.4f, pal->accent_secondary);

    /* Modal Header */
    Rectangle hdr = { modal.x, modal.y, modal.width, 40.0f };
    DrawRectangleRounded((Rectangle){ hdr.x, hdr.y, hdr.width, hdr.height }, 0.04f, 8, pal->bg_elevated);
    DrawLineEx((Vector2){ modal.x, modal.y + 40.0f }, (Vector2){ modal.x + modal.width, modal.y + 40.0f }, 1.0f, pal->border);
    DrawTextEx(font_ui, tr(STR_SETTINGS_TITLE),
               (Vector2){ floorf(modal.x + 16.0f + 0.5f), floorf(modal.y + 12.0f + 0.5f) },
               15.0f, 1.0f, pal->text_primary);

    Rectangle btn_x = { modal.x + modal.width - 36.0f, modal.y + 7.0f, 26.0f, 26.0f };
    if (GuiButton(btn_x, "✕")) {
        ui->modal_settings_open = false;
        config_save(cfg);
        return;
    }

    /* Left Sidebar Tabs (4 Categories) */
    float side_w = 196.0f;
    float body_y = modal.y + 40.0f;
    float body_h = modal.height - 86.0f;
    Rectangle side_rect = { modal.x, body_y, side_w, body_h };
    DrawRectangleRec(side_rect, pal->bg_gutter);
    DrawLineEx((Vector2){ modal.x + side_w, body_y }, (Vector2){ modal.x + side_w, body_y + body_h }, 1.0f, pal->border);

    const char *tab_names[4] = {
        tr(STR_SETTINGS_TAB_APPEARANCE),
        tr(STR_SETTINGS_TAB_EDITOR),
        tr(STR_SETTINGS_TAB_DEBUGGER),
        tr(STR_SETTINGS_TAB_TEMPLATES)
    };
    for (int t = 0; t < 4; t++) {
        Rectangle tr = { modal.x + 8.0f, body_y + 12.0f + (float)t * 36.0f, side_w - 16.0f, 30.0f };
        bool hov = CheckCollisionPointRec(mouse, tr);
        bool act = (ui->settings_active_tab == t);
        if (act || hov) {
            DrawRectangleRounded(tr, 0.22f, 6, act ? pal->bg_elevated : pal->bg_hover);
            if (act) {
                DrawRectangle((int)tr.x, (int)tr.y + 5, 3, (int)tr.height - 10, pal->accent_primary);
            }
        }
        DrawTextEx(
            font_sm,
            tab_names[t],
            (Vector2){ floorf(tr.x + 12.0f + 0.5f), floorf(tr.y + 8.0f + 0.5f) },
            13.0f,
            1.0f,
            act ? pal->accent_secondary : pal->text_primary
        );
        if (hov && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
            ui->settings_active_tab = t;
        }
    }

    /* Content Area */
    float cx = modal.x + side_w + 22.0f;
    float cy = body_y + 14.0f;
    float cw = modal.width - side_w - 44.0f;
    bool changed = false;

    if (ui->settings_active_tab == 0) {
        /* TAB 0: APARÊNCIA, IDIOMA & TEMAS (MONKEYTYPE PALETTES) */
        DrawTextEx(font_ui, tr(STR_SETTINGS_LANG_HDR), (Vector2){ cx, cy + 4.0f }, 14.0f, 1.0f, pal->accent_secondary);
        static const char *lang_names[LANG_COUNT] = { "Português (BR)", "English (US)", "Español" };
        float lbtn_w = floorf((cw - 145.0f - 12.0f) / 3.0f);
        for (int l = 0; l < LANG_COUNT; l++) {
            Rectangle lr = { cx + 145.0f + (float)l * (lbtn_w + 6.0f), cy, lbtn_w, 24.0f };
            if (draw_segmented_option_btn(lr, lang_names[l], cfg->language == l, font_sm, pal)) {
                cfg->language = l;
                i18n_set_language((AppLanguage)l);
                audio_play_sfx(SFX_UI_CLICK, cfg, 0.0f);
                changed = true;
            }
        }
        cy += 30.0f;

        DrawTextEx(font_ui, "Tema de Cores (Inspirado no Monkeytype)", (Vector2){ cx, cy }, 14.0f, 1.0f, pal->accent_secondary);
        cy += 20.0f;

        float card_w = floorf((cw - 16.0f) / 3.0f);
        float card_h = 34.0f;
        for (int th = 0; th < THEME_COUNT; th++) {
            int row = th / 3;
            int col = th % 3;
            Rectangle tc = {
                cx + (float)col * (card_w + 8.0f),
                cy + (float)row * (card_h + 6.0f),
                card_w,
                card_h
            };
            ThemePalette tp = theme_get_palette(th);
            bool is_sel = (cfg->theme_id == th);
            bool hov    = CheckCollisionPointRec(mouse, tc);

            DrawRectangleRounded(tc, 0.22f, 6, tp.bg_panel);
            DrawRectangleRoundedLinesEx(tc, 0.22f, 6, is_sel ? 2.2f : 1.2f,
                                        is_sel ? tp.accent_primary : (hov ? tp.accent_secondary : tp.border));

            /* Color swatches on the right of each theme card */
            DrawCircle((int)(tc.x + tc.width - 34.0f), (int)(tc.y + tc.height * 0.5f), 5.0f, tp.accent_primary);
            DrawCircle((int)(tc.x + tc.width - 21.0f), (int)(tc.y + tc.height * 0.5f), 5.0f, tp.accent_secondary);
            DrawCircle((int)(tc.x + tc.width - 8.0f),  (int)(tc.y + tc.height * 0.5f), 5.0f, tp.text_primary);

            DrawTextEx(
                font_sm,
                tp.name,
                (Vector2){ floorf(tc.x + 8.0f + 0.5f), floorf(tc.y + 10.0f + 0.5f) },
                13.0f,
                1.0f,
                tp.text_primary
            );

            if (hov && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
                cfg->theme_id = th;
                ui->theme     = theme_get_palette(th);
                theme_apply_to_raygui(&ui->theme);
                pal     = &ui->theme;
                changed = true;
            }
        }
        cy += (card_h + 6.0f) * 3.0f + 4.0f;

        DrawTextEx(font_ui, "Estilo do Fundo do Canvas 2D", (Vector2){ cx, cy }, 14.0f, 1.0f, pal->accent_secondary);
        cy += 19.0f;

        static const char *bg_styles[3] = { "Dot Grid", "Linhas", "Liso" };
        float seg_w = floorf((cw - 16.0f) / 3.0f);
        for (int b = 0; b < 3; b++) {
            Rectangle sr = { cx + (float)b * (seg_w + 8.0f), cy, seg_w, 26.0f };
            if (draw_segmented_option_btn(sr, bg_styles[b], cfg->canvas_bg_style == b, font_sm, pal)) {
                cfg->canvas_bg_style = b;
                changed = true;
            }
        }
        cy += 34.0f;

        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 21.0f }, "Exibir endereços de memória (0x...) nos nós", &cfg->show_memory_addresses, font_sm, pal)) changed = true;
        cy += 22.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 21.0f }, "Exibir HUD Flutuante (Call Stack & Variáveis)", &cfg->show_hud, font_sm, pal)) changed = true;
        cy += 22.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 21.0f }, "Exibir Mascote Blob (?) no canto inferior direito", &cfg->show_blob, font_sm, pal)) changed = true;
        cy += 22.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 21.0f }, "Reduzir animações (transições instantâneas)", &cfg->reduce_animations, font_sm, pal)) changed = true;
        cy += 22.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 21.0f }, "Modo Apresentação / Palco (setas e nós grandes)", &cfg->presentation_mode, font_sm, pal)) {
            ui_workspace_reload_editor_font(ui);
            changed = true;
        }
        cy += 23.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, 255.0f, 21.0f }, "Áudio Procedural (SFX)", &cfg->audio_enabled, font_sm, pal)) {
            changed = true;
            if (cfg->audio_enabled) audio_play_sfx(SFX_UI_CLICK, cfg, 0.0f);
        }
        char vol_lbl[32];
        snprintf(vol_lbl, sizeof(vol_lbl), "Vol: %d%%", (int)roundf(cfg->audio_volume * 100.0f));
        float prev_vol = cfg->audio_volume;
        GuiSliderBar((Rectangle){ cx + 245.0f, cy + 3.0f, 150.0f, 16.0f }, NULL, vol_lbl, &cfg->audio_volume, 0.0f, 1.0f);
        if (fabsf(cfg->audio_volume - prev_vol) > 0.005f) {
            changed = true;
        }
    } else if (ui->settings_active_tab == 1) {
        /* TAB 1: EDITOR DE CÓDIGO */
        DrawTextEx(font_ui, "Tipografia & Indentação do Editor C11", (Vector2){ floorf(cx), floorf(cy) }, 15.0f, 1.0f, pal->accent_secondary);
        cy += 28.0f;

        const char *fsz_title = "Fonte do Editor (Ctrl+ / -):";
        DrawTextEx(font_sm, fsz_title, (Vector2){ floorf(cx), floorf(cy + 7.0f) }, 13.0f, 1.0f, pal->text_primary);
        float font_ctrl_total_w = 236.0f;
        float ctrl_x = floorf(cx + cw - font_ctrl_total_w);
        if (GuiButton((Rectangle){ ctrl_x, cy, 32.0f, 28.0f }, "-")) {
            if (cfg->editor_font_size > 12) {
                cfg->editor_font_size--;
                ui_workspace_reload_editor_font(ui);
                changed = true;
            }
        }
        char fsz_lbl[32];
        snprintf(fsz_lbl, sizeof(fsz_lbl), "%d px", cfg->editor_font_size);
        Vector2 fsz_m = MeasureTextEx(font_ui, fsz_lbl, 15.0f, 1.0f);
        DrawTextEx(font_ui, fsz_lbl, (Vector2){ floorf(ctrl_x + 36.0f + (56.0f - fsz_m.x) * 0.5f), floorf(cy + 6.0f) }, 15.0f, 1.0f, pal->accent_primary);
        if (GuiButton((Rectangle){ ctrl_x + 96.0f, cy, 32.0f, 28.0f }, "+")) {
            if (cfg->editor_font_size < 24) {
                cfg->editor_font_size++;
                ui_workspace_reload_editor_font(ui);
                changed = true;
            }
        }
        if (GuiButton((Rectangle){ ctrl_x + 136.0f, cy, 100.0f, 28.0f }, "Padrão (16)")) {
            cfg->editor_font_size = 16;
            ui_workspace_reload_editor_font(ui);
            changed = true;
        }
        cy += 42.0f;

        DrawTextEx(font_sm, "Indentação (Tecla Tab):", (Vector2){ floorf(cx), floorf(cy + 7.0f) }, 13.0f, 1.0f, pal->text_primary);
        if (draw_segmented_option_btn((Rectangle){ ctrl_x, cy, 114.0f, 28.0f }, "2 Espaços", cfg->tab_size == 2, font_sm, pal)) {
            cfg->tab_size = 2;
            changed = true;
        }
        if (draw_segmented_option_btn((Rectangle){ ctrl_x + 122.0f, cy, 114.0f, 28.0f }, "4 Espaços", cfg->tab_size == 4, font_sm, pal)) {
            cfg->tab_size = 4;
            changed = true;
        }
        cy += 46.0f;

        DrawTextEx(font_ui, "Comportamento & Sessão", (Vector2){ floorf(cx), floorf(cy) }, 15.0f, 1.0f, pal->accent_secondary);
        cy += 26.0f;

        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Exibir números de linha na margem (Gutter)", &cfg->show_line_numbers, font_sm, pal)) changed = true;
        cy += 30.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Destacar linha atual do cursor", &cfg->highlight_current_line, font_sm, pal)) changed = true;
        cy += 30.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Auto-indentação inteligente (Enter e '{')", &cfg->auto_indent, font_sm, pal)) changed = true;
        cy += 30.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Restaurar última sessão ao iniciar", &cfg->restore_last_session, font_sm, pal)) changed = true;
        cy += 30.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Confirmar antes de descartar alterações não salvas", &cfg->confirm_unsaved_changes, font_sm, pal)) changed = true;
    } else if (ui->settings_active_tab == 2) {
        /* TAB 2: DEPURADOR (GCC / GDB) */
        DrawTextEx(font_ui, "Compilador GCC & Sanitizer de Memória", (Vector2){ floorf(cx), floorf(cy) }, 15.0f, 1.0f, pal->accent_secondary);
        cy += 26.0f;

        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f },
                              "Ativar AddressSanitizer (ASan: Use-After-Free & OOB)",
                              &cfg->asan_enabled, font_sm, pal)) {
            session->user_asan_pref = cfg->asan_enabled;
            changed = true;
        }
        cy += 38.0f;

        DrawTextEx(font_ui, "Timeout do Watchdog POSIX (Loop Infinito)", (Vector2){ floorf(cx), floorf(cy) }, 15.0f, 1.0f, pal->accent_secondary);
        cy += 24.0f;

        static const float wd_vals[4] = { 1.5f, 3.0f, 5.0f, 10.0f };
        static const char *wd_lbls[4] = { "1.5s (Padrão)", "3.0s", "5.0s", "10.0s" };
        float w_btn = floorf((cw - 24.0f) / 4.0f);
        for (int w = 0; w < 4; w++) {
            bool is_cur = (fabsf(cfg->watchdog_timeout_sec - wd_vals[w]) < 0.2f);
            if (draw_segmented_option_btn((Rectangle){ cx + (float)w * (w_btn + 8.0f), cy, w_btn, 28.0f },
                                          wd_lbls[w], is_cur, font_sm, pal)) {
                cfg->watchdog_timeout_sec     = wd_vals[w];
                session->watchdog_timeout_sec = wd_vals[w];
                changed = true;
            }
        }
        cy += 42.0f;

        DrawTextEx(font_ui, "Execução & Velocidade Padrão do Auto-Play", (Vector2){ floorf(cx), floorf(cy) }, 15.0f, 1.0f, pal->accent_secondary);
        cy += 26.0f;

        float def_spd = 0.05f * powf(2000.0f, cfg->default_speed_t);
        char spd_lbl[64];
        snprintf(spd_lbl, sizeof(spd_lbl), "Vel. inicial: %.2fx", def_spd);
        DrawTextEx(font_sm, spd_lbl, (Vector2){ floorf(cx), floorf(cy + 5.0f) }, 13.0f, 1.0f, pal->text_primary);

        float reset_w  = 92.0f;
        float slider_x = cx + 175.0f;
        float slider_w = fmaxf(120.0f, cw - 175.0f - reset_w - 14.0f);
        float prev_t   = cfg->default_speed_t;
        GuiSliderBar((Rectangle){ slider_x, cy + 4.0f, slider_w, 18.0f }, NULL, NULL, &cfg->default_speed_t, 0.0f, 1.0f);
        if (fabsf(cfg->default_speed_t - prev_t) > 0.0005f) {
            session->speed_slider_t = cfg->default_speed_t;
            changed = true;
        }
        if (GuiButton((Rectangle){ cx + cw - reset_w, cy - 1.0f, reset_w, 26.0f }, "Reset 1.0x")) {
            cfg->default_speed_t    = 0.391f;
            session->speed_slider_t = 0.391f;
            changed = true;
        }
        cy += 40.0f;

        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Iniciar depuração pausado na main()", &cfg->start_paused_on_main, font_sm, pal)) changed = true;
        cy += 28.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Abrir Terminal ao imprimir em stdout (printf)", &cfg->auto_open_terminal_on_stdout, font_sm, pal)) changed = true;
        cy += 28.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Ativar Entrada (stdin) na aba Terminal", &cfg->enable_stdin, font_sm, pal)) {
            session->enable_stdin = cfg->enable_stdin;
            changed = true;
        }
    } else if (ui->settings_active_tab == 3) {
        /* TAB 3: TEMPLATES PERSONALIZADOS (PASTA LOCAL) & JANELA */
        DrawTextEx(font_ui, "Templates Personalizados por Pasta (Biblioteca Local .c)", (Vector2){ cx, cy }, 15.0f, 1.0f, pal->accent_secondary);
        cy += 24.0f;

        static const char *tpl_modes[3] = {
            "Embutidos + Minha Pasta",
            "Apenas Minha Pasta",
            "Apenas Embutidos (24)"
        };
        float tm_w = floorf((cw - 16.0f) / 3.0f);
        for (int m = 0; m < 3; m++) {
            if (draw_segmented_option_btn((Rectangle){ cx + (float)m * (tm_w + 8.0f), cy, tm_w, 28.0f },
                                          tpl_modes[m], cfg->templates_mode == m, font_sm, pal)) {
                cfg->templates_mode = m;
                templates_refresh_list(ui);
                changed = true;
            }
        }
        cy += 38.0f;

        DrawTextEx(font_sm, "Pasta de Templates (.c):", (Vector2){ cx, cy }, 13.0f, 1.0f, pal->text_secondary);
        cy += 18.0f;

        Rectangle dir_box = { cx, cy, cw, 28.0f };
        DrawRectangleRounded(dir_box, 0.20f, 4, pal->bg_elevated);
        DrawRectangleRoundedLinesEx(dir_box, 0.20f, 4, 1.0f, pal->border);
        BeginScissorMode((int)dir_box.x + 6, (int)dir_box.y, (int)dir_box.width - 12, (int)dir_box.height);
        DrawTextEx(font_sm, cfg->templates_dir, (Vector2){ dir_box.x + 8.0f, dir_box.y + 7.0f }, 13.0f, 1.0f, pal->text_primary);
        EndScissorMode();
        cy += 34.0f;

        float ab_w = floorf((cw - 24.0f) / 4.0f);
        if (GuiButton((Rectangle){ cx, cy, ab_w, 28.0f }, "Escolher Pasta...")) {
            open_path_dialog_or_fallback(session, ui, 2, cfg->templates_dir);
        }
        if (GuiButton((Rectangle){ cx + (ab_w + 8.0f), cy, ab_w, 28.0f }, "Abrir Pasta")) {
            platform_open_folder_in_file_manager(cfg->templates_dir);
        }
        if (GuiButton((Rectangle){ cx + (ab_w + 8.0f) * 2.0f, cy, ab_w, 28.0f }, "Exportar Padrões")) {
            int count = templates_export_builtins_to_folder(cfg->templates_dir);
            templates_refresh_list(ui);
            char msg[160];
            snprintf(msg, sizeof(msg), "%d templates padrão exportados para a pasta!", count);
            show_toast(ui, msg);
        }
        if (GuiButton((Rectangle){ cx + (ab_w + 8.0f) * 3.0f, cy, ab_w, 28.0f }, "↻ Recarregar")) {
            templates_refresh_list(ui);
            show_toast(ui, "Lista de templates recarregada!");
        }
        cy += 42.0f;

        DrawTextEx(font_ui, "Janela & Desempenho Gráfico", (Vector2){ cx, cy }, 15.0f, 1.0f, pal->accent_secondary);
        cy += 24.0f;

        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Lembrar tamanho da janela e divisão dos painéis ao fechar", &cfg->remember_window_geometry, font_sm, pal)) changed = true;
        cy += 26.0f;
        if (draw_checkbox_row((Rectangle){ cx, cy, cw, 24.0f }, "Ativar VSync (Sincronização Vertical)", &cfg->vsync_enabled, font_sm, pal)) changed = true;
        cy += 32.0f;

        DrawTextEx(font_sm, "Limite de FPS:", (Vector2){ cx, cy + 6.0f }, 13.0f, 1.0f, pal->text_primary);
        static const int fps_opts[4] = { 30, 60, 120, 144 };
        for (int f = 0; f < 4; f++) {
            char flbl[24];
            snprintf(flbl, sizeof(flbl), "%d FPS", fps_opts[f]);
            if (draw_segmented_option_btn((Rectangle){ cx + 115.0f + (float)f * 88.0f, cy, 80.0f, 26.0f },
                                          flbl, cfg->target_fps == fps_opts[f], font_sm, pal)) {
                cfg->target_fps = fps_opts[f];
                SetTargetFPS(cfg->target_fps);
                changed = true;
            }
        }
    }

    if (changed) {
        config_save(cfg);
    }

    /* Modal Footer */
    float foot_y = modal.y + modal.height - 46.0f;
    DrawLineEx((Vector2){ modal.x, foot_y }, (Vector2){ modal.x + modal.width, foot_y }, 1.0f, pal->border);

    if (GuiButton((Rectangle){ modal.x + 14.0f, foot_y + 9.0f, 165.0f, 28.0f }, tr(STR_SETTINGS_RESTORE_DEFAULTS))) {
        config_reset_defaults(cfg);
        ui->theme = theme_get_palette(cfg->theme_id);
        theme_apply_to_raygui(&ui->theme);
        ui_workspace_reload_editor_font(ui);
        SetTargetFPS(cfg->target_fps);
        templates_refresh_list(ui);
        session->user_asan_pref       = cfg->asan_enabled;
        session->watchdog_timeout_sec = cfg->watchdog_timeout_sec;
        config_save(cfg);
        show_toast(ui, "Configurações restauradas para o padrão de fábrica");
    }

    char ini_path[512];
    config_get_ini_path(ini_path, sizeof(ini_path));
    DrawTextEx(font_sm, ini_path, (Vector2){ floorf(modal.x + 195.0f + 0.5f), floorf(foot_y + 16.0f + 0.5f) },
               13.0f, 1.0f, pal->text_secondary);

    if (GuiPrimaryButton((Rectangle){ modal.x + modal.width - 130.0f, foot_y + 9.0f, 116.0f, 28.0f }, tr(STR_SETTINGS_CLOSE))) {
        ui->modal_settings_open = false;
        config_save(cfg);
    }
}

static void probe_one_line_cmd(const char *cmd, char *out_buf, size_t max_sz)
{
    if (!out_buf || max_sz == 0) return;
    out_buf[0] = '\0';
    FILE *p = popen(cmd, "r");
    if (!p) {
        snprintf(out_buf, max_sz, "Não encontrado");
        return;
    }
    if (fgets(out_buf, (int)max_sz, p) != NULL) {
        size_t len = strlen(out_buf);
        while (len > 0 && (out_buf[len - 1] == '\n' || out_buf[len - 1] == '\r')) {
            out_buf[--len] = '\0';
        }
    }
    pclose(p);
    if (out_buf[0] == '\0') {
        snprintf(out_buf, max_sz, "Não encontrado");
    }
}

static void draw_sysdiag_modal(WorkspaceUI *ui, float sw, float sh)
{
    if (!ui->modal_sysdiag_open) return;

    if (!ui->sysdiag_probed) {
        probe_one_line_cmd("LC_ALL=C gcc --version 2>/dev/null | head -n 1", ui->sysdiag_gcc_ver, sizeof(ui->sysdiag_gcc_ver));
        probe_one_line_cmd("LC_ALL=C gdb --version 2>/dev/null | head -n 1", ui->sysdiag_gdb_ver, sizeof(ui->sysdiag_gdb_ver));
        ui->sysdiag_probed = true;
    }

    const ThemePalette *pal = &ui->theme;
    Font font_ui = ui->font_ui;
    Font font_sm = ui->font_sm;

    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, pal->is_light ? 0.35f : 0.62f));
    float mw = 620.0f;
    float mh = 340.0f;
    Rectangle modal = { floorf((sw - mw) * 0.5f), floorf((sh - mh) * 0.5f), mw, mh };

    DrawRectangleRounded(modal, 0.05f, 8, pal->bg_panel);
    DrawRectangleRoundedLinesEx(modal, 0.05f, 8, 1.5f, pal->accent_secondary);

    DrawTextEx(font_ui, tr(STR_SYSDIAG_TITLE),
               (Vector2){ modal.x + 20.0f, modal.y + 14.0f }, 15.0f, 1.0f, pal->accent_secondary);
    DrawLineEx((Vector2){ modal.x, modal.y + 40.0f }, (Vector2){ modal.x + mw, modal.y + 40.0f }, 1.0f, pal->border);

    char ini_path[512];
    config_get_ini_path(ini_path, sizeof(ini_path));
    bool audio_ok = audio_engine_is_ready();

    float y = modal.y + 54.0f;
    float lx = modal.x + 22.0f;
    float vx = modal.x + 175.0f;

    DrawTextEx(font_sm, "AVORA Versão:", (Vector2){ lx, y }, 13.0f, 1.0f, pal->text_secondary);
    DrawTextEx(font_sm, "v" AVORA_VERSION " (Linux x86_64 / C11 / Raylib 5.x)", (Vector2){ vx, y }, 13.0f, 1.0f, pal->accent_primary);
    y += 28.0f;

    DrawTextEx(font_sm, "Compilador GCC:", (Vector2){ lx, y }, 13.0f, 1.0f, pal->text_secondary);
    DrawTextEx(font_sm, ui->sysdiag_gcc_ver, (Vector2){ vx, y }, 13.0f, 1.0f, pal->text_primary);
    y += 28.0f;

    DrawTextEx(font_sm, "Depurador GDB:", (Vector2){ lx, y }, 13.0f, 1.0f, pal->text_secondary);
    DrawTextEx(font_sm, ui->sysdiag_gdb_ver, (Vector2){ vx, y }, 13.0f, 1.0f, pal->text_primary);
    y += 28.0f;

    DrawTextEx(font_sm, "Áudio Procedural:", (Vector2){ lx, y }, 13.0f, 1.0f, pal->text_secondary);
    DrawTextEx(font_sm,
               audio_ok ? "Ativo (miniaudio PCM 44.1 kHz em memória)" : "Fail-Safe Mudo (Sem dispositivo de áudio)",
               (Vector2){ vx, y }, 13.0f, 1.0f, audio_ok ? pal->accent_primary : pal->accent_warning);
    y += 28.0f;

    DrawTextEx(font_sm, "Arquivo config.ini:", (Vector2){ lx, y }, 13.0f, 1.0f, pal->text_secondary);
    DrawTextEx(font_sm, ini_path, (Vector2){ vx, y }, 13.0f, 1.0f, pal->text_primary);
    y += 28.0f;

    DrawTextEx(font_sm, "Pasta Templates:", (Vector2){ lx, y }, 13.0f, 1.0f, pal->text_secondary);
    DrawTextEx(font_sm, ui->config.templates_dir, (Vector2){ vx, y }, 13.0f, 1.0f, pal->text_primary);
    y += 38.0f;

    DrawLineEx((Vector2){ modal.x, modal.y + mh - 50.0f }, (Vector2){ modal.x + mw, modal.y + mh - 50.0f }, 1.0f, pal->border);

    if (GuiButton((Rectangle){ modal.x + 20.0f, modal.y + mh - 39.0f, 340.0f, 28.0f }, tr(STR_SYSDIAG_COPY_MD))) {
        char md[2048];
        snprintf(
            md,
            sizeof(md),
            "### AVORA Environment Diagnostic Report\n"
            "- **AVORA Version:** `v%s`\n"
            "- **GCC:** `%s`\n"
            "- **GDB:** `%s`\n"
            "- **Audio Engine:** `%s` (enabled=%d, vol=%.2f)\n"
            "- **Theme:** `%s` (lang=%d)\n"
            "- **ASan Enabled:** `%s` (Watchdog=%.1fs)\n"
            "- **Config Path:** `%s`\n"
            "- **Templates Dir:** `%s` (%d loaded)\n",
            AVORA_VERSION,
            ui->sysdiag_gcc_ver,
            ui->sysdiag_gdb_ver,
            audio_ok ? "Ready (miniaudio)" : "Fallback Muted",
            ui->config.audio_enabled ? 1 : 0,
            ui->config.audio_volume,
            pal->name,
            ui->config.language,
            ui->config.asan_enabled ? "true" : "false",
            ui->config.watchdog_timeout_sec,
            ini_path,
            ui->config.templates_dir,
            ui->template_count
        );
        SetClipboardText(md);
        audio_play_sfx(SFX_UI_CLICK, &ui->config, 0.1f);
        show_toast(ui, tr(STR_SYSDIAG_COPIED_TOAST));
    }

    if (GuiPrimaryButton((Rectangle){ modal.x + mw - 134.0f, modal.y + mh - 39.0f, 114.0f, 28.0f }, tr(STR_SETTINGS_CLOSE))) {
        ui->modal_sysdiag_open = false;
    }
}

/* ============================================================================
 * 11. AUXILIARY MODALS (SHORTCUTS, ABOUT, UNSAVED CONFIRM, PATH INPUT & TOAST)
 * ============================================================================ */

static void draw_shortcuts_modal(WorkspaceUI *ui, float sw, float sh)
{
    if (!ui->modal_shortcuts_open) return;
    const ThemePalette *pal = &ui->theme;
    Font font_ui = ui->font_ui;
    Font font_sm = ui->font_sm;

    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, pal->is_light ? 0.35f : 0.60f));
    float mw = fminf(880.0f, sw - 32.0f);
    float mh = 425.0f;
    Rectangle modal = { floorf((sw - mw) * 0.5f), floorf((sh - mh) * 0.5f), mw, mh };

    DrawRectangleRounded(modal, 0.05f, 8, pal->bg_panel);
    DrawRectangleRoundedLinesEx(modal, 0.05f, 8, 1.4f, pal->accent_secondary);

    DrawTextEx(font_ui, "⌨ Atalhos de Teclado do AVORA",
               (Vector2){ floorf(modal.x + 18.0f), floorf(modal.y + 14.0f) }, 15.0f, 1.0f, pal->accent_secondary);
    DrawLineEx((Vector2){ modal.x, modal.y + 40.0f }, (Vector2){ modal.x + mw, modal.y + 40.0f }, 1.0f, pal->border);

    static const struct { const char *key; const char *desc; } left_col[] = {
        { "Ctrl + N",       "Novo Scratchpad Livre" },
        { "Ctrl + O",       "Abrir arquivo .c" },
        { "Ctrl + S",       "Salvar arquivo .c atual" },
        { "Ctrl + Shift+S", "Salvar Como..." },
        { "Ctrl + E",       "Exportar Canvas PNG" },
        { "Ctrl + R",       "Resetar Template atual" },
        { "Ctrl + ,",       "Abrir Configurações" },
        { "Ctrl + Q",       "Sair do AVORA" },
        { "Ctrl + Z / Y",   "Desfazer / Refazer" }
    };
    static const struct { const char *key; const char *desc; } right_col[] = {
        { "F5 / Shift+F5",  "Compilar & Iniciar / Parar" },
        { "F10 / Shift+F10","Avançar / Voltar (Time-Travel)" },
        { "F9",             "Continuar até Breakpoint" },
        { "F8",             "Auto-Play / Pausar" },
        { "Clique Gutter",  "Alternar Breakpoint (●)" },
        { "Ctrl + ` / J",   "Mostrar / Ocultar Terminal" },
        { "Ctrl + Shift+P", "Modo Apresentação (Palco)" },
        { "Ctrl + + / - / 0","Zoom da Fonte do Editor" },
        { "F11",            "Alternar Tela Cheia" }
    };

    float y0      = modal.y + 56.0f;
    float col_w   = floorf((mw - 52.0f) * 0.5f);
    float left_x  = modal.x + 18.0f;
    float right_x = left_x + col_w + 16.0f;
    float kb_w    = 134.0f;

    for (int i = 0; i < 9; i++) {
        float y = y0 + (float)i * 32.0f;

        /* Left column */
        Rectangle kb1 = { left_x, y, kb_w, 24.0f };
        DrawRectangleRounded(kb1, 0.25f, 4, pal->bg_elevated);
        DrawRectangleRoundedLinesEx(kb1, 0.25f, 4, 1.0f, pal->border);
        DrawTextEx(font_sm, left_col[i].key, (Vector2){ floorf(kb1.x + 8.0f), floorf(kb1.y + 5.0f) }, 13.0f, 1.0f, pal->accent_primary);
        BeginScissorMode((int)(kb1.x + kb_w + 8.0f), (int)y, (int)(col_w - kb_w - 8.0f), 24);
        DrawTextEx(font_sm, left_col[i].desc, (Vector2){ floorf(kb1.x + kb_w + 8.0f), floorf(kb1.y + 5.0f) }, 13.0f, 1.0f, pal->text_primary);
        EndScissorMode();

        /* Right column */
        Rectangle kb2 = { right_x, y, kb_w, 24.0f };
        DrawRectangleRounded(kb2, 0.25f, 4, pal->bg_elevated);
        DrawRectangleRoundedLinesEx(kb2, 0.25f, 4, 1.0f, pal->border);
        DrawTextEx(font_sm, right_col[i].key, (Vector2){ floorf(kb2.x + 8.0f), floorf(kb2.y + 5.0f) }, 13.0f, 1.0f, pal->accent_secondary);
        BeginScissorMode((int)(kb2.x + kb_w + 8.0f), (int)y, (int)(col_w - kb_w - 8.0f), 24);
        DrawTextEx(font_sm, right_col[i].desc, (Vector2){ floorf(kb2.x + kb_w + 8.0f), floorf(kb2.y + 5.0f) }, 13.0f, 1.0f, pal->text_primary);
        EndScissorMode();
    }

    if (GuiPrimaryButton((Rectangle){ modal.x + mw - 132.0f, modal.y + mh - 42.0f, 114.0f, 28.0f }, "Fechar (Esc)")) {
        ui->modal_shortcuts_open = false;
    }
}

static void draw_about_modal(WorkspaceUI *ui, float sw, float sh)
{
    if (!ui->modal_about_open) return;
    const ThemePalette *pal = &ui->theme;
    Font font_ui = ui->font_ui;
    Font font_sm = ui->font_sm;

    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, pal->is_light ? 0.35f : 0.60f));
    float mw = 480.0f;
    float mh = 270.0f;
    Rectangle modal = { floorf((sw - mw) * 0.5f), floorf((sh - mh) * 0.5f), mw, mh };

    DrawRectangleRounded(modal, 0.06f, 8, pal->bg_panel);
    DrawRectangleRoundedLinesEx(modal, 0.06f, 8, 1.4f, pal->accent_primary);

    DrawTextEx(font_ui, "AVORA v" AVORA_VERSION,
               (Vector2){ modal.x + 22.0f, modal.y + 20.0f }, 15.0f, 1.0f, pal->accent_primary);
    DrawTextEx(font_sm, "Analisador Visual de Operações e Rastreamento de Árvores",
               (Vector2){ modal.x + 22.0f, modal.y + 44.0f }, 13.0f, 1.0f, pal->text_primary);

    DrawLineEx((Vector2){ modal.x + 18.0f, modal.y + 70.0f }, (Vector2){ modal.x + mw - 18.0f, modal.y + 70.0f }, 1.0f, pal->border);

    DrawTextEx(font_sm,
               "Ambiente visual de depuração em tempo real para C11:\n"
               "• Árvores Binárias, Florestas, AVL, Rubro-Negra & Árvore B\n"
               "• Listas Encadeadas (next/prev) & Vetores 1D (Busca e Ordenação)\n"
               "• Introspecção via GDB/MI + GCC + AddressSanitizer (ASan)\n"
               "• Temas Monkeytype, Modo Apresentação & Templates Dinâmicos",
               (Vector2){ modal.x + 22.0f, modal.y + 84.0f }, 13.0f, 1.0f, pal->text_secondary);

    if (GuiButton((Rectangle){ modal.x + 22.0f, modal.y + mh - 46.0f, 210.0f, 28.0f }, "Abrir Pasta ~/.config/avora")) {
        char cfg_dir[512];
        config_get_dir_path(cfg_dir, sizeof(cfg_dir));
        platform_open_folder_in_file_manager(cfg_dir);
    }
    if (GuiPrimaryButton((Rectangle){ modal.x + mw - 126.0f, modal.y + mh - 46.0f, 104.0f, 28.0f }, "Fechar")) {
        ui->modal_about_open = false;
    }
}

static void draw_unsaved_confirm_modal(DebuggerSession *session, WorkspaceUI *ui, float sw, float sh)
{
    if (!ui->modal_unsaved_confirm_open) return;
    const ThemePalette *pal = &ui->theme;
    Font font_ui = ui->font_ui;
    Font font_sm = ui->font_sm;

    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, pal->is_light ? 0.35f : 0.65f));
    float mw = 470.0f;
    float mh = 175.0f;
    Rectangle modal = { floorf((sw - mw) * 0.5f), floorf((sh - mh) * 0.5f), mw, mh };

    DrawRectangleRounded(modal, 0.06f, 8, pal->bg_panel);
    DrawRectangleRoundedLinesEx(modal, 0.06f, 8, 1.5f, pal->accent_warning);

    DrawTextEx(font_ui, "⚠ Alterações Não Salvas no Editor",
               (Vector2){ modal.x + 20.0f, modal.y + 18.0f }, 15.0f, 1.0f, pal->accent_warning);
    DrawTextEx(font_sm,
               "O código atual possui modificações não salvas.\nDeseja salvar suas alterações antes de continuar?",
               (Vector2){ modal.x + 20.0f, modal.y + 52.0f }, 13.0f, 1.0f, pal->text_primary);

    float by = modal.y + mh - 46.0f;
    if (GuiPrimaryButton((Rectangle){ modal.x + 20.0f, by, 155.0f, 30.0f }, "Salvar e Continuar")) {
        ui->modal_unsaved_confirm_open = false;
        if (ui->current_file_path[0] != '\0') {
            save_editor_to_path(session, ui, ui->current_file_path);
            execute_pending_action(session, ui, ui->pending_action_type, ui->pending_action_param);
        } else {
            open_path_dialog_or_fallback(session, ui, 1, "algoritmo.c");
            if (!ui->is_dirty) {
                execute_pending_action(session, ui, ui->pending_action_type, ui->pending_action_param);
            }
        }
        ui->pending_action_type = 0;
    }

    if (GuiButton((Rectangle){ modal.x + 185.0f, by, 155.0f, 30.0f }, "Descartar Alterações")) {
        ui->modal_unsaved_confirm_open = false;
        ui->is_dirty                   = false;
        int act = ui->pending_action_type;
        int prm = ui->pending_action_param;
        ui->pending_action_type = 0;
        execute_pending_action(session, ui, act, prm);
    }

    if (GuiButton((Rectangle){ modal.x + 350.0f, by, 100.0f, 30.0f }, "Cancelar")) {
        ui->modal_unsaved_confirm_open = false;
        ui->pending_action_type        = 0;
    }
}

static void draw_path_input_modal(DebuggerSession *session, WorkspaceUI *ui, float sw, float sh)
{
    if (!ui->modal_path_input_open) return;
    const ThemePalette *pal = &ui->theme;
    Font font_ui = ui->font_ui;
    Font font_sm = ui->font_sm;

    DrawRectangle(0, 0, (int)sw, (int)sh, Fade(BLACK, pal->is_light ? 0.35f : 0.65f));
    float mw = 540.0f;
    float mh = 175.0f;
    Rectangle modal = { floorf((sw - mw) * 0.5f), floorf((sh - mh) * 0.5f), mw, mh };

    DrawRectangleRounded(modal, 0.06f, 8, pal->bg_panel);
    DrawRectangleRoundedLinesEx(modal, 0.06f, 8, 1.5f, pal->accent_secondary);

    const char *titles[4] = {
        "Abrir Arquivo C (.c)",
        "Salvar Arquivo C (.c)",
        "Definir Caminho da Pasta de Templates",
        "Exportar Canvas como Imagem (.png)"
    };
    int mode = (ui->path_input_mode >= 0 && ui->path_input_mode <= 3) ? ui->path_input_mode : 0;

    DrawTextEx(font_ui, titles[mode], (Vector2){ modal.x + 20.0f, modal.y + 16.0f }, 15.0f, 1.0f, pal->accent_secondary);
    DrawTextEx(font_sm, "Informe o caminho do arquivo ou diretório:", (Vector2){ modal.x + 20.0f, modal.y + 44.0f }, 13.0f, 1.0f, pal->text_secondary);

    Rectangle tb = { modal.x + 20.0f, modal.y + 68.0f, mw - 40.0f, 32.0f };
    GuiTextBox(tb, ui->path_input_buf, sizeof(ui->path_input_buf), true);

    bool confirm = IsKeyPressed(KEY_ENTER);
    if (GuiPrimaryButton((Rectangle){ modal.x + mw - 236.0f, modal.y + mh - 44.0f, 108.0f, 28.0f }, "Confirmar")) {
        confirm = true;
    }
    if (GuiButton((Rectangle){ modal.x + mw - 118.0f, modal.y + mh - 44.0f, 98.0f, 28.0f }, "Cancelar")) {
        ui->modal_path_input_open = false;
        return;
    }

    if (confirm && ui->path_input_buf[0] != '\0') {
        ui->modal_path_input_open = false;
        if (mode == 0) {
            load_file_into_editor(session, ui, ui->path_input_buf, true);
        } else if (mode == 1) {
            save_editor_to_path(session, ui, ui->path_input_buf);
        } else if (mode == 2) {
            snprintf(ui->config.templates_dir, sizeof(ui->config.templates_dir), "%s", ui->path_input_buf);
            templates_refresh_list(ui);
            config_save(&ui->config);
            show_toast(ui, "Pasta de templates atualizada!");
        } else if (mode == 3) {
            snprintf(ui->pending_png_path, sizeof(ui->pending_png_path), "%s", ui->path_input_buf);
            ui->pending_png_capture = true;
        }
    }
}

static void draw_floating_toast(WorkspaceUI *ui, float sw)
{
    if (ui->toast_timer <= 0.0f || ui->toast_message[0] == '\0') return;
    ui->toast_timer -= GetFrameTime();

    const ThemePalette *pal = &ui->theme;
    Font font = ui->font_sm;
    Vector2 tsz = MeasureTextEx(font, ui->toast_message, 13.0f, 1.0f);
    float pw = tsz.x + 28.0f;
    float ph = 30.0f;
    Rectangle pill = { floorf(sw - pw - 16.0f), MENU_BAR_H + 12.0f, pw, ph };

    DrawRectangleRounded(pill, 0.35f, 6, Fade(pal->bg_elevated, 0.96f));
    DrawRectangleRoundedLinesEx(pill, 0.35f, 6, 1.4f, pal->accent_primary);
    DrawTextEx(
        font,
        ui->toast_message,
        (Vector2){ floorf(pill.x + 14.0f + 0.5f), floorf(pill.y + (pill.height - tsz.y) * 0.5f + 0.5f) },
        13.0f,
        1.0f,
        pal->text_primary
    );
}

/* ============================================================================
 * 12. MAIN WORKSPACE UPDATE & DRAW (v3.1.0 DESKTOP EXPERIENCE)
 * ============================================================================ */

void ui_workspace_update_and_draw(DebuggerSession *session, WorkspaceUI *ui)
{
    if (!session || !ui) return;

    float dt = GetFrameTime();
    float sw = (float)GetScreenWidth();
    float sh = (float)GetScreenHeight();
    Font  font = GuiGetFont();
    float fs   = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
    const ThemePalette *pal = &ui->theme;

    /* 1. Poll GDB non-blocking pipes and check configurable POSIX process-group watchdog */
    ErrorType prev_err      = session->diagnostic.error_type;
    bool prev_has_stdout    = session->has_new_stdout;
    bool prev_waiting_step  = session->waiting_for_step;
    gdb_poll_and_parse(session);
    watchdog_check(session);

    if (prev_err == ERR_NONE && session->diagnostic.error_type != ERR_NONE) {
        if (!ui->config.presentation_mode) {
            ui->terminal_open       = true;
            ui->terminal_active_tab = 0;
        }
        ui->terminal_scroll_y        = 0.0f;
        ui->terminal_target_scroll_y = 0.0f;
        ui->error_shake_timer        = 0.16f;
        ui->error_flash_timer        = 0.16f;
        audio_play_sfx(SFX_ERROR_THUD, &ui->config, 0.0f);
    } else if (prev_waiting_step && !session->waiting_for_step && session->diagnostic.error_type == ERR_NONE) {
        if (session->snapshot.mutated_node_count > 0) {
            float pitch_var = fminf(0.32f, (float)session->snapshot.node_count * 0.02f);
            audio_play_sfx(SFX_NODE_POP, &ui->config, pitch_var);
        } else if (session->state == STATE_PAUSED || session->state == STATE_RUNNING) {
            audio_play_sfx(SFX_STEP_TICK, &ui->config, 0.0f);
        }
    }

    if (!prev_has_stdout && session->has_new_stdout && ui->config.auto_open_terminal_on_stdout && !ui->config.presentation_mode) {
        ui->terminal_open       = true;
        ui->terminal_active_tab = 1;
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

    /* Handle Global Keyboard Shortcuts (F5, F10, F8, F11, Ctrl+S, Ctrl+O, Ctrl+,, etc.) */
    handle_global_shortcuts(session, ui, step_interval_sec);

    bool overlay_blocking = (ui->active_menu >= 0) ||
                            ui->modal_settings_open ||
                            ui->modal_shortcuts_open ||
                            ui->modal_about_open ||
                            ui->modal_sysdiag_open ||
                            ui->modal_path_input_open ||
                            ui->modal_unsaved_confirm_open;

    /* 3. Interactive Vertical Splitter (Editor Left vs. Full-Height Canvas Right below MENU_BAR_H) */
    Vector2 mouse   = GetMousePosition();
    float   main_y  = MENU_BAR_H;
    float   main_h  = sh - MENU_BAR_H - FOOTER_BAR_H;
    float   left_w  = floorf(sw * ui->split_vertical_ratio);
    float   right_w = sw - left_w;

    Rectangle v_splitter = { left_w - SPLITTER_THICK * 0.5f, main_y, SPLITTER_THICK, main_h };
    bool hover_v = !overlay_blocking && !ui->dropdown_edit_mode && CheckCollisionPointRec(mouse, v_splitter);

    if (hover_v && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        ui->dragging_v_split = true;
        ui->is_panning       = false;
    }
    if (ui->dragging_v_split) {
        if (!overlay_blocking && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
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

    Rectangle left_panel   = { 0.0f,   main_y,          left_w,  main_h };
    Rectangle canvas_panel = { left_w, main_y,          right_w, main_h };
    Rectangle footer_bar   = { 0.0f,   main_y + main_h, sw,      FOOTER_BAR_H };

    /* 4. Update Non-Overlapping Tree/Forest/List Layout & 60 FPS Lerp Animations */
    layout_compute_tree_positions(&session->snapshot, canvas_panel);
    layout_update_animations(&session->snapshot, dt, &ui->pulse_timer, ui->config.reduce_animations);

    if (overlay_blocking) {
        GuiLock();
    } else {
        GuiUnlock();
    }

    /* --- PANEL 1: LEFT PANEL (Theme Code Editor + Dynamic Template Bar) --- */
    DrawRectangleRec(left_panel, pal->bg_panel);

    const char *lbl_reset = tr(STR_BTN_RESET);
    float reset_w = floorf(MeasureTextEx(font, lbl_reset, fs, 1.0f).x + 22.0f);
    Rectangle reset_btn = { left_panel.x + left_panel.width - reset_w - 8.0f, left_panel.y + 8.0f, reset_w, 28.0f };
    if (GuiButton(reset_btn, lbl_reset)) {
        audio_play_sfx(SFX_UI_CLICK, &ui->config, 0.0f);
        request_guarded_action(session, ui, 4, 0);
    }

    Rectangle editor_rect = {
        left_panel.x + 8.0f,
        left_panel.y + 42.0f,
        left_panel.width - 16.0f,
        left_panel.height - 48.0f
    };
    update_and_draw_code_editor(session, ui, editor_rect, overlay_blocking);

    /* --- PANEL 2: RIGHT PANEL (Interactive Memory Canvas + Floating HUDs) --- */
    layout_render_canvas(session, ui, canvas_panel);

    /* --- VERTICAL SPLITTER BAR --- */
    Color v_col = (hover_v || ui->dragging_v_split) ? pal->accent_secondary : pal->border;
    DrawRectangleRec(v_splitter, v_col);

    /* --- RETRACTABLE TERMINAL DRAWER (Slides up above Footer Bar) --- */
    draw_retractable_terminal(session, ui, sw, footer_bar.y, ui->font_ui, ui->font_sm, overlay_blocking);

    /* --- FOOTER BAR: PROGRESSIVE DISCLOSURE PLAYBACK + STATUS SUMMARY PILL --- */
    DrawRectangleRec(footer_bar, pal->bg_panel);
    DrawLineEx((Vector2){ 0.0f, footer_bar.y }, (Vector2){ sw, footer_bar.y }, 1.0f, pal->border);

    float ctrl_y = footer_bar.y + 7.0f;
    float cur_x  = 10.0f;
    float row_h  = 30.0f;
    bool  is_running_session = (session->gdb_pid > 0) &&
                               (session->state == STATE_PAUSED ||
                                session->state == STATE_RUNNING ||
                                session->state == STATE_STEPPING);

    if (!is_running_session) {
        const char *lbl_run = tr(STR_BTN_COMPILE_START);
        float w_run = MeasureTextEx(font, lbl_run, fs, 1.0f).x + 28.0f;
        if (GuiPrimaryButton((Rectangle){ cur_x, ctrl_y, w_run, row_h }, lbl_run)) {
            trigger_compile_and_start(session, ui);
        }
        cur_x += w_run + 12.0f;
    } else {
        const char *lbl_stop = tr(STR_BTN_STOP);
        float w_stop = MeasureTextEx(font, lbl_stop, fs, 1.0f).x + 18.0f;
        if (GuiButton((Rectangle){ cur_x, ctrl_y, w_stop, row_h }, lbl_stop)) {
            audio_play_sfx(SFX_UI_CLICK, &ui->config, -0.08f);
            gdb_stop_session(session);
            session->state = STATE_IDLE;
            diagnostics_clear(&session->diagnostic);
        }
        cur_x += w_stop + 6.0f;

        const char *lbl_back = tr(STR_BTN_STEP_BACK);
        float w_back = MeasureTextEx(font, lbl_back, fs, 1.0f).x + 18.0f;
        bool can_back = session_can_step_back(session);
        if (!can_back && !overlay_blocking) GuiDisable();
        if (GuiButton((Rectangle){ cur_x, ctrl_y, w_back, row_h }, lbl_back) && can_back) {
            ui->blob_idle_timer = 0.0f;
            if (session_step_back(session)) {
                audio_play_sfx(SFX_STEP_TICK, &ui->config, -0.10f);
            }
        }
        if (!can_back && !overlay_blocking) GuiEnable();
        cur_x += w_back + 6.0f;

        const char *lbl_step = tr(STR_BTN_STEP);
        float w_step = MeasureTextEx(font, lbl_step, fs, 1.0f).x + 20.0f;
        if (GuiPrimaryButton((Rectangle){ cur_x, ctrl_y, w_step, row_h }, lbl_step)) {
            ui->blob_idle_timer = 0.0f;
            session->auto_play  = false;
            int prev_cur = session->history_cursor;
            session_step_forward_or_gdb(session);
            if (session->history_cursor != prev_cur) {
                audio_play_sfx(SFX_STEP_TICK, &ui->config, 0.05f);
            }
        }
        cur_x += w_step + 6.0f;

        const char *lbl_cont = tr(STR_BTN_CONTINUE);
        float w_cont = MeasureTextEx(font, lbl_cont, fs, 1.0f).x + 18.0f;
        if (GuiButton((Rectangle){ cur_x, ctrl_y, w_cont, row_h }, lbl_cont)) {
            audio_play_sfx(SFX_UI_CLICK, &ui->config, 0.05f);
            ui->blob_idle_timer = 0.0f;
            session->auto_play  = false;
            gdb_request_continue(session);
        }
        cur_x += w_cont + 6.0f;

        const char *lbl_play = session->auto_play ? tr(STR_BTN_PAUSE_AUTO) : tr(STR_BTN_AUTOPLAY);
        float w_play = MeasureTextEx(font, lbl_play, fs, 1.0f).x + 20.0f;
        if (GuiButton((Rectangle){ cur_x, ctrl_y, w_play, row_h }, lbl_play)) {
            audio_play_sfx(SFX_UI_CLICK, &ui->config, 0.05f);
            ui->blob_idle_timer = 0.0f;
            session->auto_play  = !session->auto_play;
            if (session->auto_play && session->state == STATE_PAUSED) {
                if (session->history_count > 0) {
                    session->history_cursor = session->history_count - 1;
                    session->snapshot = session->history_snapshots[session->history_cursor];
                }
                session->state            = STATE_RUNNING;
                session->step_accumulator = step_interval_sec;
            } else if (!session->auto_play && session->state == STATE_RUNNING) {
                session->state = STATE_PAUSED;
            }
        }
        cur_x += w_play + 10.0f;

        /* Speed Slider */
        char speed_txt[32];
        if (session->play_speed < 1.0f) {
            snprintf(speed_txt, sizeof(speed_txt), "%.2fx", session->play_speed);
        } else if (session->play_speed < 10.0f) {
            snprintf(speed_txt, sizeof(speed_txt), "%.1fx", session->play_speed);
        } else {
            snprintf(speed_txt, sizeof(speed_txt), "%.0fx", session->play_speed);
        }
        float slider_w = 88.0f;
        GuiSliderBar((Rectangle){ cur_x, ctrl_y + 6.0f, slider_w, 18.0f }, NULL, NULL, &session->speed_slider_t, 0.0f, 1.0f);
        cur_x += slider_w + 5.0f;
        float w_spd = MeasureTextEx(font, speed_txt, fs, 1.0f).x + 8.0f;
        GuiLabel((Rectangle){ cur_x, ctrl_y, w_spd, row_h }, speed_txt);
        cur_x += w_spd + 8.0f;
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
    Color dot_col  = has_alert ? pal->accent_error :
                     (session->state == STATE_FINISHED ? pal->accent_primary :
                     (is_running_session ? pal->accent_warning : pal->accent_secondary));

    bool hov_status = !overlay_blocking && CheckCollisionPointRec(mouse, status_pill);
    DrawRectangleRounded(status_pill, 0.30f, 6, hov_status ? pal->bg_hover : pal->bg_elevated);
    DrawRectangleRoundedLinesEx(status_pill, 0.30f, 6, 1.2f, has_alert ? pal->accent_error : pal->border);

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
        if (session->history_count > 1 && session->history_cursor + 1 < session->history_count) {
            snprintf(status_msg, sizeof(status_msg), "● [TIME-TRAVEL %d/%d] Linha %d (%s) | %d nós na Heap — Terminal %s",
                     session->history_cursor + 1,
                     session->history_count,
                     session->snapshot.current_executing_line,
                     session->snapshot.current_func[0] ? session->snapshot.current_func : "main",
                     session->snapshot.node_count,
                     arrow);
        } else {
            snprintf(status_msg, sizeof(status_msg), "● [DEPURANDO] Linha %d (%s) | %d nós na Heap — Terminal %s",
                     session->snapshot.current_executing_line,
                     session->snapshot.current_func[0] ? session->snapshot.current_func : "main",
                     session->snapshot.node_count,
                     arrow);
        }
    } else {
        snprintf(status_msg, sizeof(status_msg), "● [MODO EDIÇÃO] F5 para Compilar & Iniciar — Terminal & Logs %s", arrow);
    }

    BeginScissorMode((int)status_pill.x + 6, (int)status_pill.y, (int)status_pill.width - 12, (int)status_pill.height);
    Vector2 status_sz = MeasureTextEx(ui->font_sm, status_msg, 13.0f, 1.0f);
    DrawTextEx(
        ui->font_sm,
        status_msg,
        (Vector2){ floorf(status_pill.x + 10.0f + 0.5f), floorf(status_pill.y + (status_pill.height - status_sz.y) * 0.5f + 0.5f) },
        13.0f,
        1.0f,
        dot_col
    );
    EndScissorMode();

    /* --- RENDER DYNAMIC TEMPLATES DROPDOWN BEFORE TOP MENU OVERLAYS --- */
    Rectangle drop_rect = {
        left_panel.x + 8.0f,
        left_panel.y + 8.0f,
        left_panel.width - reset_w - 22.0f,
        28.0f
    };

    char preset_items[MAX_DYNAMIC_TEMPLATES * 132];
    preset_items[0] = '\0';
    size_t pos = 0;
    for (int i = 0; i < ui->template_count; i++) {
        int written = snprintf(
            preset_items + pos,
            sizeof(preset_items) - pos,
            "%s%s",
            (i > 0) ? ";" : "",
            ui->templates[i].display_name
        );
        if (written > 0 && pos + (size_t)written < sizeof(preset_items)) {
            pos += (size_t)written;
        }
    }

    int prev_tpl = ui->active_template_idx;
    int sel_tpl  = ui->active_template_idx;
    if (GuiDropdownBox(drop_rect, preset_items, &sel_tpl, ui->dropdown_edit_mode)) {
        ui->dropdown_edit_mode = !ui->dropdown_edit_mode;
        ui->blob_idle_timer    = 0.0f;
        if (ui->dropdown_edit_mode) {
            /* Automatically rescan folder when user opens dropdown so newly dropped .c files appear live! */
            templates_refresh_list(ui);
        } else if (sel_tpl != prev_tpl) {
            request_guarded_action(session, ui, 3, sel_tpl);
        }
    }

    /* Unlock RayGUI so modals and top menu bar can interact normally */
    GuiUnlock();

    /* --- TOP MENU BAR & DROPDOWN MENUS (Drawn above panels) --- */
    draw_top_menu_bar_and_dropdowns(session, ui, sw, step_interval_sec);

    /* --- FLOATING TOAST NOTIFICATION --- */
    draw_floating_toast(ui, sw);

    /* --- DESKTOP MODALS (Drawn on top of everything) --- */
    draw_settings_modal(session, ui, sw, sh);
    draw_shortcuts_modal(ui, sw, sh);
    draw_about_modal(ui, sw, sh);
    draw_sysdiag_modal(ui, sw, sh);
    draw_unsaved_confirm_modal(session, ui, sw, sh);
    draw_path_input_modal(session, ui, sw, sh);
}
