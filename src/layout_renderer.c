#include "visualizer.h"
#include "raygui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#define NODE_RADIUS         26.0f
#define LEVEL_VERTICAL_GAP  92.0f
#define MIN_HORIZONTAL_GAP  76.0f
#define FOREST_TREE_GAP     95.0f
#define STAGING_SLOT_GAP    115.0f

/* ============================================================================
 * 1. CAMERA INITIALIZATION & CENTERING
 * ============================================================================ */

void layout_init_camera(WorkspaceUI *ui, Rectangle canvas_bounds)
{
    if (!ui) return;
    ui->camera.offset   = (Vector2){ canvas_bounds.x + canvas_bounds.width * 0.5f, canvas_bounds.y + 135.0f };
    ui->camera.target   = (Vector2){ 0.0f, 110.0f };
    ui->camera.rotation = 0.0f;
    ui->camera.zoom     = 1.0f;
    ui->is_panning      = false;
}

void layout_center_camera(WorkspaceUI *ui, const TreeSnapshot *snapshot, Rectangle canvas_bounds)
{
    if (!ui) return;
    ui->camera.offset = (Vector2){ canvas_bounds.x + canvas_bounds.width * 0.5f, canvas_bounds.y + 135.0f };
    ui->camera.zoom   = 1.0f;

    if (snapshot && snapshot->node_count > 0) {
        /* Compute centroid X of all non-detached root nodes */
        float sum_x = 0.0f;
        int   cnt   = 0;
        for (int r = 0; r < snapshot->forest_root_count; r++) {
            for (int i = 0; i < snapshot->node_count; i++) {
                if (snapshot->nodes[i].addr == snapshot->forest_roots[r]) {
                    sum_x += snapshot->nodes[i].target_pos.x;
                    cnt++;
                }
            }
        }
        if (cnt > 0) {
            ui->camera.target = (Vector2){ sum_x / (float)cnt, 55.0f };
            return;
        }
    }
    ui->camera.target = (Vector2){ 0.0f, 55.0f };
}

/* ============================================================================
 * 2. HIERARCHICAL NON-OVERLAPPING TREE & MULTI-TREE FOREST LAYOUT ENGINE
 * ============================================================================ */

static int find_node_index_by_addr(const TreeSnapshot *snapshot, uint64_t addr)
{
    if (addr == 0) return -1;
    for (int i = 0; i < snapshot->node_count; i++) {
        if (snapshot->nodes[i].addr == addr) {
            return i;
        }
    }
    return -1;
}

static float compute_subtree_width_rec(
    TreeSnapshot *snapshot,
    int           node_idx,
    bool         *visited,
    int           depth
)
{
    if (node_idx < 0 || node_idx >= snapshot->node_count) return 0.0f;
    if (visited[node_idx] || depth > 64) return MIN_HORIZONTAL_GAP;

    visited[node_idx] = true;
    VisualNode *n = &snapshot->nodes[node_idx];
    n->tree_depth = depth;

    float left_w  = 0.0f;
    float right_w = 0.0f;

    if (n->left_state == EDGE_OK && n->left_addr != 0) {
        int l_idx = find_node_index_by_addr(snapshot, n->left_addr);
        if (l_idx >= 0 && !visited[l_idx]) {
            left_w = compute_subtree_width_rec(snapshot, l_idx, visited, depth + 1);
        }
    }

    if (n->right_state == EDGE_OK && n->right_addr != 0) {
        int r_idx = find_node_index_by_addr(snapshot, n->right_addr);
        if (r_idx >= 0 && !visited[r_idx]) {
            right_w = compute_subtree_width_rec(snapshot, r_idx, visited, depth + 1);
        }
    }

    /* Reserve extra horizontal width if node has a garbage or dangling hazard leaf */
    if (n->left_state == EDGE_GARBAGE_PTR || n->left_state == EDGE_DANGLING) {
        left_w = fmaxf(left_w, MIN_HORIZONTAL_GAP * 0.95f);
    }
    if (n->right_state == EDGE_GARBAGE_PTR || n->right_state == EDGE_DANGLING) {
        right_w = fmaxf(right_w, MIN_HORIZONTAL_GAP * 0.95f);
    }

    float total = left_w + right_w;
    if (total < MIN_HORIZONTAL_GAP) {
        total = MIN_HORIZONTAL_GAP;
    } else {
        total += 24.0f;
    }

    n->subtree_width = total;
    return total;
}

static void assign_coordinates_rec(
    TreeSnapshot *snapshot,
    int           node_idx,
    float         center_x,
    float         center_y,
    bool         *visited,
    int           depth
)
{
    if (node_idx < 0 || node_idx >= snapshot->node_count) return;
    if (visited[node_idx] || depth > 64) return;

    visited[node_idx] = true;
    VisualNode *n = &snapshot->nodes[node_idx];
    n->target_pos = (Vector2){ center_x, center_y };

    int l_idx = (n->left_state == EDGE_OK)  ? find_node_index_by_addr(snapshot, n->left_addr)  : -1;
    int r_idx = (n->right_state == EDGE_OK) ? find_node_index_by_addr(snapshot, n->right_addr) : -1;

    float left_w  = (l_idx >= 0 && !visited[l_idx]) ? snapshot->nodes[l_idx].subtree_width : MIN_HORIZONTAL_GAP * 0.65f;
    float right_w = (r_idx >= 0 && !visited[r_idx]) ? snapshot->nodes[r_idx].subtree_width : MIN_HORIZONTAL_GAP * 0.65f;

    if (l_idx >= 0 && !visited[l_idx]) {
        float lx = center_x - (right_w * 0.35f) - (left_w * 0.5f);
        assign_coordinates_rec(snapshot, l_idx, lx, center_y + LEVEL_VERTICAL_GAP, visited, depth + 1);
    }

    if (r_idx >= 0 && !visited[r_idx]) {
        float rx = center_x + (left_w * 0.35f) + (right_w * 0.5f);
        assign_coordinates_rec(snapshot, r_idx, rx, center_y + LEVEL_VERTICAL_GAP, visited, depth + 1);
    }
}

