#include "visualizer.h"
#include "raygui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

/* ============================================================================
 * 1. DIRECTORY & INI PATH HELPERS
 * ============================================================================ */

static void ensure_dir_exists(const char *path)
{
    if (!path || path[0] == '\0') return;
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    size_t len = strlen(tmp);
    if (len > 1 && tmp[len - 1] == '/') tmp[len - 1] = '\0';

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0755);
            *p = '/';
        }
    }
    mkdir(tmp, 0755);
}

void config_get_dir_path(char *out_buf, size_t max_len)
{
    if (!out_buf || max_len == 0) return;
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0] != '\0') {
        snprintf(out_buf, max_len, "%s/avora", xdg);
        return;
    }
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        snprintf(out_buf, max_len, "%s/.config/avora", home);
        return;
    }
    snprintf(out_buf, max_len, "/tmp/avora_config");
}

void config_get_ini_path(char *out_buf, size_t max_len)
{
    char dir[448];
    config_get_dir_path(dir, sizeof(dir));
    snprintf(out_buf, max_len, "%s/config.ini", dir);
}

/* ============================================================================
 * 2. APPCONFIG DEFAULTS, INI PARSER & PERSISTENCE
 * ============================================================================ */

void config_reset_defaults(AppConfig *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));

    /* Appearance & Localization */
    cfg->language              = LANG_PT_BR;
    cfg->theme_id              = THEME_MIDNIGHT_SLATE;
    cfg->canvas_bg_style       = 0; /* 0 = Dot Grid */
    cfg->reduce_animations     = false;
    cfg->show_memory_addresses = true;
    cfg->show_hud              = true;
    cfg->show_blob             = true;
    cfg->presentation_mode     = false;
    cfg->audio_enabled         = true;
    cfg->audio_volume          = 0.45f;
    i18n_set_language(LANG_PT_BR);

    /* Code Editor */
    cfg->editor_font_size       = 16;
    cfg->tab_size               = 4;
    cfg->show_line_numbers      = true;
    cfg->highlight_current_line = true;
    cfg->auto_indent            = true;
    cfg->restore_last_session   = true;

    /* Debugger (GCC / GDB) */
    cfg->asan_enabled                 = true;
    cfg->watchdog_timeout_sec         = 1.5f;
    cfg->default_speed_t              = 0.391f; /* ~1.00x */
    cfg->auto_open_terminal_on_stdout = false;
    cfg->start_paused_on_main         = true;
    cfg->enable_stdin                 = false;

    /* Templates & Classroom Folder */
    cfg->templates_mode = 0; /* 0 = Embutidos + Pasta */
    char dir[448];
    config_get_dir_path(dir, sizeof(dir));
    snprintf(cfg->templates_dir, sizeof(cfg->templates_dir), "%s/templates", dir);

    /* Window & Performance */
    cfg->remember_window_geometry = true;
    cfg->window_w                 = 1440;
    cfg->window_h                 = 900;
    cfg->window_maximized         = false;
    cfg->split_ratio              = 0.42f;
    cfg->vsync_enabled            = true;
    cfg->target_fps               = 60;
    cfg->confirm_unsaved_changes  = true;

    /* Session */
    cfg->recent_count           = 0;
    cfg->last_open_file[0]      = '\0';
    cfg->last_selected_template = 1;
}

static char *trim_whitespace(char *str)
{
    while (*str && isspace((unsigned char)*str)) str++;
    if (*str == '\0') return str;
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return str;
}

static bool parse_bool_str(const char *val)
{
    if (!val) return false;
    return (strcmp(val, "1") == 0 ||
            strcasecmp(val, "true") == 0 ||
            strcasecmp(val, "yes") == 0 ||
            strcasecmp(val, "on") == 0);
}

