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
#define LIST_HORIZONTAL_GAP 112.0f

/* ============================================================================
 * 1. CAMERA INITIALIZATION, BOUNDING-BOX AUTO-FOCUS & CENTERING
 * ============================================================================ */

void layout_init_camera(WorkspaceUI *ui, Rectangle canvas_bounds)
{
    if (!ui) return;
    ui->camera.offset      = (Vector2){ canvas_bounds.x + canvas_bounds.width * 0.5f, canvas_bounds.y + canvas_bounds.height * 0.42f };
    ui->camera.target      = (Vector2){ 0.0f, 90.0f };
    ui->camera.rotation    = 0.0f;
    ui->camera.zoom        = 1.0f;
    ui->is_panning         = false;
    ui->camera_auto_focus  = true;
}

static void compute_scene_bounding_box(
    const TreeSnapshot *snapshot,
    Vector2            *out_min,
    Vector2            *out_max,
    bool               *out_has_items
)
{
    *out_has_items = false;
    *out_min = (Vector2){ -120.0f, -40.0f };
    *out_max = (Vector2){  120.0f, 180.0f };
    if (!snapshot) return;

    float min_x =  1e9f, min_y =  1e9f;
    float max_x = -1e9f, max_y = -1e9f;

    for (int i = 0; i < snapshot->node_count; i++) {
        Vector2 p = snapshot->nodes[i].target_pos;
        if (p.x - 75.0f < min_x) min_x = p.x - 75.0f;
        if (p.x + 75.0f > max_x) max_x = p.x + 75.0f;
        if (p.y - 65.0f < min_y) min_y = p.y - 65.0f;
        if (p.y + 65.0f > max_y) max_y = p.y + 65.0f;
        *out_has_items = true;
    }

    if (snapshot->array_count > 0) {
        float base_y = (snapshot->node_count > 0) ? (max_y + 70.0f) : 30.0f;
        for (int a = 0; a < snapshot->array_count; a++) {
            int cnt = snapshot->arrays[a].count;
            float total_w = (float)cnt * 56.0f;
            float left_x  = -total_w * 0.5f - 40.0f;
            float right_x =  total_w * 0.5f + 40.0f;
            float top_y   = base_y + (float)a * 130.0f - 45.0f;
            float bot_y   = top_y + 125.0f;
            if (left_x  < min_x) min_x = left_x;
            if (right_x > max_x) max_x = right_x;
            if (top_y   < min_y) min_y = top_y;
            if (bot_y   > max_y) max_y = bot_y;
            *out_has_items = true;
        }
    }

    if (*out_has_items) {
        *out_min = (Vector2){ min_x, min_y };
        *out_max = (Vector2){ max_x, max_y };
    }
}

void layout_center_camera(WorkspaceUI *ui, const TreeSnapshot *snapshot, Rectangle canvas_bounds)
{
    if (!ui) return;
    ui->camera.offset = (Vector2){ canvas_bounds.x + canvas_bounds.width * 0.5f, canvas_bounds.y + canvas_bounds.height * 0.5f };

    Vector2 bmin, bmax;
    bool has_items = false;
    compute_scene_bounding_box(snapshot, &bmin, &bmax, &has_items);
    if (has_items) {
        ui->camera.target = (Vector2){ (bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f };
        float scene_w = fmaxf(240.0f, bmax.x - bmin.x + 120.0f);
        float scene_h = fmaxf(200.0f, bmax.y - bmin.y + 120.0f);
        float zx = (canvas_bounds.width  * 0.82f) / scene_w;
        float zy = (canvas_bounds.height * 0.78f) / scene_h;
        float z  = fminf(zx, zy);
        if (z < 0.42f) z = 0.42f;
        if (z > 1.15f) z = 1.15f;
        ui->camera.zoom = z;
    } else {
        ui->camera.target = (Vector2){ 0.0f, 55.0f };
        ui->camera.zoom   = 1.0f;
    }
}

/* ============================================================================
 * 2. HIERARCHICAL TREE, FOREST & LINEAR LINKED-LIST LAYOUT ENGINE
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

    /* If this is a Linked List node, lay out linearly along next (right_addr) */
    if (n->is_list_node) {
        int count = 1;
        int cur = node_idx;
        while (count < 64) {
            const VisualNode *cn = &snapshot->nodes[cur];
            if (cn->right_state == EDGE_OK && cn->right_addr != 0) {
                int nxt = find_node_index_by_addr(snapshot, cn->right_addr);
                if (nxt >= 0 && !visited[nxt]) {
                    visited[nxt] = true;
                    snapshot->nodes[nxt].is_list_node = true;
                    cur = nxt;
                    count++;
                    continue;
                }
            }
            break;
        }
        n->subtree_width = (float)count * LIST_HORIZONTAL_GAP;
        return n->subtree_width;
    }

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

    /* Linear horizontal layout for Linked Lists */
    if (n->is_list_node) {
        float start_x = center_x - n->subtree_width * 0.5f + LIST_HORIZONTAL_GAP * 0.5f;
        int cur = node_idx;
        int idx = 0;
        while (cur >= 0 && idx < 64) {
            VisualNode *cn = &snapshot->nodes[cur];
            visited[cur] = true;
            cn->is_list_node = true;
            cn->target_pos = (Vector2){ start_x + (float)idx * LIST_HORIZONTAL_GAP, center_y + 30.0f };
            idx++;
            if (cn->right_state == EDGE_OK && cn->right_addr != 0) {
                int nxt = find_node_index_by_addr(snapshot, cn->right_addr);
                if (nxt >= 0 && !visited[nxt]) {
                    cur = nxt;
                    continue;
                }
            }
            break;
        }
        return;
    }

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

    float cur_left_x = -total_forest_w * 0.5f;
    for (int r = 0; r < valid_roots; r++) {
        float w = tree_widths[r];
        float tree_center_x = cur_left_x + w * 0.5f;
        assign_coordinates_rec(snapshot, root_indices[r], tree_center_x, 45.0f, visited_p, 0);
        cur_left_x += w + FOREST_TREE_GAP;
    }

    /* Place any detached / newly allocated local island nodes in the top-left Staging Zone */
    int staging_slot = 0;
    float stage_base_x = fminf(-320.0f, -total_forest_w * 0.5f - 170.0f);
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

void layout_update_animations(TreeSnapshot *snapshot, float dt, float *pulse_timer, bool reduce_animations)
{
    if (pulse_timer) {
        *pulse_timer += dt * (reduce_animations ? 1.5f : 4.0f);
    }
    if (!snapshot) return;

    float lerp_speed = avora_exp_decay(14.5f, dt, reduce_animations);
    for (int i = 0; i < snapshot->node_count; i++) {
        VisualNode *n = &snapshot->nodes[i];
        n->current_pos = Vector2Lerp(n->current_pos, n->target_pos, lerp_speed);
        if (n->highlight_timer > 0.0f) {
            n->highlight_timer = fmaxf(0.0f, n->highlight_timer - dt * (reduce_animations ? 4.0f : 1.4f));
        }
        if (n->border_flash_timer > 0.0f) {
            n->border_flash_timer = fmaxf(0.0f, n->border_flash_timer - dt);
        }
    }
}

/* ============================================================================
 * 3. BEZIER CONNECTOR, LINKED-LIST ARROWS, CYCLES & HAZARDS
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

static void draw_curved_tree_edge(Vector2 start, Vector2 end, Color color, bool is_left, bool is_list_edge)
{
    Vector2 dir = Vector2Subtract(end, start);
    float dist = Vector2Length(dir);
    if (dist < NODE_RADIUS * 2.0f) return;

    if (is_list_edge && fabsf(end.y - start.y) < 35.0f) {
        /* Clean horizontal / slightly curved connector for Linked List next/prev */
        float y_off = is_left ? 10.0f : -6.0f;
        Vector2 p1 = { start.x + (end.x > start.x ? NODE_RADIUS : -NODE_RADIUS), start.y + y_off };
        Vector2 p2 = { end.x   + (end.x > start.x ? -(NODE_RADIUS + 2.0f) : (NODE_RADIUS + 2.0f)), end.y + y_off };
        DrawLineEx(p1, p2, 2.6f, color);
        draw_arrowhead(p1, p2, color);
        return;
    }

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
            u1*u1*u1*p1.y + 3*u1*u1*t1*c1.y + 3*u1*t1*t1*c2.y + t1*t1*t1*p4.y
        };
        DrawLineEx(a, b, 3.2f, crimson);
    }
    draw_arrowhead(c2, p4, crimson);

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
    Vector2             parent_pos,
    bool                is_left,
    Font                font,
    const ThemePalette *pal
)
{
    float offset_x = is_left ? -54.0f : 54.0f;
    float offset_y = 66.0f;
    Vector2 badge_center = { parent_pos.x + offset_x, parent_pos.y + offset_y };

    Vector2 dir  = Vector2Subtract(badge_center, parent_pos);
    Vector2 norm = Vector2Normalize(dir);
    Vector2 p1   = Vector2Add(parent_pos, Vector2Scale(norm, NODE_RADIUS));
    Vector2 p2   = Vector2Subtract(badge_center, Vector2Scale(norm, 12.0f));

    Color dash_warn  = Fade(pal->accent_warning, 0.80f);
    Color dash_muted = Fade(pal->text_secondary, 0.55f);
    int segments = 9;
    for (int i = 0; i < segments; i += 2) {
        float t0 = (float)i / (float)segments;
        float t1 = (float)(i + 1) / (float)segments;
        Vector2 a = Vector2Lerp(p1, p2, t0);
        Vector2 b = Vector2Lerp(p1, p2, t1);
        DrawLineEx(a, b, 2.0f, (i % 4 == 0) ? dash_warn : dash_muted);
    }

    const char *label = "[ ? não inic. ]";
    Vector2 tsz = MeasureTextEx(font, label, 13.0f, 1.0f);
    float bw = tsz.x + 12.0f;
    float bh = 21.0f;
    Rectangle pill = { floorf(badge_center.x - bw * 0.5f), floorf(badge_center.y - bh * 0.5f), bw, bh };

    DrawRectangleRounded(pill, 0.4f, 6, Fade(pal->bg_elevated, 0.96f));
    DrawRectangleRoundedLinesEx(pill, 0.4f, 6, 1.2f, Fade(pal->accent_warning, 0.85f));
    DrawTextEx(
        font,
        label,
        (Vector2){ floorf(pill.x + 6.0f), floorf(pill.y + (bh - tsz.y) * 0.5f) },
        13.0f,
        1.0f,
        pal->accent_warning
    );
}