void layout_compute_tree_positions(TreeSnapshot *snapshot, Rectangle canvas_bounds)
{
    if (!snapshot || snapshot->node_count == 0) return;
    (void)canvas_bounds;

    bool visited_w[MAX_TREE_NODES] = { false };
    bool visited_p[MAX_TREE_NODES] = { false };

    /* 1. Compute widths of all trees in the Forest (Subsystem E.3) */
    float tree_widths[MAX_FOREST_ROOTS] = { 0 };
    int   root_indices[MAX_FOREST_ROOTS];
    int   valid_roots = 0;
    float total_forest_w = 0.0f;

    for (int r = 0; r < snapshot->forest_root_count && valid_roots < MAX_FOREST_ROOTS; r++) {
        int r_idx = find_node_index_by_addr(snapshot, snapshot->forest_roots[r]);
        if (r_idx >= 0 && !visited_w[r_idx]) {
            float w = compute_subtree_width_rec(snapshot, r_idx, visited_w, 0);
            root_indices[valid_roots] = r_idx;
            tree_widths[valid_roots]  = w;
            total_forest_w += w;
            valid_roots++;
        }
    }

    if (valid_roots > 1) {
        total_forest_w += (float)(valid_roots - 1) * FOREST_TREE_GAP;
    }

    /* 2. Position each tree side-by-side horizontally centered around X = 0 */
    float cur_left_x = -total_forest_w * 0.5f;
    for (int r = 0; r < valid_roots; r++) {
        float w = tree_widths[r];
        float tree_center_x = cur_left_x + w * 0.5f;
        assign_coordinates_rec(snapshot, root_indices[r], tree_center_x, 45.0f, visited_p, 0);
        cur_left_x += w + FOREST_TREE_GAP;
    }

    /* 3. Place any detached / newly allocated local island nodes in the top-left Staging Zone */
    int staging_slot = 0;
    float stage_base_x = fminf(-340.0f, -total_forest_w * 0.5f - 180.0f);
    for (int i = 0; i < snapshot->node_count; i++) {
        if (!visited_p[i]) {
            VisualNode *n = &snapshot->nodes[i];
            n->is_detached = true;
            float sx = stage_base_x + (float)(staging_slot % 3) * STAGING_SLOT_GAP;
            float sy = -25.0f       + (float)(staging_slot / 3) * 95.0f;
            compute_subtree_width_rec(snapshot, i, visited_w, 0);
            assign_coordinates_rec(snapshot, i, sx, sy, visited_p, 0);
            staging_slot++;
        }
    }
}

void layout_update_animations(TreeSnapshot *snapshot, float dt, float *pulse_timer)
{
    if (pulse_timer) {
        *pulse_timer += dt * 4.0f;
    }
    if (!snapshot) return;

    float lerp_speed = fminf(1.0f, dt * 11.5f);
    for (int i = 0; i < snapshot->node_count; i++) {
        VisualNode *n = &snapshot->nodes[i];
        n->current_pos = Vector2Lerp(n->current_pos, n->target_pos, lerp_speed);
        if (n->highlight_timer > 0.0f) {
            n->highlight_timer = fmaxf(0.0f, n->highlight_timer - dt * 1.4f);
        }
    }
}

/* ============================================================================
 * 3. BEZIER CONNECTOR, CYCLE, AND GARBAGE HAZARD RENDERERS
 * ============================================================================ */

static void draw_arrowhead(Vector2 from, Vector2 to, Color color)
{
    Vector2 dir = Vector2Subtract(to, from);
    float len = Vector2Length(dir);
    if (len < 0.001f) return;
    dir = Vector2Scale(dir, 1.0f / len);
    Vector2 perp = { -dir.y, dir.x };

    float arrow_len  = 11.0f;
    float arrow_half = 5.5f;

    Vector2 base  = Vector2Subtract(to, Vector2Scale(dir, arrow_len));
    Vector2 left  = Vector2Add(base, Vector2Scale(perp, arrow_half));
    Vector2 right = Vector2Subtract(base, Vector2Scale(perp, arrow_half));

    DrawTriangle(to, right, left, color);
}

static void draw_curved_tree_edge(Vector2 start, Vector2 end, Color color, bool is_left)
{
    Vector2 dir = Vector2Subtract(end, start);
    float dist = Vector2Length(dir);
    if (dist < NODE_RADIUS * 2.0f) return;

    Vector2 norm = Vector2Scale(dir, 1.0f / dist);
    Vector2 p1 = Vector2Add(start, Vector2Scale(norm, NODE_RADIUS));
    Vector2 p4 = Vector2Subtract(end, Vector2Scale(norm, NODE_RADIUS + 2.0f));

    float dy = fabsf(p4.y - p1.y);
    float ctrl_offset = fmaxf(28.0f, dy * 0.45f);
    Vector2 c1 = { p1.x + (is_left ? -8.0f : 8.0f), p1.y + ctrl_offset };
    Vector2 c2 = { p4.x, p4.y - ctrl_offset };

    DrawSplineSegmentBezierCubic(p1, c1, c2, p4, 2.6f, color);
    draw_arrowhead(c2, p4, color);
}

static void draw_cycle_edge(Vector2 start, Vector2 ancestor, bool is_left, float pulse, Font font)
{
    float side_sign = is_left ? -1.0f : 1.0f;
    Vector2 p1 = { start.x + side_sign * NODE_RADIUS * 0.8f, start.y };
    Vector2 p4 = { ancestor.x + side_sign * NODE_RADIUS, ancestor.y };

    float loop_bulge = 95.0f + 12.0f * sinf(pulse);
    Vector2 c1 = { start.x + side_sign * loop_bulge, start.y + 35.0f };
    Vector2 c2 = { ancestor.x + side_sign * loop_bulge, ancestor.y - 35.0f };

    unsigned char alpha = (unsigned char)(190 + (int)(55.0f * sinf(pulse * 1.5f)));
    Color crimson = { 244, 63, 94, alpha };

    const int steps = 28;
    for (int i = 0; i < steps; i += 2) {
        float t0 = (float)i / (float)steps;
        float t1 = (float)(i + 1) / (float)steps;

        float u0 = 1.0f - t0;
        Vector2 a = {
            u0*u0*u0*p1.x + 3*u0*u0*t0*c1.x + 3*u0*t0*t0*c2.x + t0*t0*t0*p4.x,
            u0*u0*u0*p1.y + 3*u0*u0*t0*c1.y + 3*u0*t0*t0*c2.y + t0*t0*t0*p4.y
        };
        float u1 = 1.0f - t1;
        Vector2 b = {
            u1*u1*u1*p1.x + 3*u1*u1*t1*c1.x + 3*u1*t1*t1*c2.x + t1*t1*t1*p4.x,
            u1*u1*u1*t1*p1.y + 3*u1*u1*t1*c1.y + 3*u1*t1*t1*c2.y + t1*t1*t1*p4.y
        };
        b.y = u1*u1*u1*p1.y + 3*u1*u1*t1*c1.y + 3*u1*t1*t1*c2.y + t1*t1*t1*p4.y;
        DrawLineEx(a, b, 3.2f, crimson);
    }
    draw_arrowhead(c2, p4, crimson);

    /* Warning Badge [! CICLO] at midpoint of curve */
    Vector2 mid = {
        0.125f * p1.x + 0.375f * c1.x + 0.375f * c2.x + 0.125f * p4.x,
        0.125f * p1.y + 0.375f * c1.y + 0.375f * c2.y + 0.125f * p4.y
    };
    float bw = MeasureTextEx(font, "[! CICLO]", 13.0f, 1.0f).x + 16.0f;
    Rectangle badge = { mid.x - bw * 0.5f, mid.y - 11.0f, bw, 22.0f };
    DrawRectangleRounded(badge, 0.35f, 6, (Color){ 136, 19, 55, 240 });
    DrawRectangleRoundedLinesEx(badge, 0.35f, 6, 1.5f, crimson);
    DrawTextEx(font, "[! CICLO]", (Vector2){ badge.x + 8.0f, badge.y + 4.0f }, 13.0f, 1.0f, (Color){ 255, 228, 230, 255 });
}