bool config_load(AppConfig *cfg)
{
    if (!cfg) return false;
    char dir[480];
    config_get_dir_path(dir, sizeof(dir));
    ensure_dir_exists(dir);
    ensure_dir_exists(cfg->templates_dir);

    char ini_path[512];
    config_get_ini_path(ini_path, sizeof(ini_path));
    FILE *fp = fopen(ini_path, "r");
    if (!fp) {
        /* Create initial config.ini with defaults */
        config_save(cfg);
        return false;
    }

    char line[1024];
    cfg->recent_count = 0;

    while (fgets(line, sizeof(line), fp)) {
        char *s = trim_whitespace(line);
        if (s[0] == '\0' || s[0] == '#' || s[0] == ';' || s[0] == '[') continue;

        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = trim_whitespace(s);
        char *val = trim_whitespace(eq + 1);

        if (strcmp(key, "language") == 0) {
            int v = atoi(val);
            if (v >= 0 && v < LANG_COUNT) {
                cfg->language = v;
                i18n_set_language((AppLanguage)v);
            }
        } else if (strcmp(key, "theme_id") == 0) {
            int v = atoi(val);
            if (v >= 0 && v < THEME_COUNT) cfg->theme_id = v;
        } else if (strcmp(key, "canvas_bg_style") == 0) {
            int v = atoi(val);
            if (v >= 0 && v <= 2) cfg->canvas_bg_style = v;
        } else if (strcmp(key, "reduce_animations") == 0) {
            cfg->reduce_animations = parse_bool_str(val);
        } else if (strcmp(key, "show_memory_addresses") == 0) {
            cfg->show_memory_addresses = parse_bool_str(val);
        } else if (strcmp(key, "show_hud") == 0) {
            cfg->show_hud = parse_bool_str(val);
        } else if (strcmp(key, "show_blob") == 0) {
            cfg->show_blob = parse_bool_str(val);
        } else if (strcmp(key, "presentation_mode") == 0) {
            cfg->presentation_mode = parse_bool_str(val);
        } else if (strcmp(key, "audio_enabled") == 0) {
            cfg->audio_enabled = parse_bool_str(val);
        } else if (strcmp(key, "audio_volume") == 0) {
            float v = (float)atof(val);
            if (v >= 0.0f && v <= 1.0f) cfg->audio_volume = v;
        } else if (strcmp(key, "editor_font_size") == 0) {
            int v = atoi(val);
            if (v >= 12 && v <= 24) cfg->editor_font_size = v;
        } else if (strcmp(key, "tab_size") == 0) {
            int v = atoi(val);
            cfg->tab_size = (v == 2) ? 2 : 4;
        } else if (strcmp(key, "show_line_numbers") == 0) {
            cfg->show_line_numbers = parse_bool_str(val);
        } else if (strcmp(key, "highlight_current_line") == 0) {
            cfg->highlight_current_line = parse_bool_str(val);
        } else if (strcmp(key, "auto_indent") == 0) {
            cfg->auto_indent = parse_bool_str(val);
        } else if (strcmp(key, "restore_last_session") == 0) {
            cfg->restore_last_session = parse_bool_str(val);
        } else if (strcmp(key, "asan_enabled") == 0) {
            cfg->asan_enabled = parse_bool_str(val);
        } else if (strcmp(key, "watchdog_timeout_sec") == 0) {
            float v = (float)atof(val);
            if (v >= 0.5f && v <= 30.0f) cfg->watchdog_timeout_sec = v;
        } else if (strcmp(key, "default_speed_t") == 0) {
            float v = (float)atof(val);
            if (v >= 0.0f && v <= 1.0f) cfg->default_speed_t = v;
        } else if (strcmp(key, "auto_open_terminal_on_stdout") == 0) {
            cfg->auto_open_terminal_on_stdout = parse_bool_str(val);
        } else if (strcmp(key, "start_paused_on_main") == 0) {
            cfg->start_paused_on_main = parse_bool_str(val);
        } else if (strcmp(key, "enable_stdin") == 0) {
            cfg->enable_stdin = parse_bool_str(val);
        } else if (strcmp(key, "templates_mode") == 0) {
            int v = atoi(val);
            if (v >= 0 && v <= 2) cfg->templates_mode = v;
        } else if (strcmp(key, "templates_dir") == 0) {
            if (val[0] != '\0') {
                snprintf(cfg->templates_dir, sizeof(cfg->templates_dir), "%s", val);
            }
        } else if (strcmp(key, "remember_window_geometry") == 0) {
            cfg->remember_window_geometry = parse_bool_str(val);
        } else if (strcmp(key, "window_w") == 0) {
            int v = atoi(val);
            if (v >= 1024 && v <= 7680) cfg->window_w = v;
        } else if (strcmp(key, "window_h") == 0) {
            int v = atoi(val);
            if (v >= 680 && v <= 4320) cfg->window_h = v;
        } else if (strcmp(key, "window_maximized") == 0) {
            cfg->window_maximized = parse_bool_str(val);
        } else if (strcmp(key, "split_ratio") == 0) {
            float v = (float)atof(val);
            if (v >= 0.24f && v <= 0.68f) cfg->split_ratio = v;
        } else if (strcmp(key, "vsync_enabled") == 0) {
            cfg->vsync_enabled = parse_bool_str(val);
        } else if (strcmp(key, "target_fps") == 0) {
            int v = atoi(val);
            if (v >= 30 && v <= 240) cfg->target_fps = v;
        } else if (strcmp(key, "confirm_unsaved_changes") == 0) {
            cfg->confirm_unsaved_changes = parse_bool_str(val);
        } else if (strcmp(key, "last_open_file") == 0) {
            snprintf(cfg->last_open_file, sizeof(cfg->last_open_file), "%s", val);
        } else if (strcmp(key, "last_selected_template") == 0) {
            int v = atoi(val);
            if (v >= 0 && v < MAX_DYNAMIC_TEMPLATES) cfg->last_selected_template = v;
        } else if (strncmp(key, "recent_", 7) == 0) {
            if (val[0] != '\0' && cfg->recent_count < MAX_RECENT_FILES && access(val, R_OK) == 0) {
                snprintf(cfg->recent_files[cfg->recent_count], sizeof(cfg->recent_files[0]), "%s", val);
                cfg->recent_count++;
            }
        }
    }

    fclose(fp);
    ensure_dir_exists(cfg->templates_dir);
    return true;
}

