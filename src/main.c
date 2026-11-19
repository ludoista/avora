#include "visualizer.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void apply_avora_window_icon(void)
{
    char home_png[512] = { 0 };
    char home_jpg[512] = { 0 };
    const char *home = getenv("HOME");
    if (home && home[0] != '\0') {
        snprintf(home_png, sizeof(home_png), "%s/.local/share/avora/avora.png", home);
        snprintf(home_jpg, sizeof(home_jpg), "%s/.local/share/avora/logo.jpg", home);
    }

    const char *candidates[] = {
        home_png[0] ? home_png : "logo.jpg",
        "/usr/local/share/avora/avora.png",
        "/usr/share/pixmaps/avora.png",
        "./logo.jpg",
        "logo.jpg",
        home_jpg[0] ? home_jpg : "logo.jpg",
        "/usr/local/share/avora/logo.jpg",
        "/usr/share/avora/logo.jpg",
        NULL
    };

    for (int i = 0; candidates[i] != NULL; i++) {
        if (access(candidates[i], R_OK) == 0) {
            Image icon = LoadImage(candidates[i]);
            if (icon.data != NULL) {
                /* GLFW/X11 requires 32-bit RGBA format for SetWindowIcon */
                ImageFormat(&icon, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
                if (icon.width != 128 || icon.height != 128) {
                    ImageResize(&icon, 128, 128);
                }
                SetWindowIcon(icon);
                UnloadImage(icon);
                break;
            }
        }
    }
}

int main(void)
{
    /* Ignore SIGPIPE so writing to a closed GDB pipe never terminates the UI */
    signal(SIGPIPE, SIG_IGN);

    DebuggerSession *session = (DebuggerSession *)calloc(1, sizeof(DebuggerSession));
    WorkspaceUI     *ui      = (WorkspaceUI *)calloc(1, sizeof(WorkspaceUI));
    if (!session || !ui) {
        free(session);
        free(ui);
        return 1;
    }

    /* Load persistent configuration from ~/.config/avora/config.ini before window creation */
    config_reset_defaults(&ui->config);
    config_load(&ui->config);

    unsigned int win_flags = FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT;
    if (ui->config.vsync_enabled) {
        win_flags |= FLAG_VSYNC_HINT;
    }
    SetConfigFlags(win_flags);

    int init_w = (ui->config.remember_window_geometry && ui->config.window_w >= 1024) ? ui->config.window_w : 1440;
    int init_h = (ui->config.remember_window_geometry && ui->config.window_h >= 680)  ? ui->config.window_h : 900;

    InitWindow(init_w, init_h, AVORA_WINDOW_TITLE);
    SetWindowMinSize(1024, 680);
    apply_avora_window_icon();

    if (ui->config.remember_window_geometry && ui->config.window_maximized) {
        MaximizeWindow();
    }

    SetTargetFPS(ui->config.target_fps > 0 ? ui->config.target_fps : 60);
    SetExitKey(KEY_NULL);

    audio_engine_init();
    session_init(session);
    ui_workspace_init(ui, session);

    while (!ui->request_exit) {
        if (WindowShouldClose()) {
            if (ui->is_dirty && ui->config.confirm_unsaved_changes && !ui->modal_unsaved_confirm_open) {
                ui->modal_unsaved_confirm_open = true;
                ui->pending_action_type        = 5; /* 5 = Quit application */
            } else {
                break;
            }
        }

        BeginDrawing();
        ClearBackground(ui->theme.bg_canvas);

        ui_workspace_update_and_draw(session, ui);

        EndDrawing();
    }

    /* Persist final window geometry & split ratio on clean shutdown */
    ui->config.window_maximized = IsWindowMaximized();
    if (!ui->config.window_maximized && !IsWindowFullscreen()) {
        int cur_w = GetScreenWidth();
        int cur_h = GetScreenHeight();
        if (cur_w >= 1024 && cur_h >= 680) {
            ui->config.window_w = cur_w;
            ui->config.window_h = cur_h;
        }
    }
    ui->config.split_ratio            = ui->split_vertical_ratio;
    ui->config.last_selected_template = session->selected_preset;
    snprintf(ui->config.last_open_file, sizeof(ui->config.last_open_file), "%s", ui->current_file_path);
    config_save(&ui->config);

    ui_workspace_Unload(ui);
    session_cleanup(session);
    audio_engine_cleanup();
    free(ui);
    free(session);

    CloseWindow();
    return 0;
}