static void draw_soft_uninit_pointer(
    Vector2 parent_pos,
    bool    is_left,
    Font    font
)
{
    float offset_x = is_left ? -54.0f : 54.0f;
    float offset_y = 66.0f;
    Vector2 badge_center = { parent_pos.x + offset_x, parent_pos.y + offset_y };

    Vector2 dir  = Vector2Subtract(badge_center, parent_pos);
    Vector2 norm = Vector2Normalize(dir);
    Vector2 p1   = Vector2Add(parent_pos, Vector2Scale(norm, NODE_RADIUS));
    Vector2 p2   = Vector2Subtract(badge_center, Vector2Scale(norm, 12.0f));

    /* Soft dashed gray/amber line ("Em Construção") */
    Color dash_amber = { 217, 169, 78, 175 };
    Color dash_gray  = { 148, 163, 184, 150 };
    int segments = 9;
    for (int i = 0; i < segments; i += 2) {
        float t0 = (float)i / (float)segments;
        float t1 = (float)(i + 1) / (float)segments;
        Vector2 a = Vector2Lerp(p1, p2, t0);
        Vector2 b = Vector2Lerp(p1, p2, t1);
        DrawLineEx(a, b, 2.0f, (i % 4 == 0) ? dash_amber : dash_gray);
    }

    /* Subtle badge: [ ? não inic. ] */
    const char *label = "[ ? não inic. ]";
    Vector2 tsz = MeasureTextEx(font, label, 12.0f, 1.0f);
    float bw = tsz.x + 12.0f;
    float bh = 20.0f;
    Rectangle pill = { badge_center.x - bw * 0.5f, badge_center.y - bh * 0.5f, bw, bh };

    DrawRectangleRounded(pill, 0.4f, 6, (Color){ 28, 28, 32, 230 });
    DrawRectangleRoundedLinesEx(pill, 0.4f, 6, 1.2f, (Color){ 217, 169, 78, 185 });
    DrawTextEx(
        font,
        label,
        (Vector2){ pill.x + 6.0f, pill.y + 4.0f },
        12.0f,
        1.0f,
        (Color){ 253, 224, 71, 225 }
    );
}

static void draw_garbage_or_dangling_hazard(
    Vector2  parent_pos,
    uint64_t bad_addr,
    bool     is_left,
    bool     is_dangling,
    bool     is_fatal_garbage,
    float    pulse,
    Font     font
)
{
    /* When stepping normally (not a fatal garbage dereference or final uninit tree), render softly */
    if (!is_dangling && !is_fatal_garbage) {
        draw_soft_uninit_pointer(parent_pos, is_left, font);
        return;
    }

    float offset_x = is_left ? -58.0f : 58.0f;
    float offset_y = 76.0f;
    Vector2 hazard_pos = { parent_pos.x + offset_x, parent_pos.y + offset_y };

    Vector2 dir = Vector2Subtract(hazard_pos, parent_pos);
    Vector2 norm = Vector2Normalize(dir);
    Vector2 p1 = Vector2Add(parent_pos, Vector2Scale(norm, NODE_RADIUS));
    Vector2 p2 = Vector2Subtract(hazard_pos, Vector2Scale(norm, 20.0f));

    Color col = is_dangling ? (Color){ 217, 70, 239, 240 } : (Color){ 249, 115, 22, 240 };

    /* Draw jagged lightning line */
    Vector2 perp = { -norm.y, norm.x };
    int segs = 6;
    Vector2 prev = p1;
    for (int i = 1; i <= segs; i++) {
        float t = (float)i / (float)segs;
        Vector2 pt = Vector2Lerp(p1, p2, t);
        if (i < segs) {
            float jitter = ((i % 2 == 0) ? 1.0f : -1.0f) * 6.5f;
            pt = Vector2Add(pt, Vector2Scale(perp, jitter));
        }
        DrawLineEx(prev, pt, 2.5f, col);
        prev = pt;
    }
    draw_arrowhead(Vector2Lerp(p1, p2, 0.8f), p2, col);

    /* Draw Hazard Node */
    float r = 20.0f + 2.0f * sinf(pulse * 2.0f);
    DrawCircleV(hazard_pos, r, (Color){ 45, 20, 18, 240 });
    DrawCircleLines((int)hazard_pos.x, (int)hazard_pos.y, r, col);

    const char *icon = is_dangling ? "UAF!" : "LIXO";
    Vector2 tsz = MeasureTextEx(font, icon, 13.0f, 1.0f);
    DrawTextEx(font, icon, (Vector2){ hazard_pos.x - tsz.x * 0.5f, hazard_pos.y - 7.0f }, 13.0f, 1.0f, col);

    char addr_lbl[32];
    if (bad_addr > 0xFFFFFFFFULL) {
        snprintf(addr_lbl, sizeof(addr_lbl), "0x%04" PRIx64 "..", (uint64_t)((bad_addr >> 48) & 0xFFFFU));
    } else {
        snprintf(addr_lbl, sizeof(addr_lbl), "0x%" PRIx64, bad_addr);
    }
    Vector2 asz = MeasureTextEx(font, addr_lbl, 12.0f, 1.0f);
    DrawTextEx(font, addr_lbl, (Vector2){ hazard_pos.x - asz.x * 0.5f, hazard_pos.y + r + 3.0f },
               12.0f, 1.0f, (Color){ 253, 186, 116, 220 });
}

/* ============================================================================
 * 4. SUBSYSTEM D: ANIMATED PEDAGOGICAL "AVORA BLOB" MASCOT & SPRING SPEECH BUBBLES
 * ============================================================================ */

/* Elastic Ease-Out Back curve (c1 = 1.70158f) for bouncy spring speech bubbles */
static float ease_out_back(float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float tm1 = t - 1.0f;
    return 1.0f + c3 * tm1 * tm1 * tm1 + c1 * tm1 * tm1;
}