bool config_save(const AppConfig *cfg)
{
    if (!cfg) return false;
    char dir[480];
    config_get_dir_path(dir, sizeof(dir));
    ensure_dir_exists(dir);
    ensure_dir_exists(cfg->templates_dir);

    char ini_path[512];
    config_get_ini_path(ini_path, sizeof(ini_path));
    FILE *fp = fopen(ini_path, "w");
    if (!fp) return false;

    fprintf(fp, "# AVORA v%s — Persistent Desktop Configuration\n\n", AVORA_VERSION);

    fprintf(fp, "[Appearance]\n");
    fprintf(fp, "language = %d\n", cfg->language);
    fprintf(fp, "theme_id = %d\n", cfg->theme_id);
    fprintf(fp, "canvas_bg_style = %d\n", cfg->canvas_bg_style);
    fprintf(fp, "reduce_animations = %d\n", cfg->reduce_animations ? 1 : 0);
    fprintf(fp, "show_memory_addresses = %d\n", cfg->show_memory_addresses ? 1 : 0);
    fprintf(fp, "show_hud = %d\n", cfg->show_hud ? 1 : 0);
    fprintf(fp, "show_blob = %d\n", cfg->show_blob ? 1 : 0);
    fprintf(fp, "presentation_mode = %d\n", cfg->presentation_mode ? 1 : 0);
    fprintf(fp, "audio_enabled = %d\n", cfg->audio_enabled ? 1 : 0);
    fprintf(fp, "audio_volume = %.3f\n\n", cfg->audio_volume);

    fprintf(fp, "[Editor]\n");
    fprintf(fp, "editor_font_size = %d\n", cfg->editor_font_size);
    fprintf(fp, "tab_size = %d\n", cfg->tab_size);
    fprintf(fp, "show_line_numbers = %d\n", cfg->show_line_numbers ? 1 : 0);
    fprintf(fp, "highlight_current_line = %d\n", cfg->highlight_current_line ? 1 : 0);
    fprintf(fp, "auto_indent = %d\n", cfg->auto_indent ? 1 : 0);
    fprintf(fp, "restore_last_session = %d\n\n", cfg->restore_last_session ? 1 : 0);

    fprintf(fp, "[Debugger]\n");
    fprintf(fp, "asan_enabled = %d\n", cfg->asan_enabled ? 1 : 0);
    fprintf(fp, "watchdog_timeout_sec = %.2f\n", cfg->watchdog_timeout_sec);
    fprintf(fp, "default_speed_t = %.4f\n", cfg->default_speed_t);
    fprintf(fp, "auto_open_terminal_on_stdout = %d\n", cfg->auto_open_terminal_on_stdout ? 1 : 0);
    fprintf(fp, "start_paused_on_main = %d\n", cfg->start_paused_on_main ? 1 : 0);
    fprintf(fp, "enable_stdin = %d\n\n", cfg->enable_stdin ? 1 : 0);

    fprintf(fp, "[Templates]\n");
    fprintf(fp, "templates_mode = %d\n", cfg->templates_mode);
    fprintf(fp, "templates_dir = %s\n\n", cfg->templates_dir);

    fprintf(fp, "[Window]\n");
    fprintf(fp, "remember_window_geometry = %d\n", cfg->remember_window_geometry ? 1 : 0);
    fprintf(fp, "window_w = %d\n", cfg->window_w);
    fprintf(fp, "window_h = %d\n", cfg->window_h);
    fprintf(fp, "window_maximized = %d\n", cfg->window_maximized ? 1 : 0);
    fprintf(fp, "split_ratio = %.4f\n", cfg->split_ratio);
    fprintf(fp, "vsync_enabled = %d\n", cfg->vsync_enabled ? 1 : 0);
    fprintf(fp, "target_fps = %d\n", cfg->target_fps);
    fprintf(fp, "confirm_unsaved_changes = %d\n\n", cfg->confirm_unsaved_changes ? 1 : 0);

    fprintf(fp, "[Session]\n");
    fprintf(fp, "last_open_file = %s\n", cfg->last_open_file);
    fprintf(fp, "last_selected_template = %d\n", cfg->last_selected_template);
    for (int i = 0; i < cfg->recent_count && i < MAX_RECENT_FILES; i++) {
        fprintf(fp, "recent_%d = %s\n", i, cfg->recent_files[i]);
    }

    fclose(fp);
    return true;
}

void config_add_recent_file(AppConfig *cfg, const char *file_path)
{
    if (!cfg || !file_path || file_path[0] == '\0') return;

    char abs_path[512];
    if (!realpath(file_path, abs_path)) {
        snprintf(abs_path, sizeof(abs_path), "%s", file_path);
    }

    int existing = -1;
    for (int i = 0; i < cfg->recent_count; i++) {
        if (strcmp(cfg->recent_files[i], abs_path) == 0) {
            existing = i;
            break;
        }
    }

    if (existing == 0) return;

    int move_count = (existing > 0) ? existing :
                     ((cfg->recent_count < MAX_RECENT_FILES) ? cfg->recent_count : (MAX_RECENT_FILES - 1));

    for (int i = move_count; i > 0; i--) {
        snprintf(cfg->recent_files[i], sizeof(cfg->recent_files[i]), "%s", cfg->recent_files[i - 1]);
    }
    snprintf(cfg->recent_files[0], sizeof(cfg->recent_files[0]), "%s", abs_path);
    if (existing < 0 && cfg->recent_count < MAX_RECENT_FILES) {
        cfg->recent_count++;
    }
    config_save(cfg);
}

/* ============================================================================
 * 3. MONKEYTYPE-INSPIRED THEME PALETTES & RAYGUI LIVE BINDING
 * ============================================================================ */