static void draw_garbage_or_dangling_hazard(
    Vector2             parent_pos,
    uint64_t            bad_addr,
    bool                is_left,
    bool                is_dangling,
    bool                is_fatal_garbage,
    float               pulse,
    Font                font,
    const ThemePalette *pal
)
{
    if (!is_dangling && !is_fatal_garbage) {
        draw_soft_uninit_pointer(parent_pos, is_left, font, pal);
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
 * 4. 1D VECTOR / ARRAY & INDEX POINTERS RENDERER (SEARCH & SORTING)
 * ============================================================================ */

static void draw_visual_arrays(const TreeSnapshot *snap, Font font, const ThemePalette *pal)
{
    if (!snap || snap->array_count <= 0 || !pal) return;

    float max_node_y = -20.0f;
    for (int i = 0; i < snap->node_count; i++) {
        if (snap->nodes[i].current_pos.y > max_node_y) {
            max_node_y = snap->nodes[i].current_pos.y;
        }
    }
    float base_y = (snap->node_count > 0) ? (max_node_y + 100.0f) : 35.0f;

    const float cell_w = 54.0f;
    const float cell_h = 46.0f;

    for (int a = 0; a < snap->array_count; a++) {
        const VisualArray *va = &snap->arrays[a];
        if (va->count <= 0) continue;

        float total_w = (float)va->count * cell_w;
        float start_x = -total_w * 0.5f;
        float row_y   = base_y + (float)a * 135.0f;

        /* Array Title Badge */
        char title[96];
        snprintf(title, sizeof(title), "int %s[%d]", va->name, va->count);
        DrawTextEx(font, title, (Vector2){ start_x, row_y - 24.0f }, 15.0f, 1.0f, pal->accent_secondary);

        for (int k = 0; k < va->count; k++) {
            float cx = start_x + (float)k * cell_w;
            Rectangle cell = { cx, row_y, cell_w - 3.0f, cell_h };

            bool is_changed = va->changed[k];
            if (is_changed) {
                Rectangle glow = { cell.x - 3.0f, cell.y - 3.0f, cell.width + 6.0f, cell.height + 6.0f };
                DrawRectangleRounded(glow, 0.22f, 6, Fade(pal->accent_warning, 0.22f));
            }

            Color fill_col   = is_changed ? Fade(pal->accent_warning, pal->is_light ? 0.16f : 0.22f) : pal->bg_elevated;
            Color border_col = is_changed ? pal->accent_warning : pal->border;
            DrawRectangleRounded(cell, 0.18f, 6, fill_col);
            DrawRectangleRoundedLinesEx(cell, 0.18f, 6, is_changed ? 2.0f : 1.4f, border_col);

            /* Cell integer value */
            char vstr[32];
            snprintf(vstr, sizeof(vstr), "%d", va->values[k]);
            Vector2 vsz = MeasureTextEx(font, vstr, 17.0f, 1.0f);
            DrawTextEx(
                font,
                vstr,
                (Vector2){ cell.x + (cell.width - vsz.x) * 0.5f, cell.y + (cell.height - vsz.y) * 0.5f },
                17.0f,
                1.0f,
                is_changed ? pal->accent_warning : pal->text_primary
            );

            /* Cell index [k] below */
            char istr[16];
            snprintf(istr, sizeof(istr), "[%d]", k);
            Vector2 isz = MeasureTextEx(font, istr, 12.0f, 1.0f);
            DrawTextEx(
                font,
                istr,
                (Vector2){ cell.x + (cell.width - isz.x) * 0.5f, cell.y + cell.height + 4.0f },
                12.0f,
                1.0f,
                pal->text_secondary
            );

            /* Render any local index variable (i, j, esq, meio, dir, min_idx, etc.) pointing to index k */
            int idx_stack = 0;
            for (int s = 0; s < snap->scalar_var_count; s++) {
                const LocalScalarVar *sv = &snap->scalar_vars[s];
                if (sv->is_int && sv->is_index_candidate && sv->int_val == k) {
                    char badge_txt[64];
                    snprintf(badge_txt, sizeof(badge_txt), "%s=%d", sv->name, k);
                    Vector2 bsz = MeasureTextEx(font, badge_txt, 12.0f, 1.0f);
                    float bw = bsz.x + 12.0f;
                    float bh = 18.0f;
                    float bx = cell.x + cell.width * 0.5f - bw * 0.5f;
                    float by = cell.y + cell.height + 22.0f + (float)idx_stack * 20.0f;

                    Color b_col = (strcmp(sv->name, "meio") == 0 || strcmp(sv->name, "mid") == 0 || strcmp(sv->name, "min_idx") == 0)
                                  ? pal->accent_warning
                                  : ((strcmp(sv->name, "dir") == 0 || strcmp(sv->name, "high") == 0 || strcmp(sv->name, "j") == 0)
                                     ? pal->accent_tertiary
                                     : pal->accent_secondary);

                    Rectangle pill = { bx, by, bw, bh };
                    DrawRectangleRounded(pill, 0.4f, 6, pal->bg_panel);
                    DrawRectangleRoundedLinesEx(pill, 0.4f, 6, 1.3f, b_col);
                    DrawTextEx(font, badge_txt, (Vector2){ bx + 6.0f, by + 3.0f }, 12.0f, 1.0f, b_col);
                    idx_stack++;
                }
            }
        }
    }
}

/* ============================================================================
 * 5. FLOATING OBSERVABILITY HUD (CALL STACK & LOCAL VARIABLES)
 * ============================================================================ */

static bool draw_Observability_hud(
    const DebuggerSession *session,
    WorkspaceUI           *ui,
    Rectangle              canvas_bounds,
    Font                   font,
    bool                   input_blocked
)
{
    if (!ui->config.show_hud || session->state == STATE_IDLE) return false;
    const TreeSnapshot *snap = &session->snapshot;
    const ThemePalette *pal  = &ui->theme;
    Vector2 mouse = GetMousePosition();

    float hud_x = floorf(canvas_bounds.x + 14.0f + 0.5f);
    float hud_y = floorf(canvas_bounds.y + 14.0f + 0.5f);
    float hud_w = floorf(fminf(300.0f, canvas_bounds.width * 0.44f));
    float hdr_h = 30.0f;

    int total_vars = snap->local_var_count + snap->scalar_var_count;
    int frame_rows = (snap->frame_count > 0) ? (snap->frame_count > 5 ? 5 : snap->frame_count) : 1;
    int var_rows   = (total_vars > 0) ? (total_vars > 9 ? 9 : total_vars) : 1;

    float body_h = ui->hud_collapsed ? 0.0f : (48.0f + (float)(frame_rows + var_rows) * 20.0f);
    Rectangle hud_rect = { hud_x, hud_y, hud_w, hdr_h + body_h };
    Rectangle hdr_rect = { hud_x, hud_y, hud_w, hdr_h };

    bool mouse_over_hud = CheckCollisionPointRec(mouse, hud_rect);

    /* Translucent Theme Floating Card */
    DrawRectangleRounded(
        (Rectangle){ hud_rect.x + 3.0f, hud_rect.y + 4.0f, hud_rect.width, hud_rect.height },
        0.08f, 8, (Color){ 0, 0, 0, pal->is_light ? 45 : 110 }
    );
    DrawRectangleRounded(hud_rect, 0.08f, 8, Fade(pal->bg_panel, 0.94f));
    DrawRectangleRoundedLinesEx(hud_rect, 0.08f, 8, 1.2f, pal->border);

    /* Header Toggle */
    bool hdr_hover = !input_blocked && CheckCollisionPointRec(mouse, hdr_rect);
    if (hdr_hover && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->hud_collapsed = !ui->hud_collapsed;
    }

    char hdr_txt[96];
    snprintf(
        hdr_txt,
        sizeof(hdr_txt),
        tr(STR_HUD_HEADER_FMT),
        ui->hud_collapsed ? "▸" : "▾",
        snap->frame_count > 0 ? snap->frame_count : 1,
        total_vars
    );
    DrawTextEx(
        font,
        hdr_txt,
        (Vector2){ floorf(hdr_rect.x + 10.0f + 0.5f), floorf(hdr_rect.y + 8.0f + 0.5f) },
        13.0f,
        1.0f,
        hdr_hover ? pal->accent_secondary : pal->text_primary
    );

    if (ui->hud_collapsed) return mouse_over_hud;

    BeginScissorMode((int)hud_rect.x + 2, (int)(hud_rect.y + hdr_h), (int)hud_rect.width - 4, (int)body_h - 4);
    float y = floorf(hud_rect.y + hdr_h + 4.0f + 0.5f);

    /* Section 1: Call Stack */
    DrawLineEx((Vector2){ hud_x + 8.0f, y }, (Vector2){ hud_x + hud_w - 8.0f, y }, 1.0f, pal->border);
    y += 4.0f;
    DrawTextEx(font, tr(STR_HUD_CALL_STACK), (Vector2){ hud_x + 10.0f, y }, 11.0f, 1.0f, pal->text_secondary);
    y += 16.0f;

    if (snap->frame_count == 0) {
        char f0[96];
        snprintf(f0, sizeof(f0), "#0 %s() : L%d", snap->current_func[0] ? snap->current_func : "main", snap->current_executing_line);
        DrawTextEx(font, f0, (Vector2){ hud_x + 12.0f, y }, 13.0f, 1.0f, pal->accent_warning);
        y += 20.0f;
    } else {
        for (int f = 0; f < frame_rows; f++) {
            const StackFrameInfo *sf = &snap->frames[f];
            char fline[112];
            snprintf(fline, sizeof(fline), "#%d %s()  L%d", sf->level, sf->func, sf->line);
            Color fcol = (f == 0) ? pal->accent_warning : pal->text_secondary;
            if (f == 0) {
                DrawRectangleRounded((Rectangle){ hud_x + 8.0f, y - 2.0f, hud_w - 16.0f, 18.0f }, 0.25f, 4, Fade(pal->accent_warning, 0.16f));
            }
            DrawTextEx(font, fline, (Vector2){ hud_x + 12.0f, y }, 13.0f, 1.0f, fcol);
            y += 20.0f;
        }
    }

    /* Section 2: Local Variables (Pointers + Scalars) */
    y += 4.0f;
    DrawLineEx((Vector2){ hud_x + 8.0f, y }, (Vector2){ hud_x + hud_w - 8.0f, y }, 1.0f, pal->border);
    y += 4.0f;
    DrawTextEx(font, tr(STR_HUD_LOCAL_VARS), (Vector2){ hud_x + 10.0f, y }, 11.0f, 1.0f, pal->text_secondary);
    y += 16.0f;

    int drawn_vars = 0;
    if (total_vars == 0) {
        DrawTextEx(font, tr(STR_HUD_NO_VARS), (Vector2){ hud_x + 12.0f, y }, 13.0f, 1.0f, pal->text_secondary);
    } else {
        /* Draw Pointer variables first */
        for (int i = 0; i < snap->local_var_count && drawn_vars < 9; i++) {
            const LocalPointerVar *lv = &snap->local_vars[i];
            if (lv->changed) {
                DrawRectangleRounded((Rectangle){ hud_x + 8.0f, y - 2.0f, hud_w - 16.0f, 18.0f }, 0.25f, 4, Fade(pal->accent_warning, 0.18f));
            }
            char vline[128];
            if (lv->is_null) {
                snprintf(vline, sizeof(vline), "%s* %s = NULL", lv->changed ? "● " : "", lv->name);
            } else if (lv->is_garbage) {
                snprintf(vline, sizeof(vline), "%s* %s = [? não inic.]", lv->changed ? "● " : "", lv->name);
            } else if (ui->config.show_memory_addresses) {
                snprintf(vline, sizeof(vline), "%s* %s = 0x%" PRIx64, lv->changed ? "● " : "", lv->name, (uint64_t)(lv->addr & 0xFFFFFFU));
            } else {
                snprintf(vline, sizeof(vline), "%s* %s -> [Nó]", lv->changed ? "● " : "", lv->name);
            }
            Color vcol = lv->changed ? pal->accent_warning :
                         (lv->is_null ? pal->text_secondary : pal->accent_secondary);
            DrawTextEx(font, vline, (Vector2){ hud_x + 12.0f, y }, 13.0f, 1.0f, vcol);
            y += 20.0f;
            drawn_vars++;
        }

        /* Draw Scalar/Primitive variables */
        for (int i = 0; i < snap->scalar_var_count && drawn_vars < 9; i++) {
            const LocalScalarVar *sv = &snap->scalar_vars[i];
            if (sv->changed) {
                DrawRectangleRounded((Rectangle){ hud_x + 8.0f, y - 2.0f, hud_w - 16.0f, 18.0f }, 0.25f, 4, Fade(pal->accent_warning, 0.18f));
            }
            char sline[128];
            snprintf(sline, sizeof(sline), "%s%s = %s", sv->changed ? "● " : "", sv->name, sv->value_str);
            Color scol = sv->changed ? pal->accent_warning : pal->text_primary;
            DrawTextEx(font, sline, (Vector2){ hud_x + 12.0f, y }, 13.0f, 1.0f, scol);
            y += 20.0f;
            drawn_vars++;
        }
    }

    EndScissorMode();
    return mouse_over_hud;
}

/* ============================================================================
 * 6. FLOATING CAMERA PILL ([ ⌖ | - 100% + ]) IN BOTTOM-LEFT OF CANVAS
 * ============================================================================ */

static bool draw_floating_camera_pill(
    const TreeSnapshot *snap,
    WorkspaceUI        *ui,
    Rectangle           canvas_bounds,
    Font                font,
    bool                input_blocked
)
{
    const ThemePalette *pal = &ui->theme;
    Vector2 mouse = GetMousePosition();
    float pill_w = 186.0f;
    float pill_h = 32.0f;
    float visible_bottom = canvas_bounds.y + canvas_bounds.height - ui->terminal_anim_h;
    Rectangle pill = {
        canvas_bounds.x + 14.0f,
        fmaxf(canvas_bounds.y + 14.0f, floorf(visible_bottom - pill_h - 14.0f + 0.5f)),
        pill_w,
        pill_h
    };

    bool over_pill = CheckCollisionPointRec(mouse, pill);

    DrawRectangleRounded(
        (Rectangle){ pill.x + 2.0f, pill.y + 3.0f, pill.width, pill.height },
        0.45f, 8, (Color){ 0, 0, 0, pal->is_light ? 45 : 120 }
    );
    DrawRectangleRounded(pill, 0.45f, 8, Fade(pal->bg_panel, 0.94f));
    DrawRectangleRoundedLinesEx(pill, 0.45f, 8, 1.2f, pal->border);

    /* 1. [ ⌖ ] Auto-Focus Toggle Button */
    Rectangle btn_focus = { pill.x + 4.0f, pill.y + 4.0f, 44.0f, pill_h - 8.0f };
    bool hov_focus = !input_blocked && CheckCollisionPointRec(mouse, btn_focus);
    Color focus_bg = ui->camera_auto_focus
                     ? pal->accent_primary
                     : (hov_focus ? pal->bg_hover : pal->bg_elevated);
    Color focus_fg = ui->camera_auto_focus ? pal->accent_primary_fg : pal->text_primary;
    DrawRectangleRounded(btn_focus, 0.35f, 6, focus_bg);
    Vector2 fsz = MeasureTextEx(font, "Auto", 13.0f, 1.0f);
    DrawTextEx(
        font,
        "Auto",
        (Vector2){ floorf(btn_focus.x + (btn_focus.width - fsz.x) * 0.5f + 0.5f), floorf(btn_focus.y + (btn_focus.height - fsz.y) * 0.5f + 0.5f) },
        13.0f,
        1.0f,
        focus_fg
    );
    if (hov_focus && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->camera_auto_focus = !ui->camera_auto_focus;
        if (ui->camera_auto_focus) {
            layout_center_camera(ui, snap, canvas_bounds);
        }
    }

    /* Divider */
    DrawLineEx(
        (Vector2){ pill.x + 53.0f, pill.y + 6.0f },
        (Vector2){ pill.x + 53.0f, pill.y + pill_h - 6.0f },
        1.0f, pal->border
    );

    /* 2. [ - ] Zoom Out */
    Rectangle btn_minus = { pill.x + 58.0f, pill.y + 4.0f, 26.0f, pill_h - 8.0f };
    bool hov_minus = !input_blocked && CheckCollisionPointRec(mouse, btn_minus);
    DrawRectangleRounded(btn_minus, 0.35f, 4, hov_minus ? pal->bg_hover : pal->bg_elevated);
    Vector2 msz = MeasureTextEx(font, "-", 13.0f, 1.0f);
    DrawTextEx(
        font,
        "-",
        (Vector2){ floorf(btn_minus.x + (btn_minus.width - msz.x) * 0.5f + 0.5f), floorf(btn_minus.y + (btn_minus.height - msz.y) * 0.5f + 0.5f) },
        13.0f,
        1.0f,
        pal->text_primary
    );
    if (hov_minus && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->camera_auto_focus = false;
        ui->camera.zoom = fmaxf(0.30f, ui->camera.zoom - 0.15f);
    }

    /* 3. [ 100% ] Zoom Reset */
    Rectangle btn_pct = { pill.x + 87.0f, pill.y + 4.0f, 64.0f, pill_h - 8.0f };
    bool hov_pct = !input_blocked && CheckCollisionPointRec(mouse, btn_pct);
    if (hov_pct) {
        DrawRectangleRounded(btn_pct, 0.30f, 4, pal->bg_hover);
    }
    char ztxt[24];
    snprintf(ztxt, sizeof(ztxt), "%d%%", (int)roundf(ui->camera.zoom * 100.0f));
    Vector2 zsz = MeasureTextEx(font, ztxt, 13.0f, 1.0f);
    DrawTextEx(
        font,
        ztxt,
        (Vector2){ floorf(btn_pct.x + (btn_pct.width - zsz.x) * 0.5f + 0.5f), floorf(btn_pct.y + (btn_pct.height - zsz.y) * 0.5f + 0.5f) },
        13.0f,
        1.0f,
        pal->text_primary
    );
    if (hov_pct && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->camera_auto_focus = false;
        layout_center_camera(ui, snap, canvas_bounds);
        ui->camera.zoom = 1.0f;
    }

    /* 4. [ + ] Zoom In */
    Rectangle btn_plus = { pill.x + 154.0f, pill.y + 4.0f, 26.0f, pill_h - 8.0f };
    bool hov_plus = !input_blocked && CheckCollisionPointRec(mouse, btn_plus);
    DrawRectangleRounded(btn_plus, 0.35f, 4, hov_plus ? pal->bg_hover : pal->bg_elevated);
    Vector2 psz = MeasureTextEx(font, "+", 13.0f, 1.0f);
    DrawTextEx(
        font,
        "+",
        (Vector2){ floorf(btn_plus.x + (btn_plus.width - psz.x) * 0.5f + 0.5f), floorf(btn_plus.y + (btn_plus.height - psz.y) * 0.5f + 0.5f) },
        13.0f,
        1.0f,
        pal->text_primary
    );
    if (hov_plus && IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        ui->camera_auto_focus = false;
        ui->camera.zoom = fminf(2.60f, ui->camera.zoom + 0.15f);
    }

    return over_pill;
}

/* ============================================================================
 * 7. INTERACTIVE DECISION-TREE "BLOB" MASCOT (NO TOP '?' CLUTTER)
 * ============================================================================ */

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
                DrawTextEx(font, word, (Vector2){ floorf(x + 0.5f), floorf(y + 0.5f) }, font_size, 1.0f, color);
            }
            x += wsz.x + space_w;
            word = strtok_r(NULL, " ", &save_word);
        }
        y += font_size + 5.0f;
        line = strtok_r(NULL, "\n", &save_line);
    }
}