static void draw_wrapped_bubble_text(Font font, const char *text, Rectangle bounds, float font_size, Color color)
{
    if (!text || text[0] == '\0') return;
    char buf[1024];
    snprintf(buf, sizeof(buf), "%s", text);

    float x = bounds.x;
    float y = bounds.y;
    float space_w = MeasureTextEx(font, " ", font_size, 1.0f).x;

    char *save_line = NULL;
    char *line = strtok_r(buf, "\n", &save_line);
    while (line != NULL) {
        x = bounds.x;
        char line_copy[512];
        snprintf(line_copy, sizeof(line_copy), "%s", line);

        char *save_word = NULL;
        char *word = strtok_r(line_copy, " ", &save_word);
        while (word != NULL) {
            Vector2 wsz = MeasureTextEx(font, word, font_size, 1.0f);
            if (x + wsz.x > bounds.x + bounds.width && x > bounds.x) {
                x = bounds.x;
                y += font_size + 4.0f;
            }
            if (y + font_size <= bounds.y + bounds.height) {
                DrawTextEx(font, word, (Vector2){ x, y }, font_size, 1.0f, color);
            }
            x += wsz.x + space_w;
            word = strtok_r(NULL, " ", &save_word);
        }
        y += font_size + 6.0f;
        line = strtok_r(NULL, "\n", &save_line);
    }
}