ThemePalette theme_get_palette(int theme_id)
{
    ThemePalette p;
    memset(&p, 0, sizeof(p));

    switch (theme_id) {
        case THEME_SERIKA_DARK:
            /* 1. Serika Dark — Monkeytype Flagship (#323437 / #E2B714 / #D1D0C5) */
            p.name              = "Serika Dark";
            p.is_light          = false;
            p.bg_canvas         = (Color){ 40,  42,  45,  255 }; /* #282A2D */
            p.canvas_grid       = (Color){ 62,  65,  69,  255 }; /* #3E4145 */
            p.bg_panel          = (Color){ 50,  52,  55,  255 }; /* #323437 */
            p.bg_gutter         = (Color){ 44,  46,  49,  255 }; /* #2C2E31 */
            p.bg_elevated       = (Color){ 60,  62,  66,  255 }; /* #3C3E42 */
            p.bg_hover          = (Color){ 74,  77,  82,  255 }; /* #4A4D52 */
            p.border            = (Color){ 78,  81,  86,  255 }; /* #4E5156 */
            p.text_primary      = (Color){ 209, 208, 197, 255 }; /* #D1D0C5 */
            p.text_secondary    = (Color){ 125, 127, 130, 255 }; /* #7D7F82 */
            p.accent_primary    = (Color){ 226, 183, 20,  255 }; /* #E2B714 Monkeytype Gold */
            p.accent_primary_fg = (Color){ 32,  34,  37,  255 };
            p.accent_secondary  = (Color){ 226, 183, 20,  255 }; /* Gold left/focus */
            p.accent_tertiary   = (Color){ 136, 192, 208, 255 }; /* Soft cyan right */
            p.accent_warning    = (Color){ 245, 158, 11,  255 };
            p.accent_error      = (Color){ 202, 71,  84,  255 }; /* #CA4754 */
            p.syntax_keyword    = (Color){ 226, 183, 20,  255 };
            p.syntax_type       = (Color){ 136, 192, 208, 255 };
            p.syntax_func       = (Color){ 240, 210, 100, 255 };
            p.syntax_string     = (Color){ 163, 190, 140, 255 };
            p.syntax_number     = (Color){ 208, 135, 112, 255 };
            p.syntax_comment    = (Color){ 110, 112, 115, 255 };
            break;

        case THEME_CATPPUCCIN_MOCHA:
            /* 2. Catppuccin Mocha (#1E1E2E / #313244 / #CBA6F7 / #89B4FA) */
            p.name              = "Catppuccin Mocha";
            p.is_light          = false;
            p.bg_canvas         = (Color){ 24,  24,  37,  255 }; /* #181825 Mantle */
            p.canvas_grid       = (Color){ 49,  50,  68,  255 }; /* #313244 Surface0 */
            p.bg_panel          = (Color){ 30,  30,  46,  255 }; /* #1E1E2E Base */
            p.bg_gutter         = (Color){ 24,  24,  37,  255 }; /* #181825 */
            p.bg_elevated       = (Color){ 49,  50,  68,  255 }; /* #313244 */
            p.bg_hover          = (Color){ 69,  71,  90,  255 }; /* #45475A Surface1 */
            p.border            = (Color){ 88,  91,  112, 255 }; /* #585B70 Surface2 */
            p.text_primary      = (Color){ 205, 214, 244, 255 }; /* #CDD6F4 Text */
            p.text_secondary    = (Color){ 147, 153, 178, 255 }; /* #9399B2 Overlay2 */
            p.accent_primary    = (Color){ 166, 227, 161, 255 }; /* #A6E3A1 Green */
            p.accent_primary_fg = (Color){ 17,  17,  27,  255 }; /* #11111B Crust */
            p.accent_secondary  = (Color){ 137, 180, 250, 255 }; /* #89B4FA Blue */
            p.accent_tertiary   = (Color){ 203, 166, 247, 255 }; /* #CBA6F7 Mauve */
            p.accent_warning    = (Color){ 249, 226, 175, 255 }; /* #F9E2AF Yellow */
            p.accent_error      = (Color){ 243, 139, 168, 255 }; /* #F38BA8 Red */
            p.syntax_keyword    = (Color){ 203, 166, 247, 255 };
            p.syntax_type       = (Color){ 137, 180, 250, 255 };
            p.syntax_func       = (Color){ 249, 226, 175, 255 };
            p.syntax_string     = (Color){ 166, 227, 161, 255 };
            p.syntax_number     = (Color){ 250, 179, 135, 255 };
            p.syntax_comment    = (Color){ 108, 112, 134, 255 };
            break;

        case THEME_NORD:
            /* 3. Nord (#2E3440 / #3B4252 / #88C0D0) */
            p.name              = "Nord";
            p.is_light          = false;
            p.bg_canvas         = (Color){ 36,  41,  51,  255 }; /* #242933 */
            p.canvas_grid       = (Color){ 59,  66,  82,  255 }; /* #3B4252 */
            p.bg_panel          = (Color){ 46,  52,  64,  255 }; /* #2E3440 */
            p.bg_gutter         = (Color){ 39,  44,  54,  255 };
            p.bg_elevated       = (Color){ 59,  66,  82,  255 }; /* #3B4252 */
            p.bg_hover          = (Color){ 67,  76,  94,  255 }; /* #434C5E */
            p.border            = (Color){ 76,  86,  106, 255 }; /* #4C566A */
            p.text_primary      = (Color){ 236, 239, 244, 255 }; /* #ECEFF4 */
            p.text_secondary    = (Color){ 143, 155, 179, 255 };
            p.accent_primary    = (Color){ 136, 192, 208, 255 }; /* #88C0D0 Frost Cyan */
            p.accent_primary_fg = (Color){ 36,  41,  51,  255 };
            p.accent_secondary  = (Color){ 136, 192, 208, 255 }; /* #88C0D0 */
            p.accent_tertiary   = (Color){ 180, 142, 173, 255 }; /* #B48EAD Aurora Purple */
            p.accent_warning    = (Color){ 235, 203, 139, 255 }; /* #EBCB8B Aurora Yellow */
            p.accent_error      = (Color){ 191, 97,  106, 255 }; /* #BF616A Aurora Red */
            p.syntax_keyword    = (Color){ 129, 161, 193, 255 };
            p.syntax_type       = (Color){ 143, 188, 187, 255 };
            p.syntax_func       = (Color){ 136, 192, 208, 255 };
            p.syntax_string     = (Color){ 163, 190, 140, 255 };
            p.syntax_number     = (Color){ 180, 142, 173, 255 };
            p.syntax_comment    = (Color){ 108, 122, 147, 255 };
            break;

        case THEME_GRUVBOX_DARK:
            /* 4. Gruvbox Dark (#282828 / #3C3836 / #FABD2F / #B8BB26) */
            p.name              = "Gruvbox Dark";
            p.is_light          = false;
            p.bg_canvas         = (Color){ 29,  32,  33,  255 }; /* #1D2021 Hard Dark */
            p.canvas_grid       = (Color){ 60,  56,  54,  255 }; /* #3C3836 */
            p.bg_panel          = (Color){ 40,  40,  40,  255 }; /* #282828 */
            p.bg_gutter         = (Color){ 32,  32,  32,  255 };
            p.bg_elevated       = (Color){ 60,  56,  54,  255 }; /* #3C3836 */
            p.bg_hover          = (Color){ 80,  73,  69,  255 }; /* #504945 */
            p.border            = (Color){ 102, 92,  84,  255 }; /* #665C54 */
            p.text_primary      = (Color){ 235, 219, 178, 255 }; /* #EBDBB2 */
            p.text_secondary    = (Color){ 168, 153, 132, 255 }; /* #A89984 */
            p.accent_primary    = (Color){ 184, 187, 38,  255 }; /* #B8BB26 Gruvbox Green */
            p.accent_primary_fg = (Color){ 29,  32,  33,  255 };
            p.accent_secondary  = (Color){ 131, 165, 152, 255 }; /* #83A598 Aqua */
            p.accent_tertiary   = (Color){ 211, 134, 155, 255 }; /* #D3869B Purple */
            p.accent_warning    = (Color){ 250, 189, 47,  255 }; /* #FABD2F Yellow */
            p.accent_error      = (Color){ 251, 73,  52,  255 }; /* #FB4934 Red */
            p.syntax_keyword    = (Color){ 251, 73,  52,  255 };
            p.syntax_type       = (Color){ 250, 189, 47,  255 };
            p.syntax_func       = (Color){ 184, 187, 38,  255 };
            p.syntax_string     = (Color){ 184, 187, 38,  255 };
            p.syntax_number     = (Color){ 211, 134, 155, 255 };
            p.syntax_comment    = (Color){ 146, 131, 116, 255 };
            break;

        case THEME_DRACULA:
            /* 5. Dracula (#282A36 / #44475A / #BD93F9 / #50FA7B) */
            p.name              = "Dracula";
            p.is_light          = false;
            p.bg_canvas         = (Color){ 30,  31,  41,  255 }; /* #1E1F29 */
            p.canvas_grid       = (Color){ 68,  71,  90,  255 }; /* #44475A */
            p.bg_panel          = (Color){ 40,  42,  54,  255 }; /* #282A36 */
            p.bg_gutter         = (Color){ 33,  34,  44,  255 }; /* #21222C */
            p.bg_elevated       = (Color){ 56,  58,  74,  255 };
            p.bg_hover          = (Color){ 68,  71,  90,  255 }; /* #44475A */
            p.border            = (Color){ 98,  114, 164, 255 }; /* #6272A4 */
            p.text_primary      = (Color){ 248, 248, 242, 255 }; /* #F8F8F2 */
            p.text_secondary    = (Color){ 139, 152, 199, 255 };
            p.accent_primary    = (Color){ 80,  250, 123, 255 }; /* #50FA7B Green */
            p.accent_primary_fg = (Color){ 30,  31,  41,  255 };
            p.accent_secondary  = (Color){ 139, 233, 253, 255 }; /* #8BE9FD Cyan */
            p.accent_tertiary   = (Color){ 189, 147, 249, 255 }; /* #BD93F9 Purple */
            p.accent_warning    = (Color){ 255, 184, 108, 255 }; /* #FFB86C Orange */
            p.accent_error      = (Color){ 255, 85,  85,  255 }; /* #FF5555 Red */
            p.syntax_keyword    = (Color){ 255, 121, 198, 255 };
            p.syntax_type       = (Color){ 139, 233, 253, 255 };
            p.syntax_func       = (Color){ 80,  250, 123, 255 };
            p.syntax_string     = (Color){ 241, 250, 140, 255 };
            p.syntax_number     = (Color){ 189, 147, 249, 255 };
            p.syntax_comment    = (Color){ 98,  114, 164, 255 };
            break;

        case THEME_SERIKA_LIGHT:
            /* 6. Serika Light / Paper — High-Contrast Classroom Projector & Report Theme */
            p.name              = "Serika Light (Paper)";
            p.is_light          = true;
            p.bg_canvas         = (Color){ 245, 245, 240, 255 }; /* #F5F5F0 Warm Paper */
            p.canvas_grid       = (Color){ 214, 213, 204, 255 }; /* #D6D5CC */
            p.bg_panel          = (Color){ 235, 234, 226, 255 }; /* #EBEAE2 */
            p.bg_gutter         = (Color){ 225, 224, 214, 255 }; /* #E1E0D6 */
            p.bg_elevated       = (Color){ 255, 255, 252, 255 }; /* #FFFFFC Crisp Node/Button */
            p.bg_hover          = (Color){ 220, 219, 208, 255 }; /* #DCDBD0 */
            p.border            = (Color){ 188, 186, 174, 255 }; /* #BCBWAE */
            p.text_primary      = (Color){ 36,  41,  47,  255 }; /* #24292F Deep Ink */
            p.text_secondary    = (Color){ 100, 106, 115, 255 }; /* #646A73 */
            p.accent_primary    = (Color){ 5,   150, 105, 255 }; /* #059669 Deep Emerald */
            p.accent_primary_fg = (Color){ 255, 255, 255, 255 };
            p.accent_secondary  = (Color){ 2,   132, 199, 255 }; /* #0284C7 Strong Sky Blue */
            p.accent_tertiary   = (Color){ 124, 58,  237, 255 }; /* #7C3AED Deep Violet */
            p.accent_warning    = (Color){ 217, 119, 6,   255 }; /* #D97706 Rich Amber */
            p.accent_error      = (Color){ 220, 38,  38,  255 }; /* #DC2626 Crimson */
            p.syntax_keyword    = (Color){ 190, 24,  93,  255 };
            p.syntax_type       = (Color){ 2,   132, 199, 255 };
            p.syntax_func       = (Color){ 180, 83,  9,   255 };
            p.syntax_string     = (Color){ 4,   120, 87,  255 };
            p.syntax_number     = (Color){ 109, 40,  217, 255 };
            p.syntax_comment    = (Color){ 115, 122, 132, 255 };
            break;

        case THEME_MIDNIGHT_SLATE:
        default:
            /* 0. Midnight Slate — Default AVORA v3 Signature Theme */
            p.name              = "Midnight Slate";
            p.is_light          = false;
            p.bg_canvas         = (Color){ 15,  17,  26,  255 }; /* #0F111A */
            p.canvas_grid       = (Color){ 30,  34,  51,  255 }; /* #1E2233 */
            p.bg_panel          = (Color){ 22,  25,  37,  255 }; /* #161925 */
            p.bg_gutter         = (Color){ 16,  19,  29,  255 }; /* #10131D */
            p.bg_elevated       = (Color){ 31,  36,  55,  255 }; /* #1F2437 */
            p.bg_hover          = (Color){ 40,  46,  71,  255 }; /* #282E47 */
            p.border            = (Color){ 42,  48,  71,  255 }; /* #2A3047 */
            p.text_primary      = (Color){ 226, 232, 240, 255 }; /* #E2E8F0 */
            p.text_secondary    = (Color){ 100, 116, 139, 255 }; /* #64748B */
            p.accent_primary    = (Color){ 16,  185, 129, 255 }; /* #10B981 Emerald */
            p.accent_primary_fg = (Color){ 15,  17,  26,  255 }; /* #0F111A */
            p.accent_secondary  = (Color){ 56,  189, 248, 255 }; /* #38BDF8 Cyan */
            p.accent_tertiary   = (Color){ 167, 139, 250, 255 }; /* #A78BFA Purple */
            p.accent_warning    = (Color){ 245, 158, 11,  255 }; /* #F59E0B Amber */
            p.accent_error      = (Color){ 244, 63,  94,  255 }; /* #F43F5E Crimson */
            p.syntax_keyword    = (Color){ 244, 114, 182, 255 };
            p.syntax_type       = (Color){ 56,  189, 248, 255 };
            p.syntax_func       = (Color){ 250, 204, 21,  255 };
            p.syntax_string     = (Color){ 52,  211, 153, 255 };
            p.syntax_number     = (Color){ 167, 139, 250, 255 };
            p.syntax_comment    = (Color){ 100, 116, 139, 255 };
            break;
    }

    return p;
}

