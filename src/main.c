#include "visualizer.h"
#include <signal.h>
#include <stdlib.h>

int main(void)
{
    /* Ignore SIGPIPE so writing to a closed GDB pipe never terminates the UI */
    signal(SIGPIPE, SIG_IGN);

    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(1440, 900, AVORA_WINDOW_TITLE);
    SetWindowMinSize(1024, 680);
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);

    DebuggerSession *session = (DebuggerSession *)calloc(1, sizeof(DebuggerSession));
    WorkspaceUI     *ui      = (WorkspaceUI *)calloc(1, sizeof(WorkspaceUI));
    if (!session || !ui) {
        free(session);
        free(ui);
        CloseWindow();
        return 1;
    }

    session_init(session);
    ui_workspace_init(ui, session);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){ 16, 16, 18, 255 });

        ui_workspace_update_and_draw(session, ui);

        EndDrawing();
    }

    ui_workspace_Unload(ui);
    session_cleanup(session);
    free(ui);
    free(session);

    CloseWindow();
    return 0;
}