static void draw_avora_blob_and_hints(
    const DebuggerSession *session,
    WorkspaceUI           *ui,
    Rectangle              canvas_bounds,
    Font                   font,
    bool                   clicked_hint_button
)
{
    float dt = GetFrameTime();
    float t  = (float)GetTime();

    /* Track newly triggered errors so the alert bubble only pops up once per error and can be dismissed */
    bool has_error = (session->diagnostic.error_type != ERR_NONE);
    if (session->diagnostic.error_type != ui->last_seen_error_type) {
        ui->last_seen_error_type = session->diagnostic.error_type;
        ui->blob_error_dismissed = false;
    }

    bool show_error_bubble = has_error && !ui->blob_error_dismissed && (ui->active_blob_topic == BLOB_HINT_NONE);

    /* 1. Update Spring Animation Progress for Hint Bubbles */
    if (ui->active_blob_topic != BLOB_HINT_NONE) {
        ui->displayed_blob_topic = ui->active_blob_topic;
        ui->blob_anim_progress   = fminf(1.0f, ui->blob_anim_progress + dt * 4.2f);
    } else {
        ui->blob_anim_progress   = fmaxf(0.0f, ui->blob_anim_progress - dt * 5.5f);
    }

    /* 2. Compute Blob Anchor Position & Idle Shrink-into-a-Ball Animation */
    Vector2 mouse = GetMousePosition();

    Vector2 awake_pos = {
        canvas_bounds.x + canvas_bounds.width - 48.0f,
        canvas_bounds.y + canvas_bounds.height - 46.0f
    };
    Vector2 ball_pos = {
        canvas_bounds.x + canvas_bounds.width - 34.0f,
        canvas_bounds.y + canvas_bounds.height - 34.0f
    };

    ui->blob_mascot_bounds = (Rectangle){ awake_pos.x - 36.0f, awake_pos.y - 40.0f, 72.0f, 74.0f };

    /* Handle clicks: click on Blob toggles bubble; click anywhere outside Hint buttons & Blob closes bubble */
    bool any_bubble_open = (ui->active_blob_topic != BLOB_HINT_NONE) || show_error_bubble;
    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && !clicked_hint_button) {
        if (CheckCollisionPointRec(mouse, ui->blob_mascot_bounds)) {
            ui->blob_idle_timer = 0.0f;
            if (any_bubble_open) {
                ui->active_blob_topic    = BLOB_HINT_NONE;
                ui->blob_error_dismissed = true;
                show_error_bubble        = false;
            } else {
                ui->active_blob_topic    = BLOB_HINT_MASCOT;
                ui->blob_anim_progress   = 0.05f;
                ui->blob_error_dismissed = true;
            }
        } else if (any_bubble_open) {
            ui->active_blob_topic    = BLOB_HINT_NONE;
            ui->blob_error_dismissed = true;
            show_error_bubble        = false;
        }
    }

    float dist_to_mouse = Vector2Distance(mouse, awake_pos);
    bool  mouse_near    = (dist_to_mouse < 85.0f);
    bool  is_busy       = (ui->active_blob_topic != BLOB_HINT_NONE) ||
                          show_error_bubble ||
                          (session->state == STATE_RUNNING) ||
                          (session->state == STATE_STEPPING);

    if (mouse_near || is_busy) {
        ui->blob_idle_timer = 0.0f;
    } else {
        ui->blob_idle_timer += dt;
    }

    /* Shrink into a cozy little ball after 4.5s of inactivity */
    float target_awake = (ui->blob_idle_timer < 4.5f) ? 1.0f : 0.0f;
    float anim_speed   = (target_awake > ui->blob_awake_anim) ? 6.5f : 3.2f;
    ui->blob_awake_anim += (target_awake - ui->blob_awake_anim) * fminf(1.0f, dt * anim_speed);
    if (ui->blob_awake_anim < 0.0f) ui->blob_awake_anim = 0.0f;
    if (ui->blob_awake_anim > 1.0f) ui->blob_awake_anim = 1.0f;

    float a = ui->blob_awake_anim;
    /* Cute elastic overshoot when waking up */
    float bounce = sinf(a * 3.14159265f) * 0.16f;
    Vector2 blob_center = Vector2Lerp(ball_pos, awake_pos, a);

    float breath_awake = sinf(t * 3.5f);
    float breath_ball  = sinf(t * 2.1f);

    float rx_awake = 25.0f + breath_awake * 1.8f;
    float ry_awake = 23.0f - breath_awake * 1.6f;
    float r_ball   = 12.5f + breath_ball * 0.9f;

    float rx = r_ball + (rx_awake - r_ball) * a + bounce * 6.0f;
    float ry = r_ball + (ry_awake - r_ball) * a - bounce * 4.0f;

    /* Ground Shadow & Soft Monochrome Pulsing Aura */
    DrawEllipse(
        (int)blob_center.x,
        (int)(blob_center.y + ry + 1.0f),
        rx * 0.85f,
        3.5f + 2.5f * a,
        (Color){ 0, 0, 0, (unsigned char)(85 + (int)(35.0f * a)) }
    );

    Color aura_col = has_error
                     ? (Color){ 244, 63, 94, (unsigned char)(75 + (int)(35.0f * sinf(t * 8.0f))) }
                     : (Color){ 16, 185, 129, (unsigned char)(38 + (int)(26.0f * a + 12.0f * breath_ball)) };
    DrawEllipse((int)blob_center.x, (int)blob_center.y, rx + 4.5f + 2.0f * a, ry + 4.5f + 2.0f * a, aura_col);

    /* Cute Sprout Stem & Leaves on top of Blob's head */
    Vector2 head_top = { blob_center.x, blob_center.y - ry };
    float sway = sinf(t * (2.2f + 2.0f * a)) * (1.2f + 2.0f * a);
    float stem_h = 4.0f + 7.5f * a;
    Vector2 stem_top = { head_top.x + sway, head_top.y - stem_h };
    DrawLineEx(head_top, stem_top, 2.2f, (Color){ 52, 211, 153, 255 });
    float leaf_rx = 3.8f + 2.8f * a;
    float leaf_ry = 2.2f + 1.4f * a;
    DrawEllipse((int)(stem_top.x - (3.5f + 2.5f * a)), (int)(stem_top.y - 1.5f), leaf_rx, leaf_ry, (Color){ 16, 185, 129, 255 });
    DrawEllipse((int)(stem_top.x + (3.5f + 2.5f * a)), (int)(stem_top.y - 1.0f), leaf_rx, leaf_ry, (Color){ 52, 211, 153, 255 });

    /* Friendly Emerald Green Slime/Orb Body + Specular Highlight */
    Color body_main = has_error ? (Color){ 225, 29, 72, 245 } : (Color){ 16, 158, 114, 248 };
    Color body_rim  = has_error ? (Color){ 253, 164, 175, 255 } : (Color){ 110, 231, 183, 255 };
    DrawEllipse((int)blob_center.x, (int)blob_center.y, rx, ry, body_main);
    DrawEllipseLines((int)blob_center.x, (int)blob_center.y, rx, ry, body_rim);
    DrawEllipse(
        (int)(blob_center.x - rx * 0.32f),
        (int)(blob_center.y - ry * 0.34f),
        2.8f + 3.2f * a,
        1.8f + 1.8f * a,
        (Color){ 255, 255, 255, 130 }
    );

    if (a < 0.38f && !has_error) {
        /* Curled up into a cozy little ball: cute sleeping/happy curved eyelids & floating mini-bubble */
        float eye_dx = 4.2f;
        Vector2 l_eye = { blob_center.x - eye_dx, blob_center.y + 0.5f };
        Vector2 r_eye = { blob_center.x + eye_dx, blob_center.y + 0.5f };
        DrawLineEx((Vector2){ l_eye.x - 2.5f, l_eye.y }, (Vector2){ l_eye.x + 2.5f, l_eye.y }, 1.8f, (Color){ 209, 250, 229, 225 });
        DrawLineEx((Vector2){ r_eye.x - 2.5f, r_eye.y }, (Vector2){ r_eye.x + 2.5f, r_eye.y }, 1.8f, (Color){ 209, 250, 229, 225 });

        /* Soft floating sleep/idle sparkle above the ball */
        float z_phase = fmodf(t * 0.85f, 1.0f);
        Vector2 z_pos = {
            blob_center.x + 9.0f + sinf(t * 2.5f) * 2.5f,
            blob_center.y - ry - 6.0f - z_phase * 10.0f
        };
        unsigned char z_alpha = (unsigned char)(180.0f * (1.0f - z_phase) * (1.0f - a));
        DrawCircleV(z_pos, 2.2f, (Color){ 167, 243, 208, z_alpha });
    } else {
        /* Awake & Friendly Face: Expressive Eyes tracking Mouse + Rosy Cheeks + Warm Smile */
        Vector2 target_dir = Vector2Subtract(mouse, blob_center);
        if (Vector2Length(target_dir) > 1.0f) {
            target_dir = Vector2Scale(Vector2Normalize(target_dir), 2.6f * a);
        }
        ui->blob_eye_look = Vector2Lerp(ui->blob_eye_look, target_dir, fminf(1.0f, dt * 10.0f));

        ui->blob_blink_timer += dt;
        bool blinking = (fmodf(ui->blob_blink_timer, 4.1f) > 3.92f);

        float eye_sep = 8.5f * (0.65f + 0.35f * a);
        Vector2 left_eye  = { blob_center.x - eye_sep, blob_center.y - 1.5f };
        Vector2 right_eye = { blob_center.x + eye_sep, blob_center.y - 1.5f };

        /* Friendly rosy cheeks */
        if (!has_error) {
            DrawEllipse((int)(left_eye.x - 3.5f),  (int)(left_eye.y + 6.0f),  3.6f * a, 2.1f * a, (Color){ 251, 113, 133, 115 });
            DrawEllipse((int)(right_eye.x + 3.5f), (int)(right_eye.y + 6.0f), 3.6f * a, 2.1f * a, (Color){ 251, 113, 133, 115 });
        }

        if (has_error) {
            DrawCircleV(left_eye,  5.2f, WHITE);
            DrawCircleV(right_eye, 5.2f, WHITE);
            DrawCircleV(Vector2Add(left_eye,  ui->blob_eye_look), 2.2f, (Color){ 16, 16, 18, 255 });
            DrawCircleV(Vector2Add(right_eye, ui->blob_eye_look), 2.2f, (Color){ 16, 16, 18, 255 });
            DrawCircle((int)blob_center.x, (int)(blob_center.y + 8.5f), 3.5f, (Color){ 16, 16, 18, 240 });
        } else if (blinking) {
            DrawLineEx((Vector2){ left_eye.x - 4.0f, left_eye.y }, (Vector2){ left_eye.x + 4.0f, left_eye.y }, 2.0f, WHITE);
            DrawLineEx((Vector2){ right_eye.x - 4.0f, right_eye.y }, (Vector2){ right_eye.x + 4.0f, right_eye.y }, 2.0f, WHITE);
            DrawCircle((int)blob_center.x, (int)(blob_center.y + 6.5f), 2.2f, (Color){ 209, 250, 229, 230 });
        } else {
            float eye_r = 4.8f * (0.7f + 0.3f * a);
            DrawCircleV(left_eye,  eye_r, WHITE);
            DrawCircleV(right_eye, eye_r, WHITE);
            Vector2 l_pupil = Vector2Add(left_eye,  ui->blob_eye_look);
            Vector2 r_pupil = Vector2Add(right_eye, ui->blob_eye_look);
            DrawCircleV(l_pupil, 2.5f, (Color){ 16, 16, 18, 255 });
            DrawCircleV(r_pupil, 2.5f, (Color){ 16, 16, 18, 255 });
            /* Tiny eye sparkle for extra friendliness */
            DrawCircleV((Vector2){ l_pupil.x - 0.9f, l_pupil.y - 0.9f }, 0.9f, WHITE);
            DrawCircleV((Vector2){ r_pupil.x - 0.9f, r_pupil.y - 0.9f }, 0.9f, WHITE);

            /* Cute friendly smile */
            DrawLineEx(
                (Vector2){ blob_center.x - 4.2f * a, blob_center.y + 6.5f * a },
                (Vector2){ blob_center.x,            blob_center.y + 9.0f * a },
                1.9f, WHITE
            );
            DrawLineEx(
                (Vector2){ blob_center.x,            blob_center.y + 9.0f * a },
                (Vector2){ blob_center.x + 4.2f * a, blob_center.y + 6.5f * a },
                1.9f, WHITE
            );
        }
    }

    /* Mascot label ("Blob") fades gently when curled into a ball */
    Vector2 lbl_sz = MeasureTextEx(font, "Blob", 12.0f, 1.0f);
    DrawTextEx(
        font,
        "Blob",
        (Vector2){ blob_center.x - lbl_sz.x * 0.5f, blob_center.y + ry + 3.0f },
        12.0f,
        1.0f,
        (Color){ 167, 243, 208, (unsigned char)(115 + (int)(130.0f * a)) }
    );

    /* 3. Draw Spring-Animated Speech Bubble if active or if an Error just occurred */
    if (ui->blob_anim_progress > 0.01f || show_error_bubble) {
        float scale = show_error_bubble ? 1.0f : ease_out_back(ui->blob_anim_progress);
        float bw = fminf(415.0f, canvas_bounds.width - 36.0f);
        float bh = show_error_bubble ? 116.0f : 158.0f;

        float sw_scaled = bw * scale;
        float sh_scaled = bh * scale;

        Vector2 anchor = { blob_center.x - 16.0f, blob_center.y - 42.0f };
        Rectangle bubble = {
            anchor.x - sw_scaled,
            anchor.y - sh_scaled,
            sw_scaled,
            sh_scaled
        };
        if (bubble.x < canvas_bounds.x + 12.0f) bubble.x = canvas_bounds.x + 12.0f;
        if (bubble.y < canvas_bounds.y + 48.0f) bubble.y = canvas_bounds.y + 48.0f;

        /* Drop Shadow + Rounded Monochrome Bubble + Speech Tail */
        DrawRectangleRounded(
            (Rectangle){ bubble.x + 4.0f, bubble.y + 5.0f, bubble.width, bubble.height },
            0.12f, 8, (Color){ 0, 0, 0, 150 }
        );
        Color b_bg     = show_error_bubble ? (Color){ 69, 10, 20, 248 } : (Color){ 24, 24, 27, 250 };
        Color b_border = show_error_bubble ? (Color){ 251, 113, 133, 255 } : (Color){ 228, 228, 231, 255 };
        DrawRectangleRounded(bubble, 0.12f, 8, b_bg);
        DrawRectangleRoundedLinesEx(bubble, 0.12f, 8, 1.8f, b_border);

        /* Speech tail pointing toward Blob */
        Vector2 t1 = { bubble.x + bubble.width - 34.0f, bubble.y + bubble.height };
        Vector2 t2 = { bubble.x + bubble.width - 16.0f, bubble.y + bubble.height };
        Vector2 t3 = { blob_center.x - 8.0f,            blob_center.y - 24.0f };
        DrawTriangle(t1, t3, t2, b_bg);

        if (scale > 0.65f) {
            const char *title = "";
            const char *body  = "";

            if (show_error_bubble) {
                title = "Ei! O Blob encontrou um problema na memória:";
                body  = "Olhe para o nó destacado no Canvas! No painel aqui embaixo eu te explico exatamente por que esse ponteiro falhou e como consertar.";
            } else {
                switch (ui->displayed_blob_topic) {
                    case BLOB_HINT_BUILD_TREE:
                        title = "Dica do Blob: Criando Árvores com BUILD_TREE";
                        body  = "Quer testar um caso rápido sem fazer vários inserts? Use BUILD_TREE(\"[4,1,6,null,2]\")! "
                                "Ele lê o vetor em largura (BFS) exatamente como no LeetCode. E se você criar duas raízes no main(), eu desenho a floresta lado a lado!";
                        break;
                    case BLOB_HINT_LEETCODE:
                        title = "Dica do Blob: LeetCode & Estruturas de Dados";
                        body  = "Cole sua função (BST, AVL, Rubro-Negra, Árvore B, DFS/BFS ou problemas do LeetCode) no editor! "
                                "Se não quiser escrever a main(), pode colar só a função: eu crio uma main() automática usando o vetor da barra inferior.";
                        break;
                    case BLOB_HINT_POINTERS:
                        title = "Dica do Blob: Guia Visual da Heap";
                        body  = "- Ciano (->left) e Roxo (->right): Ponteiros saudáveis.\n"
                                "- Tracejado [ ? não inic. ]: Nó recém-alocado aguardando atribuição.\n"
                                "- Raio Laranja (LIXO): Acesso ou término com lixo de memória!\n"
                                "- Curva Carmesim ([! CICLO]) e Roxo (UAF!): Ciclo ou nó após free().";
                        break;
                    case BLOB_HINT_MASCOT:
                    default:
                        title = "Oi! Eu sou o Blob, seu amigo de Algoritmos!";
                        body  = "Estou aqui para te ajudar a enxergar cada ponteiro na memória! Escolha um template (AVL, Rubro-Negra, Árvore B, Buscas...) "
                                "e avance passo a passo. Clique fora ou em mim para fechar este balão!";
                        break;
                }
            }

            BeginScissorMode((int)bubble.x + 4, (int)bubble.y + 4, (int)bubble.width - 8, (int)bubble.height - 8);
            DrawTextEx(font, title, (Vector2){ bubble.x + 12.0f, bubble.y + 10.0f }, 15.0f, 1.0f, b_border);
            Rectangle text_box = {
                bubble.x + 12.0f,
                bubble.y + 32.0f,
                bubble.width - 24.0f,
                bubble.height - 38.0f
            };
            draw_wrapped_bubble_text(font, body, text_box, 14.0f, (Color){ 244, 244, 245, 255 });
            EndScissorMode();
        }
    }
}