static int color_to_hex_int(Color c)
{
    unsigned int u = ((unsigned int)c.r << 24) |
                     ((unsigned int)c.g << 16) |
                     ((unsigned int)c.b << 8)  |
                     ((unsigned int)c.a);
    return (int)u;
}

void theme_apply_to_raygui(const ThemePalette *pal)
{
    if (!pal) return;
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR,      color_to_hex_int(pal->bg_panel));
    GuiSetStyle(DEFAULT, LINE_COLOR,            color_to_hex_int(pal->border));
    GuiSetStyle(DEFAULT, BASE_COLOR_NORMAL,     color_to_hex_int(pal->bg_elevated));
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,   color_to_hex_int(pal->border));
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,     color_to_hex_int(pal->text_primary));
    GuiSetStyle(DEFAULT, BASE_COLOR_FOCUSED,    color_to_hex_int(pal->bg_hover));
    GuiSetStyle(DEFAULT, BORDER_COLOR_FOCUSED,  color_to_hex_int(pal->accent_secondary));
    GuiSetStyle(DEFAULT, TEXT_COLOR_FOCUSED,    color_to_hex_int(pal->text_primary));
    GuiSetStyle(DEFAULT, BASE_COLOR_PRESSED,    color_to_hex_int(pal->accent_primary));
    GuiSetStyle(DEFAULT, BORDER_COLOR_PRESSED,  color_to_hex_int(pal->accent_primary));
    GuiSetStyle(DEFAULT, TEXT_COLOR_PRESSED,    color_to_hex_int(pal->accent_primary_fg));
    GuiSetStyle(DEFAULT, BASE_COLOR_DISABLED,   color_to_hex_int(pal->bg_gutter));
    GuiSetStyle(DEFAULT, BORDER_COLOR_DISABLED, color_to_hex_int(pal->border));
    GuiSetStyle(DEFAULT, TEXT_COLOR_DISABLED,   color_to_hex_int(pal->text_secondary));
}