static bool draw_blob_choice_btn(Rectangle bounds, const char *label, Font font, Color accent, const ThemePalette *pal, bool input_blocked)
{
    Vector2 mouse = GetMousePosition();
    bool hov = !input_blocked && CheckCollisionPointRec(mouse, bounds);
    Color bg = hov ? pal->bg_hover : pal->bg_elevated;
    Color bd = hov ? accent : pal->border;
    DrawRectangleRounded(bounds, 0.28f, 6, bg);
    DrawRectangleRoundedLinesEx(bounds, 0.28f, 6, 1.2f, bd);
    Vector2 tsz = MeasureTextEx(font, label, 13.0f, 1.0f);
    DrawTextEx(
        font,
        label,
        (Vector2){ floorf(bounds.x + 10.0f + 0.5f), floorf(bounds.y + (bounds.height - tsz.y) * 0.5f + 0.5f) },
        13.0f,
        1.0f,
        pal->text_primary
    );
    return (hov && IsMouseButtonReleased(MOUSE_LEFT_BUTTON));
}

static void draw_avora_blob_decision_tree(
    const DebuggerSession *session,
    WorkspaceUI           *ui,
    Rectangle              canvas_bounds,
    Font                   font,
    bool                   input_blocked
)
{
    if (!ui->config.show_blob) {
        ui->blob_mascot_bounds = (Rectangle){ 0, 0, 0, 0 };
        ui->blob_card_bounds   = (Rectangle){ 0, 0, 0, 0 };
        return;
    }

    const ThemePalette *pal = &ui->theme;
    float dt = GetFrameTime();
    float t  = (float)GetTime();

    /* Automatically open Blob in Error Decision-Tree mode when a new error occurs */
    bool has_error = (session->diagnostic.error_type != ERR_NONE);
    if (session->diagnostic.error_type != ui->last_seen_error_type) {
        ui->last_seen_error_type = session->diagnostic.error_type;
        if (has_error) {
            ui->blob_error_dismissed = false;
            ui->active_blob_topic    = BLOB_HINT_ERR_ROOT;
            ui->blob_anim_progress   = 0.10f;
        }
    }

    if (ui->active_blob_topic != BLOB_HINT_NONE) {
        ui->displayed_blob_topic = ui->active_blob_topic;
        ui->blob_anim_progress  += (1.0f - ui->blob_anim_progress) * avora_exp_decay(12.0f, dt, ui->config.reduce_animations);
        if (ui->blob_anim_progress > 0.995f) ui->blob_anim_progress = 1.0f;
    } else {
        ui->blob_anim_progress  += (0.0f - ui->blob_anim_progress) * avora_exp_decay(15.0f, dt, ui->config.reduce_animations);
        if (ui->blob_anim_progress < 0.005f) ui->blob_anim_progress = 0.0f;
    }

    Vector2 mouse = GetMousePosition();
    float visible_bottom = canvas_bounds.y + canvas_bounds.height - ui->terminal_anim_h;
    Vector2 awake_pos = {
        canvas_bounds.x + canvas_bounds.width - 48.0f,
        fmaxf(canvas_bounds.y + 46.0f, visible_bottom - 46.0f)
    };
    Vector2 ball_pos = {
        canvas_bounds.x + canvas_bounds.width - 34.0f,
        fmaxf(canvas_bounds.y + 34.0f, visible_bottom - 34.0f)
    };

    ui->blob_mascot_bounds = (Rectangle){ awake_pos.x - 36.0f, awake_pos.y - 40.0f, 72.0f, 74.0f };

    float dist_to_mouse = Vector2Distance(mouse, awake_pos);
    bool  mouse_near    = (dist_to_mouse < 85.0f);
    bool  is_busy       = (ui->active_blob_topic != BLOB_HINT_NONE) ||
                          (session->state == STATE_RUNNING) ||
                          (session->state == STATE_STEPPING);

    if (mouse_near || is_busy) {
        ui->blob_idle_timer = 0.0f;
    } else {
        ui->blob_idle_timer += dt;
    }

    float target_awake = (ui->blob_idle_timer < 4.5f) ? 1.0f : 0.0f;
    float anim_speed   = (target_awake > ui->blob_awake_anim) ? 8.5f : 4.2f;
    ui->blob_awake_anim += (target_awake - ui->blob_awake_anim) * avora_exp_decay(anim_speed, dt, ui->config.reduce_animations);
    if (ui->blob_awake_anim < 0.0f) ui->blob_awake_anim = 0.0f;
    if (ui->blob_awake_anim > 1.0f) ui->blob_awake_anim = 1.0f;

    float a = ui->blob_awake_anim;
    float bounce = sinf(a * 3.14159265f) * 0.16f;
    Vector2 blob_center = Vector2Lerp(ball_pos, awake_pos, a);

    float breath_awake = sinf(t * 3.5f);
    float breath_ball  = sinf(t * 2.1f);
    float rx_awake = 25.0f + breath_awake * 1.8f;
    float ry_awake = 23.0f - breath_awake * 1.6f;
    float r_ball   = 12.5f + breath_ball * 0.9f;
    float rx = r_ball + (rx_awake - r_ball) * a + bounce * 6.0f;
    float ry = r_ball + (ry_awake - r_ball) * a - bounce * 4.0f;

    /* Shadow & Emerald/Crimson Aura */
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

    /* Sprout Stem & Leaves */
    Vector2 head_top = { blob_center.x, blob_center.y - ry };
    float sway = sinf(t * (2.2f + 2.0f * a)) * (1.2f + 2.0f * a);
    float stem_h = 4.0f + 7.5f * a;
    Vector2 stem_top = { head_top.x + sway, head_top.y - stem_h };
    DrawLineEx(head_top, stem_top, 2.2f, (Color){ 52, 211, 153, 255 });
    float leaf_rx = 3.8f + 2.8f * a;
    float leaf_ry = 2.2f + 1.4f * a;
    DrawEllipse((int)(stem_top.x - (3.5f + 2.5f * a)), (int)(stem_top.y - 1.5f), leaf_rx, leaf_ry, (Color){ 16, 185, 129, 255 });
    DrawEllipse((int)(stem_top.x + (3.5f + 2.5f * a)), (int)(stem_top.y - 1.0f), leaf_rx, leaf_ry, (Color){ 52, 211, 153, 255 });

    /* Emerald Green Body */
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
        float eye_dx = 4.2f;
        Vector2 l_eye = { blob_center.x - eye_dx, blob_center.y + 0.5f };
        Vector2 r_eye = { blob_center.x + eye_dx, blob_center.y + 0.5f };
        DrawLineEx((Vector2){ l_eye.x - 2.5f, l_eye.y }, (Vector2){ l_eye.x + 2.5f, l_eye.y }, 1.8f, (Color){ 209, 250, 229, 225 });
        DrawLineEx((Vector2){ r_eye.x - 2.5f, r_eye.y }, (Vector2){ r_eye.x + 2.5f, r_eye.y }, 1.8f, (Color){ 209, 250, 229, 225 });
    } else {
        Vector2 target_dir = Vector2Subtract(mouse, blob_center);
        if (Vector2Length(target_dir) > 1.0f) {
            target_dir = Vector2Scale(Vector2Normalize(target_dir), 2.6f * a);
        }
        ui->blob_eye_look = Vector2Lerp(ui->blob_eye_look, target_dir, avora_exp_decay(14.0f, dt, ui->config.reduce_animations));
        ui->blob_blink_timer += dt;
        bool blinking = (fmodf(ui->blob_blink_timer, 4.1f) > 3.92f);

        float eye_sep = 8.5f * (0.65f + 0.35f * a);
        Vector2 left_eye  = { blob_center.x - eye_sep, blob_center.y - 1.5f };
        Vector2 right_eye = { blob_center.x + eye_sep, blob_center.y - 1.5f };

        if (!has_error) {
            DrawEllipse((int)(left_eye.x - 3.5f),  (int)(left_eye.y + 6.0f),  3.6f * a, 2.1f * a, (Color){ 251, 113, 133, 115 });
            DrawEllipse((int)(right_eye.x + 3.5f), (int)(right_eye.y + 6.0f), 3.6f * a, 2.1f * a, (Color){ 251, 113, 133, 115 });
        }

        if (has_error) {
            DrawCircleV(left_eye,  5.2f, WHITE);
            DrawCircleV(right_eye, 5.2f, WHITE);
            DrawCircleV(Vector2Add(left_eye,  ui->blob_eye_look), 2.2f, (Color){ 15, 17, 26, 255 });
            DrawCircleV(Vector2Add(right_eye, ui->blob_eye_look), 2.2f, (Color){ 15, 17, 26, 255 });
            DrawCircle((int)blob_center.x, (int)(blob_center.y + 8.5f), 3.5f, (Color){ 15, 17, 26, 240 });
        } else if (blinking) {
            DrawLineEx((Vector2){ left_eye.x - 4.0f, left_eye.y }, (Vector2){ left_eye.x + 4.0f, left_eye.y }, 2.0f, WHITE);
            DrawLineEx((Vector2){ right_eye.x - 4.0f, right_eye.y }, (Vector2){ right_eye.x + 4.0f, right_eye.y }, 2.0f, WHITE);
        } else {
            float eye_r = 4.8f * (0.7f + 0.3f * a);
            DrawCircleV(left_eye,  eye_r, WHITE);
            DrawCircleV(right_eye, eye_r, WHITE);
            Vector2 l_pupil = Vector2Add(left_eye,  ui->blob_eye_look);
            Vector2 r_pupil = Vector2Add(right_eye, ui->blob_eye_look);
            DrawCircleV(l_pupil, 2.5f, (Color){ 15, 17, 26, 255 });
            DrawCircleV(r_pupil, 2.5f, (Color){ 15, 17, 26, 255 });
            DrawCircleV((Vector2){ l_pupil.x - 0.9f, l_pupil.y - 0.9f }, 0.9f, WHITE);
            DrawCircleV((Vector2){ r_pupil.x - 0.9f, r_pupil.y - 0.9f }, 0.9f, WHITE);

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

    Vector2 lbl_sz = MeasureTextEx(font, "Blob (?)", 13.0f, 1.0f);
    DrawTextEx(
        font,
        "Blob (?)",
        (Vector2){ floorf(blob_center.x - lbl_sz.x * 0.5f + 0.5f), floorf(blob_center.y + ry + 3.0f + 0.5f) },
        13.0f,
        1.0f,
        pal->is_light ? pal->accent_primary : (Color){ 167, 243, 208, (unsigned char)(140 + (int)(110.0f * a)) }
    );

    /* Render Floating Interactive Decision-Tree Card */
    ui->blob_card_bounds = (Rectangle){ 0, 0, 0, 0 };
    bool clicked_choice = false;

    if (ui->blob_anim_progress > 0.01f) {
        float scale = ui->config.reduce_animations ? 1.0f : ease_out_back(ui->blob_anim_progress);
        float bw = fminf(390.0f, canvas_bounds.width - 28.0f);
        float bh = 212.0f;

        float sw_scaled = bw * scale;
        float sh_scaled = bh * scale;

        Vector2 anchor = { blob_center.x - 12.0f, blob_center.y - 44.0f };
        Rectangle card = {
            anchor.x - sw_scaled,
            anchor.y - sh_scaled,
            sw_scaled,
            sh_scaled
        };
        if (card.x < canvas_bounds.x + 12.0f) card.x = canvas_bounds.x + 12.0f;
        if (card.y < canvas_bounds.y + 12.0f) card.y = canvas_bounds.y + 12.0f;
        ui->blob_card_bounds = card;

        bool is_err_topic = (ui->displayed_blob_topic == BLOB_HINT_ERR_ROOT ||
                             ui->displayed_blob_topic == BLOB_HINT_ERR_WHY  ||
                             ui->displayed_blob_topic == BLOB_HINT_ERR_FIX);

        Color accent = is_err_topic ? pal->accent_error : pal->accent_primary;
        Color bg_col = Fade(pal->bg_panel, 0.98f);

        DrawRectangleRounded(
            (Rectangle){ card.x + 4.0f, card.y + 5.0f, card.width, card.height },
            0.09f, 8, (Color){ 0, 0, 0, pal->is_light ? 55 : 165 }
        );
        DrawRectangleRounded(card, 0.09f, 8, bg_col);
        DrawRectangleRoundedLinesEx(card, 0.09f, 8, 1.6f, accent);

        if (scale > 0.72f) {
            const char *title = "";
            char body_buf[768] = { 0 };
            const char *opt1_lbl = NULL, *opt2_lbl = NULL, *opt3_lbl = NULL;
            BlobHintTopic opt1_next = BLOB_HINT_NONE, opt2_next = BLOB_HINT_NONE, opt3_next = BLOB_HINT_NONE;
            bool opt3_opens_terminal = false;

            switch (ui->displayed_blob_topic) {
                case BLOB_HINT_BUILD_TREE:
                    title = "🌳 Como usar BUILD_TREE & Listas";
                    snprintf(body_buf, sizeof(body_buf),
                             "Na funcao main(), use BUILD_TREE(\"[10, 5, 15, null, 7]\") para criar arvores binarias em largura (BFS), "
                             "ou new_node(valor) para alocar nos manualmente.");
                    opt1_lbl  = "Como criar Florestas ou Listas Encadeadas?";
                    opt1_next = BLOB_HINT_BUILD_TREE_FOREST;
                    opt2_lbl  = "⬅ Voltar ao Menu Inicial";
                    opt2_next = BLOB_HINT_MASCOT;
                    break;

                case BLOB_HINT_BUILD_TREE_FOREST:
                    title = "🌲 Florestas & Listas (BUILD_LIST)";
                    snprintf(body_buf, sizeof(body_buf),
                             "- Florestas: Crie 2 ou mais raizes no main() (ex: arvore1 e arvore2) e o AVORA desenha lado a lado!\n"
                             "- Listas: Use struct ListNode* l = BUILD_LIST(\"[10, 20, 30]\"); para listas com ->next e ->prev!");
                    opt1_lbl  = "⬅ Voltar para BUILD_TREE";
                    opt1_next = BLOB_HINT_BUILD_TREE;
                    opt2_lbl  = "🏠 Menu Inicial do Blob";
                    opt2_next = BLOB_HINT_MASCOT;
                    break;

                case BLOB_HINT_LEETCODE:
                    title = "⚡ Modo LeetCode (Com ou Sem main)";
                    snprintf(body_buf, sizeof(body_buf),
                             "Configure seus casos de teste diretamente na main() do editor! "
                             "Se voce colar apenas a funcao do LeetCode sem main(), o AVORA gera uma main() automatica de fallback.");
                    opt1_lbl  = "Como visualizar Vetores e Busca/Ordenacao?";
                    opt1_next = BLOB_HINT_LEETCODE_ARRAYS;
                    opt2_lbl  = "⬅ Voltar ao Menu Inicial";
                    opt2_next = BLOB_HINT_MASCOT;
                    break;

                case BLOB_HINT_LEETCODE_ARRAYS:
                    title = "📊 Vetores, Índices e Ordenação";
                    snprintf(body_buf, sizeof(body_buf),
                             "Declare vetores locais (ex: int vetor[7] = {...}) e indices (i, j, esq, meio, dir). "
                             "O AVORA desenha as celulas no Canvas com selos apontando para cada indice e destaca trocas em dourado!");
                    opt1_lbl  = "⬅ Voltar para Modo LeetCode";
                    opt1_next = BLOB_HINT_LEETCODE;
                    opt2_lbl  = "🏠 Menu Inicial do Blob";
                    opt2_next = BLOB_HINT_MASCOT;
                    break;

                case BLOB_HINT_POINTERS:
                    title = "🧭 Guia Visual de Ponteiros & Heap";
                    snprintf(body_buf, sizeof(body_buf),
                             "- Ciano (left/prev) e Roxo (right/next): Ponteiros validos.\n"
                             "- Halo Dourado (#F59E0B): No em foco ou variavel alterada.\n"
                             "- Curva Carmesim [! CICLO] e Raio Laranja: Ciclo ou Lixo!");
                    opt1_lbl  = "O que significa [ ? não inic. ]?";
                    opt1_next = BLOB_HINT_POINTERS_UNINIT;
                    opt2_lbl  = "⬅ Voltar ao Menu Inicial";
                    opt2_next = BLOB_HINT_MASCOT;
                    break;

                case BLOB_HINT_POINTERS_UNINIT:
                    title = "🛠️ Ponteiros Em Construção [ ? não inic. ]";
                    snprintf(body_buf, sizeof(body_buf),
                             "Quando voce chama malloc(), left e right nascem com lixo (0xBE). "
                             "O AVORA mostra '[ ? não inic. ]' sem travar para voce avançar ate as linhas que atribuem NULL ou filhos!");
                    opt1_lbl  = "⬅ Voltar para Guia de Ponteiros";
                    opt1_next = BLOB_HINT_POINTERS;
                    opt2_lbl  = "🏠 Menu Inicial do Blob";
                    opt2_next = BLOB_HINT_MASCOT;
                    break;

                case BLOB_HINT_ERR_ROOT:
                    title = session->diagnostic.pedagogical_title[0]
                            ? session->diagnostic.pedagogical_title
                            : "⚠️ O Blob detectou uma falha na execução!";
                    snprintf(body_buf, sizeof(body_buf),
                             "Ocorreu uma anomalia na Linha %d. O que voce gostaria de investigar agora?",
                             session->diagnostic.fault_line);
                    opt1_lbl  = "🔍 Por que isso aconteceu nessa linha?";
                    opt1_next = BLOB_HINT_ERR_WHY;
                    opt2_lbl  = "🛠️ Como corrigir esse problema em C?";
                    opt2_next = BLOB_HINT_ERR_FIX;
                    opt3_lbl  = "💻 Ver log bruto no Terminal ▲";
                    opt3_opens_terminal = true;
                    break;

                case BLOB_HINT_ERR_WHY:
                    title = "🔍 Causa Raiz na Memória / Compilador";
                    snprintf(body_buf, sizeof(body_buf), "%s", session->diagnostic.pedagogical_explanation);
                    opt1_lbl  = "🛠️ Como corrigir esse problema?";
                    opt1_next = BLOB_HINT_ERR_FIX;
                    opt2_lbl  = "💻 Ver saída bruta do GCC/ASan no Terminal";
                    opt2_next = BLOB_HINT_ERR_ROOT;
                    opt3_lbl  = "⬅ Voltar ao Resumo do Erro";
                    opt3_next = BLOB_HINT_ERR_ROOT;
                    break;

                case BLOB_HINT_ERR_FIX:
                    title = "🛠️ Como Corrigir no Código C";
                    if (session->diagnostic.error_type == ERR_SIGSEGV_NULL) {
                        snprintf(body_buf, sizeof(body_buf),
                                 "Adicione uma verificacao de caso base antes de acessar '->val' ou '->left':\n"
                                 "if (raiz == NULL) return 0;");
                    } else if (session->diagnostic.error_type == ERR_SIGSEGV_GARBAGE) {
                        snprintf(body_buf, sizeof(body_buf),
                                 "Logo apos alocar com malloc(), inicialize explicitamente os ponteiros:\n"
                                 "novo->left = NULL; novo->right = NULL;");
                    } else if (session->diagnostic.error_type == ERR_CYCLE_DETECTED) {
                        snprintf(body_buf, sizeof(body_buf),
                                 "Revise a ordem das atribuicoes na rotacao/insercao: use um ponteiro auxiliar (ex: temp) "
                                 "para que um filho nunca aponte para seu proprio ancestral.");
                    } else {
                        snprintf(body_buf, sizeof(body_buf),
                                 "Confira a linha %d destacada em vermelho no editor e abra o Terminal no rodape para ver a linha exata apontada pelo GCC.",
                                 session->diagnostic.fault_line);
                    }
                    opt1_lbl  = "🔍 Ver explicação da causa raiz";
                    opt1_next = BLOB_HINT_ERR_WHY;
                    opt2_lbl  = "🏠 Ir para o Menu de Dicas do Blob";
                    opt2_next = BLOB_HINT_MASCOT;
                    break;

                case BLOB_HINT_STDIN_DETECTED:
                    title = "⌨ Detectei leitura de entrada (scanf / stdin)!";
                    snprintf(body_buf, sizeof(body_buf),
                             "Detectei leitura de entrada (scanf/fgets/getchar) no seu codigo, mas a Entrada (stdin) esta desativada. "
                             "Deseja ativar a Entrada (stdin) no Terminal agora?");
                    opt1_lbl  = "✅ Sim, ativar Entrada (stdin) no Terminal";
                    opt1_next = BLOB_HINT_NONE;
                    opt2_lbl  = "Ignorar (usar stdin vazio / EOF imediato)";
                    opt2_next = BLOB_HINT_NONE;
                    break;

                case BLOB_HINT_MASCOT:
                default:
                    title = "👋 Oi! Eu sou o Blob, seu guia visual!";
                    snprintf(body_buf, sizeof(body_buf),
                             "Escolha um topico abaixo para explorar como depurar arvores, listas, vetores e ponteiros no AVORA:");
                    opt1_lbl  = "🌳 Como usar BUILD_TREE & Listas Encadeadas";
                    opt1_next = BLOB_HINT_BUILD_TREE;
                    opt2_lbl  = "⚡ Modo LeetCode, Vetores & Ordenação";
                    opt2_next = BLOB_HINT_LEETCODE;
                    opt3_lbl  = "🧭 Guia Visual de Ponteiros & Memória Heap";
                    opt3_next = BLOB_HINT_POINTERS;
                    break;
            }

            BeginScissorMode((int)card.x + 4, (int)card.y + 4, (int)card.width - 8, (int)card.height - 8);
            DrawTextEx(
                ui->font_ui.texture.id ? ui->font_ui : font,
                title,
                (Vector2){ floorf(card.x + 12.0f + 0.5f), floorf(card.y + 9.0f + 0.5f) },
                15.0f,
                1.0f,
                accent
            );

            int btn_count = (opt1_lbl ? 1 : 0) + (opt2_lbl ? 1 : 0) + (opt3_lbl ? 1 : 0);
            float btns_total_h = (float)btn_count * 27.0f + 6.0f;

            Rectangle text_box = {
                card.x + 12.0f,
                card.y + 32.0f,
                card.width - 24.0f,
                card.height - 38.0f - btns_total_h
            };
            draw_wrapped_bubble_text(font, body_buf, text_box, 13.0f, pal->text_primary);

            float by = card.y + card.height - btns_total_h - 4.0f;
            if (opt1_lbl) {
                if (draw_blob_choice_btn((Rectangle){ card.x + 12.0f, by, card.width - 24.0f, 23.0f }, opt1_lbl, font, accent, pal, input_blocked)) {
                    if (ui->displayed_blob_topic == BLOB_HINT_STDIN_DETECTED) {
                        ui->config.enable_stdin   = true;
                        config_save(&ui->config);
                        ui->terminal_open         = true;
                        ui->terminal_active_tab   = 1;
                        ui->stdin_textbox_focused = true;
                        audio_play_sfx(SFX_UI_CLICK, &ui->config, 0.08f);
                    }
                    ui->active_blob_topic = opt1_next;
                    clicked_choice = true;
                }
                by += 26.0f;
            }
            if (opt2_lbl) {
                if (draw_blob_choice_btn((Rectangle){ card.x + 12.0f, by, card.width - 24.0f, 23.0f }, opt2_lbl, font, accent, pal, input_blocked)) {
                    if (ui->displayed_blob_topic == BLOB_HINT_ERR_WHY) {
                        ui->terminal_open       = true;
                        ui->terminal_active_tab = 0;
                    }
                    ui->active_blob_topic = opt2_next;
                    clicked_choice = true;
                }
                by += 26.0f;
            }
            if (opt3_lbl) {
                if (draw_blob_choice_btn((Rectangle){ card.x + 12.0f, by, card.width - 24.0f, 23.0f }, opt3_lbl, font, accent, pal, input_blocked)) {
                    if (opt3_opens_terminal) {
                        ui->terminal_open       = true;
                        ui->terminal_active_tab = 0;
                        ui->active_blob_topic   = BLOB_HINT_NONE;
                        ui->blob_error_dismissed = true;
                    } else {
                        ui->active_blob_topic = opt3_next;
                    }
                    clicked_choice = true;
                }
            }
            EndScissorMode();
        }
    }

    /* Click handling: click on Blob toggles card; click outside card & Blob closes card */
    if (!input_blocked && IsMouseButtonReleased(MOUSE_LEFT_BUTTON) && !clicked_choice) {
        bool on_blob = CheckCollisionPointRec(mouse, ui->blob_mascot_bounds);
        bool on_card = (ui->blob_anim_progress > 0.05f) && CheckCollisionPointRec(mouse, ui->blob_card_bounds);
        if (on_blob) {
            ui->blob_idle_timer = 0.0f;
            if (ui->active_blob_topic != BLOB_HINT_NONE) {
                ui->active_blob_topic    = BLOB_HINT_NONE;
                ui->blob_error_dismissed = true;
            } else {
                ui->active_blob_topic  = has_error ? BLOB_HINT_ERR_ROOT : BLOB_HINT_MASCOT;
                ui->blob_anim_progress = 0.05f;
            }
        } else if (!on_card && ui->active_blob_topic != BLOB_HINT_NONE) {
            ui->active_blob_topic    = BLOB_HINT_NONE;
            ui->blob_error_dismissed = true;
        }
    }
}

/* ============================================================================
 * 8. PRESENTATION / STAGE MODE CONCATENATED OVERLAY (DIAGNOSTICS + STDOUT + CONTROLS)
 * ============================================================================ */

static void draw_presentation_stage_overlay(
    const DebuggerSession *session,
    const WorkspaceUI     *ui,
    Rectangle              canvas_bounds,
    Font                   ui_font,
    Font                   sm_font
)
{
    if (!ui->config.presentation_mode) return;

    const ThemePalette *pal  = &ui->theme;
    const TreeSnapshot *snap = &session->snapshot;

    /* 1. Top-Right Stage Controls Pill (Slide Clicker / Arrow Key Legend) */
    const char *ctrl_txt = ui->has_focus
        ? "● PALCO [EDITANDO CÓDIGO]  •  Esc Ativar Setas/Passador"
        : "● MODO PALCO  •  ←/PgUp Voltar  •  →/PgDn/Espaço Passo  •  ↑↓ Rolar  •  [ ] Template  •  Esc Sair";
    Vector2 csz = MeasureTextEx(sm_font, ctrl_txt, 13.0f, 1.0f);
    float pw = fminf(canvas_bounds.width - 24.0f, csz.x + 24.0f);
    Rectangle top_pill = {
        floorf(canvas_bounds.x + canvas_bounds.width - pw - 12.0f),
        floorf(canvas_bounds.y + 12.0f),
        pw,
        28.0f
    };

    DrawRectangleRounded(top_pill, 0.35f, 6, Fade(pal->bg_panel, 0.94f));
    DrawRectangleRoundedLinesEx(top_pill, 0.35f, 6, 1.3f,
                                ui->has_focus ? pal->accent_warning : pal->accent_primary);
    BeginScissorMode((int)top_pill.x + 4, (int)top_pill.y, (int)top_pill.width - 8, (int)top_pill.height);
    DrawTextEx(
        sm_font,
        ctrl_txt,
        (Vector2){ floorf(top_pill.x + 12.0f), floorf(top_pill.y + (top_pill.height - csz.y) * 0.5f) },
        13.0f,
        1.0f,
        ui->has_focus ? pal->accent_warning : pal->accent_primary
    );
    EndScissorMode();

    /* 2. Sparse Concatenated Diagnostics & Terminal (stdout) Card on the Canvas Stage */
    bool has_error  = (session->diagnostic.error_type != ERR_NONE);
    bool has_stdout = (session->program_stdout[0] != '\0');
    bool is_active  = (session->state != STATE_IDLE) || has_error || has_stdout;
    if (!is_active) return;

    /* Extract up to last 3 non-empty lines of stdout (or compiler log on compile error) */
    const char *src_stream = (session->diagnostic.error_type == ERR_COMPILE && session->diagnostic.raw_compiler_log[0])
                             ? session->diagnostic.raw_compiler_log
                             : session->program_stdout;
    char tail_lines[3][160];
    int  tail_count = 0;

    if (src_stream && src_stream[0] != '\0') {
        const char *p = src_stream;
        while (*p) {
            const char *nl = strchr(p, '\n');
            size_t len = nl ? (size_t)(nl - p) : strlen(p);
            if (len > 0) {
                if (tail_count < 3) {
                    size_t cpy = (len < 155) ? len : 155;
                    memcpy(tail_lines[tail_count], p, cpy);
                    tail_lines[tail_count][cpy] = '\0';
                    tail_count++;
                } else {
                    memmove(tail_lines[0], tail_lines[1], sizeof(tail_lines[0]));
                    memmove(tail_lines[1], tail_lines[2], sizeof(tail_lines[0]));
                    size_t cpy = (len < 155) ? len : 155;
                    memcpy(tail_lines[2], p, cpy);
                    tail_lines[2][cpy] = '\0';
                }
            }
            if (!nl) break;
            p = nl + 1;
        }
    }

    float card_h = has_error ? 52.0f : 36.0f;
    if (tail_count > 0) {
        card_h += 8.0f + (float)tail_count * 18.0f;
    }

    float left_margin  = 210.0f; /* Leaves room for bottom-left Camera Pill */
    float right_margin = ui->config.show_blob ? 96.0f : 18.0f; /* Leaves room for Blob */
    float card_w       = fmaxf(260.0f, canvas_bounds.width - left_margin - right_margin);
    float bottom_y     = canvas_bounds.y + canvas_bounds.height - ui->terminal_anim_h - card_h - 12.0f;

    Rectangle stage_card = {
        floorf(canvas_bounds.x + left_margin),
        floorf(bottom_y),
        floorf(card_w),
        floorf(card_h)
    };

    Color border_c = has_error ? pal->accent_error :
                     (session->state == STATE_FINISHED ? pal->accent_primary : pal->accent_secondary);

    DrawRectangleRounded(stage_card, 0.14f, 6, Fade(pal->bg_panel, 0.90f));
    DrawRectangleRoundedLinesEx(stage_card, 0.14f, 6, 1.4f, border_c);

    BeginScissorMode((int)stage_card.x + 8, (int)stage_card.y + 4, (int)stage_card.width - 16, (int)stage_card.height - 8);

    float cy = stage_card.y + 8.0f;
    char hdr_buf[256];
    if (has_error) {
        snprintf(hdr_buf, sizeof(hdr_buf), "⚠ %s", session->diagnostic.pedagogical_title);
        DrawTextEx(ui_font, hdr_buf, (Vector2){ floorf(stage_card.x + 12.0f), floorf(cy) }, 15.0f, 1.0f, pal->accent_error);
        cy += 19.0f;
        DrawTextEx(sm_font, session->diagnostic.pedagogical_explanation,
                   (Vector2){ floorf(stage_card.x + 12.0f), floorf(cy) }, 13.0f, 1.0f, pal->text_primary);
        cy += 18.0f;
    } else if (session->state == STATE_FINISHED) {
        snprintf(hdr_buf, sizeof(hdr_buf), "✓ [CONCLUÍDO] %d nós na Heap  •  Pressione → ou Espaço para reiniciar", snap->node_count);
        DrawTextEx(ui_font, hdr_buf, (Vector2){ floorf(stage_card.x + 12.0f), floorf(cy) }, 15.0f, 1.0f, pal->accent_primary);
        cy += 22.0f;
    } else {
        snprintf(hdr_buf, sizeof(hdr_buf), "▶ Linha %d  •  %s()  •  %d nós na Heap  (Passo %d/%d)",
                 snap->current_executing_line,
                 snap->current_func[0] ? snap->current_func : "main",
                 snap->node_count,
                 (session->history_cursor >= 0) ? (session->history_cursor + 1) : 1,
                 (session->history_count > 0) ? session->history_count : 1);
        DrawTextEx(ui_font, hdr_buf, (Vector2){ floorf(stage_card.x + 12.0f), floorf(cy) }, 15.0f, 1.0f, pal->accent_warning);
        cy += 22.0f;
    }

    if (tail_count > 0) {
        DrawLineEx((Vector2){ stage_card.x + 10.0f, cy - 2.0f },
                   (Vector2){ stage_card.x + stage_card.width - 10.0f, cy - 2.0f },
                   1.0f, pal->border);
        cy += 2.0f;
        for (int l = 0; l < tail_count; l++) {
            char out_line[180];
            snprintf(out_line, sizeof(out_line), "%s %.155s",
                     (session->diagnostic.error_type == ERR_COMPILE) ? "gcc>" : "stdout>",
                     tail_lines[l]);
            DrawTextEx(
                sm_font,
                out_line,
                (Vector2){ floorf(stage_card.x + 12.0f), floorf(cy) },
                13.0f,
                1.0f,
                has_error ? pal->accent_error : pal->accent_primary
            );
            cy += 17.0f;
        }
    }

    EndScissorMode();
}

/* ============================================================================
 * 9. MAIN INTERACTIVE MEMORY CANVAS RENDERER (DYNAMIC THEME + FULL HEIGHT)
 * ============================================================================ */

void layout_render_canvas(
    DebuggerSession *session,
    WorkspaceUI     *ui,
    Rectangle        canvas_bounds
)
{
    if (!session || !ui) return;
    Font font    = (ui->font_canvas.texture.id != 0) ? ui->font_canvas : GuiGetFont();
    Font sm_font = (ui->font_sm.texture.id != 0)     ? ui->font_sm     : GuiGetFont();
    Font ui_font = (ui->font_ui.texture.id != 0)     ? ui->font_ui     : GuiGetFont();
    const TreeSnapshot *snap = &session->snapshot;
    const ThemePalette *pal  = &ui->theme;
    const AppConfig    *cfg  = &ui->config;
    float dt = GetFrameTime();

    bool input_blocked = (ui->active_menu >= 0) ||
                         ui->dropdown_edit_mode ||
                         ui->modal_settings_open ||
                         ui->modal_shortcuts_open ||
                         ui->modal_about_open ||
                         ui->modal_sysdiag_open ||
                         ui->modal_path_input_open ||
                         ui->modal_unsaved_confirm_open;

    /* 1. Theme Canvas Background */
    DrawRectangleRec(canvas_bounds, pal->bg_canvas);

    /* 2. Auto-Focus Camera Lerp when [ ⌖ Auto ] is enabled */
    if (ui->camera_auto_focus && session->state != STATE_IDLE) {
        Vector2 bmin, bmax;
        bool has_items = false;
        compute_scene_bounding_box(snap, &bmin, &bmax, &has_items);
        if (has_items) {
            Vector2 desired_target = { (bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f };
            Vector2 desired_offset = {
                canvas_bounds.x + canvas_bounds.width * 0.52f,
                canvas_bounds.y + canvas_bounds.height * 0.48f
            };
            float scene_w = fmaxf(260.0f, bmax.x - bmin.x + 140.0f);
            float scene_h = fmaxf(220.0f, bmax.y - bmin.y + 140.0f);
            float fit_w   = cfg->presentation_mode ? 0.88f : 0.80f;
            float fit_h   = cfg->presentation_mode ? 0.82f : 0.76f;
            float zx = (canvas_bounds.width  * fit_w) / scene_w;
            float zy = (canvas_bounds.height * fit_h) / scene_h;
            float desired_zoom = fminf(zx, zy);
            float min_z = cfg->presentation_mode ? 0.50f : 0.42f;
            float max_z = cfg->presentation_mode ? 1.38f : 1.12f;
            if (desired_zoom < min_z) desired_zoom = min_z;
            if (desired_zoom > max_z) desired_zoom = max_z;

            float cam_lerp = avora_exp_decay(8.5f, dt, cfg->reduce_animations);
            ui->camera.target = Vector2Lerp(ui->camera.target, desired_target, cam_lerp);
            ui->camera.offset = Vector2Lerp(ui->camera.offset, desired_offset, cam_lerp);
            ui->camera.zoom  += (desired_zoom - ui->camera.zoom) * cam_lerp;
        }
    }

    /* Update Error Shake & Error Flash Timers */
    if (ui->error_shake_timer > 0.0f) {
        ui->error_shake_timer = fmaxf(0.0f, ui->error_shake_timer - dt);
    }
    if (ui->error_flash_timer > 0.0f) {
        ui->error_flash_timer = fmaxf(0.0f, ui->error_flash_timer - dt);
    }

    Camera2D render_cam = ui->camera;
    if (ui->error_shake_timer > 0.0f && !cfg->reduce_animations) {
        float intensity = ui->error_shake_timer / 0.16f;
        render_cam.offset.x += sinf(ui->error_shake_timer * 155.0f) * 2.8f * intensity;
        render_cam.offset.y += cosf(ui->error_shake_timer * 210.0f) * 2.2f * intensity;
    }

    /* 3. Confine all 2D world & overlay rendering strictly inside BeginScissorMode */
    BeginScissorMode((int)canvas_bounds.x, (int)canvas_bounds.y, (int)canvas_bounds.width, (int)canvas_bounds.height);

    /* 4. Dynamic World-Space Grid (0 = Dot Grid, 1 = Line Grid, 2 = Plain/Smooth) */
    if (cfg->canvas_bg_style != 2) {
        float world_step = 36.0f;
        if (render_cam.zoom < 0.55f) world_step = 72.0f;
        Vector2 tl_world = GetScreenToWorld2D((Vector2){ canvas_bounds.x, canvas_bounds.y }, render_cam);
        Vector2 br_world = GetScreenToWorld2D((Vector2){ canvas_bounds.x + canvas_bounds.width, canvas_bounds.y + canvas_bounds.height }, render_cam);

        float start_wx = floorf(tl_world.x / world_step) * world_step;
        float start_wy = floorf(tl_world.y / world_step) * world_step;

        if (cfg->canvas_bg_style == 0) {
            for (float wx = start_wx; wx <= br_world.x; wx += world_step) {
                for (float wy = start_wy; wy <= br_world.y; wy += world_step) {
                    Vector2 sp = GetWorldToScreen2D((Vector2){ wx, wy }, render_cam);
                    DrawRectangle((int)sp.x, (int)sp.y, 2, 2, pal->canvas_grid);
                }
            }
        } else if (cfg->canvas_bg_style == 1) {
            Color line_col = Fade(pal->canvas_grid, 0.72f);
            for (float wx = start_wx; wx <= br_world.x; wx += world_step) {
                float sx = GetWorldToScreen2D((Vector2){ wx, 0.0f }, render_cam).x;
                DrawLine((int)sx, (int)canvas_bounds.y, (int)sx, (int)(canvas_bounds.y + canvas_bounds.height), line_col);
            }
            for (float wy = start_wy; wy <= br_world.y; wy += world_step) {
                float sy = GetWorldToScreen2D((Vector2){ 0.0f, wy }, render_cam).y;
                DrawLine((int)canvas_bounds.x, (int)sy, (int)(canvas_bounds.x + canvas_bounds.width), (int)sy, line_col);
            }
        }
    }

    BeginMode2D(render_cam);

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
        DrawRectangleRounded(stage_box, 0.08f, 6, Fade(pal->bg_panel, 0.85f));
        DrawRectangleRoundedLinesEx(stage_box, 0.08f, 6, 1.4f, pal->border);
        DrawTextEx(font, tr(STR_CANVAS_STAGING_BOX),
                   (Vector2){ stage_box.x + 12.0f, stage_box.y + 8.0f }, 13.0f, 1.0f, pal->text_secondary);
    }

    /* PASS 1: Draw Edges (Left/Prev = accent_secondary, Right/Next = accent_tertiary) */
    Color left_col  = pal->accent_secondary;
    Color right_col = pal->accent_tertiary;
    bool  is_fatal_garbage = (session->diagnostic.error_type == ERR_SIGSEGV_GARBAGE);

    for (int i = 0; i < snap->node_count; i++) {
        const VisualNode *n = &snap->nodes[i];

        /* Left / Prev pointer edge */
        if (n->left_state == EDGE_OK && n->left_addr != 0) {
            int l_idx = find_node_index_by_addr(snap, n->left_addr);
            if (l_idx >= 0) {
                draw_curved_tree_edge(n->current_pos, snap->nodes[l_idx].current_pos, left_col, true, n->is_list_node);
            }
        } else if (n->left_state == EDGE_CYCLE && n->left_addr != 0) {
            int l_idx = find_node_index_by_addr(snap, n->left_addr);
            if (l_idx >= 0) {
                draw_cycle_edge(n->current_pos, snap->nodes[l_idx].current_pos, true, ui->pulse_timer, font);
            }
        } else if (n->left_state == EDGE_GARBAGE_PTR || n->left_state == EDGE_DANGLING) {
            draw_garbage_or_dangling_hazard(
                n->current_pos, n->left_addr, true,
                (n->left_state == EDGE_DANGLING), is_fatal_garbage, ui->pulse_timer, font, pal
            );
        }

        /* Right / Next pointer edge */
        if (n->right_state == EDGE_OK && n->right_addr != 0) {
            int r_idx = find_node_index_by_addr(snap, n->right_addr);
            if (r_idx >= 0) {
                draw_curved_tree_edge(n->current_pos, snap->nodes[r_idx].current_pos, right_col, false, n->is_list_node);
            }
        } else if (n->right_state == EDGE_CYCLE && n->right_addr != 0) {
            int r_idx = find_node_index_by_addr(snap, n->right_addr);
            if (r_idx >= 0) {
                draw_cycle_edge(n->current_pos, snap->nodes[r_idx].current_pos, false, ui->pulse_timer, font);
            }
        } else if (n->right_state == EDGE_GARBAGE_PTR || n->right_state == EDGE_DANGLING) {
            draw_garbage_or_dangling_hazard(
                n->current_pos, n->right_addr, false,
                (n->right_state == EDGE_DANGLING), is_fatal_garbage, ui->pulse_timer, font, pal
            );
        }
    }

    /* PASS 2: Draw Tree & Linked-List Nodes */
    Color amber_focus = pal->accent_warning;
    float node_r      = cfg->presentation_mode ? 30.0f : NODE_RADIUS;
    float node_val_fs = cfg->presentation_mode ? 22.0f : 18.0f;

    for (int i = 0; i < snap->node_count; i++) {
        const VisualNode *n = &snap->nodes[i];
        Vector2 pos = n->current_pos;

        /* Soft outer glow halo on focused or mutated node */
        if (n->is_active_focus || n->highlight_timer > 0.01f) {
            float halo_r = node_r + 9.0f + 2.0f * sinf(ui->pulse_timer * 1.4f);
            DrawCircleV(pos, halo_r, Fade(amber_focus, 0.16f));
            DrawCircleV(pos, node_r + 4.5f, Fade(amber_focus, 0.24f));
        }

        Color body_col   = n->is_dangling ? (Color){ 88, 28, 135, 245 } : pal->bg_elevated;
        Color border_col = n->is_dangling ? (Color){ 232, 121, 249, 255 } :
                           ((n->is_active_focus || n->highlight_timer > 0.05f) ? amber_focus : pal->accent_secondary);

        DrawCircleV(pos, node_r, body_col);
        DrawRing(pos, node_r - (cfg->presentation_mode ? 2.6f : 2.2f), node_r, 0.0f, 360.0f, 36, border_col);

        /* ~150ms Crisp Border Flash on newly allocated or mutated node */
        if (n->border_flash_timer > 0.0f && !cfg->reduce_animations) {
            float ft = fminf(1.0f, n->border_flash_timer / 0.16f);
            DrawRing(pos, node_r - 1.0f, node_r + 3.0f, 0.0f, 360.0f, 36,
                     Fade(pal->is_light ? pal->accent_primary : WHITE, ft * 0.90f));
        }

        /* Node integer value */
        char val_str[32];
        snprintf(val_str, sizeof(val_str), "%d", n->val);
        Vector2 vsz = MeasureTextEx(font, val_str, node_val_fs, 1.0f);
        DrawTextEx(font, val_str, (Vector2){ pos.x - vsz.x * 0.5f, pos.y - vsz.y * 0.5f },
                   node_val_fs, 1.0f, pal->text_primary);

        /* Memory address badge below node (0x...) — toggled via show_memory_addresses */
        if (cfg->show_memory_addresses) {
            char addr_str[32];
            snprintf(addr_str, sizeof(addr_str), "0x%" PRIx64, (uint64_t)(n->addr & 0xFFFFFFU));
            Vector2 asz = MeasureTextEx(font, addr_str, 11.0f, 1.0f);
            Rectangle addr_pill = { pos.x - asz.x * 0.5f - 4.0f, pos.y + node_r + 4.0f, asz.x + 8.0f, 15.0f };
            DrawRectangleRounded(addr_pill, 0.3f, 4, Fade(pal->bg_panel, 0.92f));
            DrawTextEx(font, addr_str, (Vector2){ addr_pill.x + 4.0f, addr_pill.y + 2.0f },
                       11.0f, 1.0f, pal->text_secondary);
        }

        /* Floating Variable Name Pill Badges above the node */
        int badge_stack = 0;
        float badge_fs = cfg->presentation_mode ? 15.0f : 13.0f;
        for (int v = 0; v < snap->local_var_count; v++) {
            if (snap->local_vars[v].addr == n->addr) {
                const char *vname = snap->local_vars[v].name;
                bool vchanged     = snap->local_vars[v].changed;
                Vector2 bsz = MeasureTextEx(font, vname, badge_fs, 1.0f);
                float bw = bsz.x + 14.0f;
                float bh = cfg->presentation_mode ? 21.0f : 19.0f;
                float bx = pos.x - bw * 0.5f;
                float by = pos.y - node_r - 26.0f - (float)badge_stack * 23.0f;

                Rectangle pill = { bx, by, bw, bh };
                Color p_border = vchanged ? amber_focus : pal->accent_secondary;
                DrawRectangleRounded(pill, 0.45f, 6, Fade(pal->bg_panel, 0.95f));
                DrawRectangleRoundedLinesEx(pill, 0.45f, 6, 1.3f, p_border);
                DrawTextEx(font, vname, (Vector2){ bx + 7.0f, by + 3.0f },
                           badge_fs, 1.0f, vchanged ? amber_focus : pal->text_primary);
                badge_stack++;
            }
        }
    }

    /* PASS 3: Draw 1D Arrays / Vectors & Index Pointers (Search & Sorting) */
    draw_visual_arrays(snap, font, pal);

    EndMode2D();

    /* Capture clean 2D Canvas PNG screenshot if requested (before HUD/menus/modals overlay) */
    if (ui->pending_png_capture && ui->pending_png_path[0] != '\0') {
        EndScissorMode();
        Image screen_img = LoadImageFromScreen();
        if (screen_img.data != NULL) {
            Rectangle crop = canvas_bounds;
            if (crop.x < 0) crop.x = 0;
            if (crop.y < 0) crop.y = 0;
            if (crop.x + crop.width > (float)screen_img.width) crop.width = (float)screen_img.width - crop.x;
            if (crop.y + crop.height > (float)screen_img.height) crop.height = (float)screen_img.height - crop.y;
            ImageCrop(&screen_img, crop);
            bool ok = ExportImage(screen_img, ui->pending_png_path);
            UnloadImage(screen_img);
            snprintf(ui->toast_message, sizeof(ui->toast_message),
                     ok ? "Imagem PNG do Canvas exportada: %s" : "Falha ao exportar PNG em: %s",
                     ui->pending_png_path);
            ui->toast_timer = 3.8f;
        }
        ui->pending_png_capture = false;
        BeginScissorMode((int)canvas_bounds.x, (int)canvas_bounds.y, (int)canvas_bounds.width, (int)canvas_bounds.height);
    }

    /* Empty Canvas State Overlay (Clean & Minimal) */
    if (snap->node_count == 0 && snap->array_count == 0) {
        const char *empty_msg = (session->state == STATE_IDLE)
            ? tr(STR_CANVAS_EMPTY_IDLE)
            : tr(STR_CANVAS_EMPTY_RUN);
        Vector2 msz = MeasureTextEx(ui_font, empty_msg, 15.0f, 1.0f);
        float mx = floorf(canvas_bounds.x + fmaxf(14.0f, (canvas_bounds.width - msz.x) * 0.5f) + 0.5f);
        float my = floorf(canvas_bounds.y + canvas_bounds.height * 0.50f + 0.5f);
        DrawTextEx(
            ui_font,
            empty_msg,
            (Vector2){ mx, my },
            15.0f,
            1.0f,
            pal->text_secondary
        );
    }

    /* 5. Floating Observability HUD (Top-Left: Call Stack & Local Variables) */
    bool over_hud = draw_Observability_hud(session, ui, canvas_bounds, sm_font, input_blocked);

    /* 5b. Presentation / Stage Mode Concatenated Overlay (Top-Right Controls + Bottom Stage Console) */
    draw_presentation_stage_overlay(session, ui, canvas_bounds, ui_font, sm_font);

    /* 6. Floating Camera Pill (Bottom-Left: [ Auto | - 100% + ]) */
    bool over_pill = draw_floating_camera_pill(snap, ui, canvas_bounds, sm_font, input_blocked);

    /* 7. Interactive Decision-Tree Blob Mascot (Bottom-Right) */
    draw_avora_blob_decision_tree(session, ui, canvas_bounds, sm_font, input_blocked);

    /* 8. Canvas Mouse Pan & Wheel Zoom (auto-disables Auto-Focus when user interacts manually) */
    Vector2 mouse = GetMousePosition();
    bool over_terminal = (ui->terminal_anim_h > 1.5f) &&
                         (mouse.y >= canvas_bounds.y + canvas_bounds.height - ui->terminal_anim_h);
    bool over_blob = ui->config.show_blob &&
                     (CheckCollisionPointRec(mouse, ui->blob_mascot_bounds) ||
                      (ui->blob_anim_progress > 0.05f && CheckCollisionPointRec(mouse, ui->blob_card_bounds)));
    bool inside_canvas = !input_blocked &&
                         CheckCollisionPointRec(mouse, canvas_bounds) &&
                         !over_terminal && !over_hud && !over_pill && !over_blob &&
                         !ui->dragging_v_split;

    if (inside_canvas) {
        float wheel = GetMouseWheelMove();
        if (fabsf(wheel) > 0.001f) {
            ui->camera_auto_focus = false;
            Vector2 mouseWorldPos = GetScreenToWorld2D(mouse, ui->camera);
            ui->camera.offset = mouse;
            ui->camera.target = mouseWorldPos;
            ui->camera.zoom += wheel * 0.12f;
            if (ui->camera.zoom < 0.30f) ui->camera.zoom = 0.30f;
            if (ui->camera.zoom > 2.60f) ui->camera.zoom = 2.60f;
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) || IsMouseButtonPressed(MOUSE_MIDDLE_BUTTON)) {
            ui->is_panning        = true;
            ui->camera_auto_focus = false;
            ui->pan_start_mouse   = mouse;
            ui->pan_start_target  = ui->camera.target;
        }
    }

    if (ui->is_panning) {
        if (!input_blocked && (IsMouseButtonDown(MOUSE_LEFT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON))) {
            Vector2 delta = Vector2Subtract(ui->pan_start_mouse, mouse);
            ui->camera.target = Vector2Add(ui->pan_start_target, Vector2Scale(delta, 1.0f / ui->camera.zoom));
        } else {
            ui->is_panning = false;
        }
    }

    /* Subtle ~150ms Crimson Error Flash on Compile / SIGSEGV / ASan Trap */
    if (ui->error_flash_timer > 0.0f && !cfg->reduce_animations) {
        float ef = fminf(1.0f, ui->error_flash_timer / 0.16f);
        DrawRectangleRec(canvas_bounds, Fade(pal->accent_error, ef * 0.09f));
        DrawRectangleLinesEx(canvas_bounds, 2.5f, Fade(pal->accent_error, ef * 0.85f));
    }

    EndScissorMode();
}