/* ============================================================================
 * 5. MAIN INTERACTIVE MEMORY CANVAS RENDERER
 * ============================================================================ */

void layout_render_canvas(
    const DebuggerSession *session,
    WorkspaceUI           *ui,
    Rectangle              canvas_bounds
)
{
    if (!session || !ui) return;
    Font font = GuiGetFont();
    const TreeSnapshot *snap = &session->snapshot;

    /* Matte Carbon Black canvas background matching logo.jpg monochrome contrast */
    DrawRectangleRec(canvas_bounds, (Color){ 16, 16, 18, 255 });

    /* Mouse Pan & Wheel Zoom when cursor is inside canvas_bounds */
    Vector2 mouse = GetMousePosition();
    bool inside_canvas = CheckCollisionPointRec(mouse, canvas_bounds) &&
                         !ui->dropdown_edit_mode &&
                         !ui->dragging_v_split &&
                         !ui->dragging_h_split;

    if (inside_canvas) {
        float wheel = GetMouseWheelMove();
        if (fabsf(wheel) > 0.001f) {
            Vector2 mouseWorldPos = GetScreenToWorld2D(mouse, ui->camera);
            ui->camera.offset = mouse;
            ui->camera.target = mouseWorldPos;
            ui->camera.zoom += wheel * 0.12f;
            if (ui->camera.zoom < 0.30f) ui->camera.zoom = 0.30f;
            if (ui->camera.zoom > 2.60f) ui->camera.zoom = 2.60f;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
            Rectangle top_bar_zone = { canvas_bounds.x, canvas_bounds.y, canvas_bounds.width, 46.0f };
            if (!CheckCollisionPointRec(mouse, top_bar_zone) &&
                !CheckCollisionPointRec(mouse, ui->blob_mascot_bounds)) {
                ui->is_panning       = true;
                ui->pan_start_mouse  = mouse;
                ui->pan_start_target = ui->camera.target;
            }
        }
    }

    if (ui->is_panning) {
        if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
            Vector2 delta = Vector2Subtract(ui->pan_start_mouse, mouse);
            ui->camera.target = Vector2Add(ui->pan_start_target, Vector2Scale(delta, 1.0f / ui->camera.zoom));
        } else {
            ui->is_panning = false;
        }
    }

    /* Confine all 2D world & overlay rendering strictly inside BeginScissorMode */
    BeginScissorMode((int)canvas_bounds.x, (int)canvas_bounds.y, (int)canvas_bounds.width, (int)canvas_bounds.height);

    /* Subtle neutral graphite dot/grid background in screen space */
    int grid_step = (int)(36.0f * ui->camera.zoom);
    if (grid_step >= 14) {
        for (int gx = (int)canvas_bounds.x; gx < (int)(canvas_bounds.x + canvas_bounds.width); gx += grid_step) {
            for (int gy = (int)canvas_bounds.y; gy < (int)(canvas_bounds.y + canvas_bounds.height); gy += grid_step) {
                DrawPixel(gx, gy, (Color){ 48, 48, 54, 120 });
            }
        }
    }

    BeginMode2D(ui->camera);

    /* Draw Staging Zone Box if any detached local nodes exist */
    bool has_detached = false;
    float min_det_x = 0.0f;
    for (int i = 0; i < snap->node_count; i++) {
        if (snap->nodes[i].is_detached) {
            if (!has_detached || snap->nodes[i].target_pos.x < min_det_x) {
                min_det_x = snap->nodes[i].target_pos.x;
            }
            has_detached = true;
        }
    }
    if (has_detached) {
        Rectangle stage_box = { min_det_x - 65.0f, -80.0f, 360.0f, 185.0f };
        DrawRectangleRounded(stage_box, 0.08f, 6, (Color){ 26, 26, 30, 195 });
        DrawRectangleRoundedLinesEx(stage_box, 0.08f, 6, 1.5f, (Color){ 113, 113, 122, 170 });
        DrawTextEx(font, "Área de Alocação / Ponteiros Locais (Heap)",
                   (Vector2){ stage_box.x + 12.0f, stage_box.y + 8.0f }, 14.0f, 1.0f, (Color){ 212, 212, 216, 220 });
    }

    /* PASS 1: Draw Edges (Left = Cyan, Right = Purple, Cycle = Crimson, Garbage = Soft/Orange) */
    Color left_col  = { 56, 189, 248, 235 };  /* Cyan accent */
    Color right_col = { 167, 139, 250, 235 }; /* Purple accent */
    bool  is_fatal_garbage = (session->diagnostic.error_type == ERR_SIGSEGV_GARBAGE);

    for (int i = 0; i < snap->node_count; i++) {
        const VisualNode *n = &snap->nodes[i];

        /* Left pointer edge */
        if (n->left_state == EDGE_OK && n->left_addr != 0) {
            int l_idx = find_node_index_by_addr(snap, n->left_addr);
            if (l_idx >= 0) {
                draw_curved_tree_edge(n->current_pos, snap->nodes[l_idx].current_pos, left_col, true);
            }
        } else if (n->left_state == EDGE_CYCLE && n->left_addr != 0) {
            int l_idx = find_node_index_by_addr(snap, n->left_addr);
            if (l_idx >= 0) {
                draw_cycle_edge(n->current_pos, snap->nodes[l_idx].current_pos, true, ui->pulse_timer, font);
            }
        } else if (n->left_state == EDGE_GARBAGE_PTR || n->left_state == EDGE_DANGLING) {
            draw_garbage_or_dangling_hazard(
                n->current_pos, n->left_addr, true,
                (n->left_state == EDGE_DANGLING), is_fatal_garbage, ui->pulse_timer, font
            );
        }

        /* Right pointer edge */
        if (n->right_state == EDGE_OK && n->right_addr != 0) {
            int r_idx = find_node_index_by_addr(snap, n->right_addr);
            if (r_idx >= 0) {
                draw_curved_tree_edge(n->current_pos, snap->nodes[r_idx].current_pos, right_col, false);
            }
        } else if (n->right_state == EDGE_CYCLE && n->right_addr != 0) {
            int r_idx = find_node_index_by_addr(snap, n->right_addr);
            if (r_idx >= 0) {
                draw_cycle_edge(n->current_pos, snap->nodes[r_idx].current_pos, false, ui->pulse_timer, font);
            }
        } else if (n->right_state == EDGE_GARBAGE_PTR || n->right_state == EDGE_DANGLING) {
            draw_garbage_or_dangling_hazard(
                n->current_pos, n->right_addr, false,
                (n->right_state == EDGE_DANGLING), is_fatal_garbage, ui->pulse_timer, font
            );
        }
    }

    /* PASS 2: Draw Tree Nodes, Addresses, and Floating Variable Name Badges (Monochrome Black & White style) */
    for (int i = 0; i < snap->node_count; i++) {
        const VisualNode *n = &snap->nodes[i];
        Vector2 pos = n->current_pos;

        if (n->highlight_timer > 0.01f) {
            float glow_r = NODE_RADIUS + 8.0f * n->highlight_timer;
            DrawCircleV(pos, glow_r, (Color){ 250, 204, 21, (unsigned char)(110.0f * n->highlight_timer) });
        } else if (n->is_active_focus) {
            DrawCircleV(pos, NODE_RADIUS + 5.0f, (Color){ 255, 255, 255, 65 });
        }

        /* Pure Monochrome Obsidian/Graphite Node Body & Crisp White/Silver Rim (matching logo.jpg) */
        Color tree_tint  = (n->forest_id == 1) ? (Color){ 32, 32, 36, 255 } : (Color){ 18, 18, 20, 255 };
        Color body_col   = n->is_dangling ? (Color){ 88, 28, 135, 240 } :
                           (n->is_detached ? (Color){ 36, 36, 42, 255 } : tree_tint);
        Color border_col = n->is_dangling ? (Color){ 232, 121, 249, 255 } :
                           (n->is_active_focus ? (Color){ 255, 255, 255, 255 } : (Color){ 228, 228, 231, 240 });

        DrawCircleV(pos, NODE_RADIUS, body_col);
        DrawRing(pos, NODE_RADIUS - 2.4f, NODE_RADIUS, 0.0f, 360.0f, 36, border_col);

        /* Node integer value */
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%d", n->val);
        Vector2 vsz = MeasureTextEx(font, val_str, 18.0f, 1.0f);
        DrawTextEx(font, val_str, (Vector2){ pos.x - vsz.x * 0.5f, pos.y - vsz.y * 0.5f },
                   18.0f, 1.0f, (Color){ 250, 250, 250, 255 });

        /* Memory address badge below node (0x...) */
        char addr_str[32];
        snprintf(addr_str, sizeof(addr_str), "0x%" PRIx64, (uint64_t)(n->addr & 0xFFFFFFU));
        Vector2 asz = MeasureTextEx(font, addr_str, 12.0f, 1.0f);
        Rectangle addr_pill = { pos.x - asz.x * 0.5f - 4.0f, pos.y + NODE_RADIUS + 4.0f, asz.x + 8.0f, 15.0f };
        DrawRectangleRounded(addr_pill, 0.3f, 4, (Color){ 24, 24, 27, 225 });
        DrawTextEx(font, addr_str, (Vector2){ addr_pill.x + 4.0f, addr_pill.y + 1.5f },
                   12.0f, 1.0f, (Color){ 161, 161, 170, 235 });

        /* Floating Variable Name Pill Badges above the node (Monochrome Graphite + White Rim) */
        int badge_stack = 0;
        for (int v = 0; v < snap->local_var_count; v++) {
            if (snap->local_vars[v].addr == n->addr) {
                const char *vname = snap->local_vars[v].name;
                Vector2 bsz = MeasureTextEx(font, vname, 13.0f, 1.0f);
                float bw = bsz.x + 14.0f;
                float bh = 19.0f;
                float bx = pos.x - bw * 0.5f;
                float by = pos.y - NODE_RADIUS - 24.0f - (float)badge_stack * 22.0f;

                Rectangle pill = { bx, by, bw, bh };
                DrawRectangleRounded(pill, 0.45f, 6, (Color){ 36, 36, 40, 245 });
                DrawRectangleRoundedLinesEx(pill, 0.45f, 6, 1.2f, (Color){ 244, 244, 245, 255 });
                DrawTextEx(font, vname, (Vector2){ bx + 7.0f, by + 3.0f },
                           13.0f, 1.0f, (Color){ 250, 250, 250, 255 });
                badge_stack++;
            }
        }
    }

    EndMode2D();

    /* Empty Canvas Overlay Instructions (No Embedded Logo Watermark) */
    if (snap->node_count == 0) {
        const char *empty_msg = (session->state == STATE_IDLE)
            ? "AVORA Canvas — Clique em [ Compilar & Iniciar ] para visualizar sua árvore ou solução LeetCode"
            : "Árvore vazia (NULL) — Avance com [ Passo ] para observar a construção da árvore na Heap";
        Vector2 msz = MeasureTextEx(font, empty_msg, 15.0f, 1.0f);
        float mx = canvas_bounds.x + fmaxf(12.0f, (canvas_bounds.width - msz.x) * 0.5f);
        DrawTextEx(
            font,
            empty_msg,
            (Vector2){ mx, canvas_bounds.y + canvas_bounds.height * 0.48f },
            15.0f,
            1.0f,
            (Color){ 161, 161, 170, 200 }
        );
    }

    /* Top Toolbar: Dynamic Hint Buttons + Legend + Camera Controls (Zero Overlap!) */
    float cur_x = canvas_bounds.x + 10.0f;
    float top_y = canvas_bounds.y + 8.0f;
    float fs    = (float)GuiGetStyle(DEFAULT, TEXT_SIZE);
    bool  clicked_hint_button = false;

    const char *lbl_bt = "? BUILD_TREE";
    float w_bt = MeasureTextEx(font, lbl_bt, fs, 1.0f).x + 22.0f;
    if (GuiButton((Rectangle){ cur_x, top_y, w_bt, 28.0f }, lbl_bt)) {
        ui->active_blob_topic    = (ui->active_blob_topic == BLOB_HINT_BUILD_TREE) ? BLOB_HINT_NONE : BLOB_HINT_BUILD_TREE;
        ui->blob_anim_progress   = 0.05f;
        ui->blob_error_dismissed = true;
        clicked_hint_button      = true;
    }
    cur_x += w_bt + 6.0f;

    const char *lbl_lc = "? LeetCode";
    float w_lc = MeasureTextEx(font, lbl_lc, fs, 1.0f).x + 22.0f;
    if (GuiButton((Rectangle){ cur_x, top_y, w_lc, 28.0f }, lbl_lc)) {
        ui->active_blob_topic    = (ui->active_blob_topic == BLOB_HINT_LEETCODE) ? BLOB_HINT_NONE : BLOB_HINT_LEETCODE;
        ui->blob_anim_progress   = 0.05f;
        ui->blob_error_dismissed = true;
        clicked_hint_button      = true;
    }
    cur_x += w_lc + 6.0f;

    const char *lbl_pt = "? Ponteiros";
    float w_pt = MeasureTextEx(font, lbl_pt, fs, 1.0f).x + 22.0f;
    if (GuiButton((Rectangle){ cur_x, top_y, w_pt, 28.0f }, lbl_pt)) {
        ui->active_blob_topic    = (ui->active_blob_topic == BLOB_HINT_POINTERS) ? BLOB_HINT_NONE : BLOB_HINT_POINTERS;
        ui->blob_anim_progress   = 0.05f;
        ui->blob_error_dismissed = true;
        clicked_hint_button      = true;
    }
    cur_x += w_pt + 10.0f;

    /* Right-aligned Camera Controls ([ Centralizar ] + Zoom %) */
    char zoom_lbl[32];
    snprintf(zoom_lbl, sizeof(zoom_lbl), "Zoom: %d%%", (int)(ui->camera.zoom * 100.0f));
    float w_zoom = MeasureTextEx(font, zoom_lbl, 14.0f, 1.0f).x + 20.0f;
    Rectangle zoom_box = {
        canvas_bounds.x + canvas_bounds.width - w_zoom - 10.0f,
        top_y,
        w_zoom,
        28.0f
    };
    DrawRectangleRounded(zoom_box, 0.25f, 4, (Color){ 28, 28, 32, 225 });
    DrawTextEx(font, zoom_lbl, (Vector2){ zoom_box.x + 10.0f, zoom_box.y + 7.0f },
               14.0f, 1.0f, (Color){ 228, 228, 231, 255 });

    const char *lbl_center = "Centralizar";
    float w_center = MeasureTextEx(font, lbl_center, fs, 1.0f).x + 24.0f;
    Rectangle center_btn = {
        zoom_box.x - w_center - 8.0f,
        top_y,
        w_center,
        28.0f
    };
    if (center_btn.x > cur_x + 4.0f) {
        if (GuiButton(center_btn, lbl_center)) {
            layout_center_camera(ui, snap, canvas_bounds);
        }
    }

    /* Draw Animated Pedagogical Blob Mascot & Spring Hint Speech Bubbles */
    draw_avora_blob_and_hints(session, ui, canvas_bounds, font, clicked_hint_button);

    EndScissorMode();
    DrawRectangleLinesEx(canvas_bounds, 1.0f, (Color){ 56, 56, 62, 255 });
}