/* ============================================================================
 * 4. DYNAMIC FOLDER-BASED TEMPLATES ENGINE (CLASSROOM / AED1 SUPPORT)
 * ============================================================================ */

static bool has_c_extension(const char *name)
{
    if (!name) return false;
    size_t len = strlen(name);
    return (len > 2 && strcasecmp(name + len - 2, ".c") == 0);
}

static int compare_template_entries(const void *a, const void *b)
{
    const TemplateEntry *ta = (const TemplateEntry *)a;
    const TemplateEntry *tb = (const TemplateEntry *)b;
    return strcasecmp(ta->display_name, tb->display_name);
}

static void scan_folder_for_c_templates(
    const char    *base_dir,
    const char    *sub_dir,
    TemplateEntry *out_list,
    int           *inout_count,
    int            max_items
)
{
    if (!base_dir || !out_list || !inout_count || *inout_count >= max_items) return;

    char full_dir[1024];
    if (sub_dir && sub_dir[0] != '\0') {
        snprintf(full_dir, sizeof(full_dir), "%s/%s", base_dir, sub_dir);
    } else {
        snprintf(full_dir, sizeof(full_dir), "%s", base_dir);
    }

    DIR *dir = opendir(full_dir);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && *inout_count < max_items) {
        if (ent->d_name[0] == '.') continue;

        char item_path[1400];
        snprintf(item_path, sizeof(item_path), "%.1000s/%.255s", full_dir, ent->d_name);

        struct stat st;
        if (stat(item_path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            /* Scan 1 level of subdirectories (e.g., Aula01/, Semana05/) */
            if (!sub_dir || sub_dir[0] == '\0') {
                scan_folder_for_c_templates(base_dir, ent->d_name, out_list, inout_count, max_items);
            }
        } else if (S_ISREG(st.st_mode) && has_c_extension(ent->d_name)) {
            TemplateEntry *te = &out_list[*inout_count];
            memset(te, 0, sizeof(*te));
            te->is_builtin    = false;
            te->builtin_index = -1;
            snprintf(te->file_path, sizeof(te->file_path), "%.511s", item_path);

            if (sub_dir && sub_dir[0] != '\0') {
                snprintf(te->display_name, sizeof(te->display_name), "[%s] %s", sub_dir, ent->d_name);
            } else {
                snprintf(te->display_name, sizeof(te->display_name), "[Pasta] %s", ent->d_name);
            }
            /* Sanitize ';' because RayGUI dropdown uses ';' as separator */
            for (char *c = te->display_name; *c; c++) {
                if (*c == ';') *c = ':';
            }
            (*inout_count)++;
        }
    }

    closedir(dir);
}

void templates_refresh_list(WorkspaceUI *ui)
{
    if (!ui) return;
    ensure_dir_exists(ui->config.templates_dir);

    ui->template_count = 0;

    /* 1. Add Built-in 18 Templates when mode == 0 (Embutidos + Pasta) or mode == 2 (Apenas Embutidos) */
    if (ui->config.templates_mode == 0 || ui->config.templates_mode == 2) {
        for (int i = 0; i < PRESET_COUNT && ui->template_count < MAX_DYNAMIC_TEMPLATES; i++) {
            const char *dname = NULL;
            const char *fname = NULL;
            if (get_builtin_preset_info(i, &dname, &fname) != NULL && dname) {
                TemplateEntry *te = &ui->templates[ui->template_count++];
                memset(te, 0, sizeof(*te));
                snprintf(te->display_name, sizeof(te->display_name), "%s", dname);
                te->file_path[0]  = '\0';
                te->is_builtin    = true;
                te->builtin_index = i;
            }
        }
    }

    /* 2. Scan User/Professor Templates Folder when mode == 0 or mode == 1 (Apenas Pasta) */
    if (ui->config.templates_mode == 0 || ui->config.templates_mode == 1) {
        int folder_start = ui->template_count;
        scan_folder_for_c_templates(
            ui->config.templates_dir,
            NULL,
            ui->templates,
            &ui->template_count,
            MAX_DYNAMIC_TEMPLATES
        );
        int folder_items = ui->template_count - folder_start;
        if (folder_items > 1) {
            qsort(&ui->templates[folder_start], (size_t)folder_items, sizeof(TemplateEntry), compare_template_entries);
        }
    }

    /* 3. Fallback if mode == 1 (Apenas Pasta) and the folder is currently empty */
    if (ui->template_count == 0) {
        const char *dname = NULL;
        get_builtin_preset_info(0, &dname, NULL);
        TemplateEntry *te = &ui->templates[ui->template_count++];
        memset(te, 0, sizeof(*te));
        snprintf(te->display_name, sizeof(te->display_name), "0. Vazio (Pasta de Templates Vazia)");
        te->is_builtin    = true;
        te->builtin_index = 0;
    }

    if (ui->active_template_idx < 0 || ui->active_template_idx >= ui->template_count) {
        ui->active_template_idx = (ui->template_count > 1) ? 1 : 0;
    }
}

int templates_export_builtins_to_folder(const char *folder_path)
{
    if (!folder_path || folder_path[0] == '\0') return 0;
    ensure_dir_exists(folder_path);

    int exported = 0;
    for (int i = 0; i < PRESET_COUNT; i++) {
        const char *dname = NULL;
        const char *fname = NULL;
        const char *code  = get_builtin_preset_info(i, &dname, &fname);
        if (!code || !fname) continue;

        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, fname);

        /* Do not overwrite files already customized by the user/professor */
        if (access(full_path, F_OK) == 0) continue;

        FILE *fp = fopen(full_path, "w");
        if (fp) {
            fputs(code, fp);
            fclose(fp);
            exported++;
        }
    }
    return exported;
}

/* ============================================================================
 * 5. NATIVE LINUX FILE DIALOGS (ZENITY / KDIALOG) & FILE MANAGER OPENER
 * ============================================================================ */

void platform_open_folder_in_file_manager(const char *folder_path)
{
    if (!folder_path || folder_path[0] == '\0') return;
    ensure_dir_exists(folder_path);

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "xdg-open \"%s\" >/dev/null 2>&1 &", folder_path);
    int rc = system(cmd);
    (void)rc;
}

bool platform_native_file_dialog(
    int         mode, /* 0 = Open .c, 1 = Save .c, 2 = Pick Folder, 3 = Save .png */
    const char *initial_path,
    char       *out_path,
    size_t      out_max
)
{
    if (!out_path || out_max == 0) return false;
    out_path[0] = '\0';

    bool has_zenity  = (access("/usr/bin/zenity", X_OK) == 0);
    bool has_kdialog = (!has_zenity && access("/usr/bin/kdialog", X_OK) == 0);
    if (!has_zenity && !has_kdialog) {
        return false; /* Caller will open RayGUI internal path input modal */
    }

    const char *init = (initial_path && initial_path[0] != '\0') ? initial_path : "";
    char cmd[1200];

    if (has_zenity) {
        if (mode == 0) {
            snprintf(cmd, sizeof(cmd),
                     "zenity --file-selection --title=\"AVORA — Abrir Arquivo C\" "
                     "--file-filter=\"Código C (*.c) | *.c\" --file-filter=\"Todos | *\" "
                     "%s%s%s 2>/dev/null",
                     init[0] ? "--filename=\"" : "", init, init[0] ? "\"" : "");
        } else if (mode == 1) {
            snprintf(cmd, sizeof(cmd),
                     "zenity --file-selection --save --confirm-overwrite "
                     "--title=\"AVORA — Salvar Arquivo C\" "
                     "--file-filter=\"Código C (*.c) | *.c\" "
                     "--filename=\"%s\" 2>/dev/null",
                     init[0] ? init : "algoritmo.c");
        } else if (mode == 2) {
            snprintf(cmd, sizeof(cmd),
                     "zenity --file-selection --directory "
                     "--title=\"AVORA — Escolher Pasta de Templates\" "
                     "--filename=\"%s/\" 2>/dev/null",
                     init[0] ? init : ".");
        } else {
            snprintf(cmd, sizeof(cmd),
                     "zenity --file-selection --save --confirm-overwrite "
                     "--title=\"AVORA — Exportar Canvas como PNG\" "
                     "--file-filter=\"Imagem PNG (*.png) | *.png\" "
                     "--filename=\"%s\" 2>/dev/null",
                     init[0] ? init : "avora_canvas.png");
        }
    } else {
        const char *start_dir = init[0] ? init : ".";
        if (mode == 0) {
            snprintf(cmd, sizeof(cmd), "kdialog --getopenfilename \"%s\" \"*.c|Código C (*.c)\" 2>/dev/null", start_dir);
        } else if (mode == 1) {
            snprintf(cmd, sizeof(cmd), "kdialog --getsavefilename \"%s\" \"*.c|Código C (*.c)\" 2>/dev/null", start_dir);
        } else if (mode == 2) {
            snprintf(cmd, sizeof(cmd), "kdialog --getexistingdirectory \"%s\" 2>/dev/null", start_dir);
        } else {
            snprintf(cmd, sizeof(cmd), "kdialog --getsavefilename \"%s\" \"*.png|Imagem PNG (*.png)\" 2>/dev/null", start_dir);
        }
    }

    FILE *pipe = popen(cmd, "r");
    if (!pipe) return false;

    char buf[1024] = { 0 };
    if (fgets(buf, sizeof(buf), pipe) != NULL) {
        size_t len = strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
    }
    int status = pclose(pipe);
    if (status == 0 && buf[0] != '\0') {
        snprintf(out_path, out_max, "%s", buf);
        return true;
    }
    return false;
}
