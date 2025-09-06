// json_visualizer.c
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "server.h"

// ---------- stato globale per il file SIMULAZIONE ----------
static FILE *g_f = NULL;
static bool g_first_tick = true;

// ---------- stato globale per POSITIONS (JSON) ----------
static FILE *g_pos_f = NULL;
static bool g_pos_first_em = true;

extern server_context_t *ctx;

// ---------- stringify enum ----------
static const char* rescuer_status_to_string(rescuer_status_t s){
    switch (s){
        case IDLE: return "IDLE";
        case EN_ROUTE_TO_SCENE: return "EN_ROUTE_TO_SCENE";
        case ON_SCENE: return "ON_SCENE";
        case RETURNING_TO_BASE: return "RETURNING_TO_BASE";
        default: return "UNKNOWN";
    }
}

static const char* emergency_status_to_string(emergency_status_t s){
    switch (s){
        case WAITING: return "WAITING";
        case ASSIGNED: return "ASSIGNED";
        case ASKING_FOR_RESCUERS: return "ASKING_FOR_RESCUERS";
        case WAITING_FOR_RESCUERS: return "WAITING_FOR_RESCUERS";
        case IN_PROGRESS: return "IN_PROGRESS";
        case PAUSED: return "PAUSED";
        case COMPLETED: return "COMPLETED";
        case CANCELED: return "CANCELED";
        case TIMEOUT: return "TIMEOUT";
        default: return "UNKNOWN";
    }
}

// ======================
//   DUMP PER-TICK (JSON)
// ======================
static void dump_rescuers(FILE *f){
    fprintf(f, "      \"rescuers\": [\n");
    bool first = true;
    for (int i = 0; i < ctx->rescuers->count; i++){
        rescuer_type_t *rt = ctx->rescuers->types[i];
        if (!rt) continue;
        for (int j = 0; j < rt->amount; j++){
            rescuer_digital_twin_t *t = rt->twins[j];
            if (!t) continue;
            if (t->status == IDLE) continue; // NON rappresentiamo gli IDLE

            if (!first) fprintf(f, ",\n");
            first = false;

            fprintf(f,
                "        { \"id\": %d, \"type\": \"%s\", \"status\": \"%s\", \"x\": %d, \"y\": %d }",
                t->id,
                rt->rescuer_type_name ? rt->rescuer_type_name : "?",
                rescuer_status_to_string(t->status),
                t->x, t->y
            );
        }
    }
    fprintf(f, "\n      ]");
}

static void dump_emergencies_tick(FILE *f){
    fprintf(f, "      \"emergencies\": [\n");
    bool first = true;
    for (int i = 0; i < WORKER_THREADS_NUMBER; i++){
        emergency_t *e = ctx->active_emergencies->array[i];
        if (!e) continue;

        if (!first) fprintf(f, ",\n");
        first = false;

        fprintf(f,
            "        { \"id\": %d, \"type\": \"%s\", \"status\": \"%s\", \"x\": %d, \"y\": %d }",
            e->id,
            (e->type && e->type->emergency_desc) ? e->type->emergency_desc : "?",
            emergency_status_to_string(e->status),
            e->x, e->y
        );
    }
    fprintf(f, "\n      ]");
}

// ==========================
//   API SIMULAZIONE (JSON)
// ==========================
bool json_visualizer_begin(const char *path, int width, int height){
    g_f = fopen(path, "w");
    if (!g_f) return false;
    g_first_tick = true;

    fprintf(g_f, "{\n");
    fprintf(g_f, "  \"width\": %d,\n", width);
    fprintf(g_f, "  \"height\": %d,\n", height);
    fprintf(g_f, "  \"ticks\": [\n");
    fflush(g_f);
    return true;
}

bool json_visualizer_append_tick(int tick){
    if (!g_f) return false;

    if (!g_first_tick) fprintf(g_f, ",\n");
    g_first_tick = false;

    fprintf(g_f, "    {\n");
    fprintf(g_f, "      \"tick\": %d,\n", tick);
    dump_rescuers(g_f); fprintf(g_f, ",\n");
    dump_emergencies_tick(g_f);
    fprintf(g_f, "\n    }");
    fflush(g_f);
    return true;
}

bool json_visualizer_end(void){
    if (!g_f) return false;
    fprintf(g_f, "\n  ]\n");
    fprintf(g_f, "}\n");
    bool ok = (fclose(g_f) == 0);
    g_f = NULL;
    return ok;
}

// ============================================
//   POSITIONS.JSON (rescuer_types + emergenze)
// ============================================

static void positions_dump_rescuer_types(FILE *f){
    fprintf(f, "  \"rescuer_types\": [\n");
    bool first = true;
    for (int i = 0; i < ctx->rescuers->count; i++){
        rescuer_type_t *rt = ctx->rescuers->types[i];
        if (!rt) continue;

        if (!first) fprintf(f, ",\n");
        first = false;

        fprintf(f,
            "    { \"name\": \"%s\", \"amount\": %d, \"x\": %d, \"y\": %d }",
            rt->rescuer_type_name ? rt->rescuer_type_name : "?",
            rt->amount,
            rt->x, rt->y
        );
    }
    fprintf(f, "\n  ],\n");
}

static void positions_dump_one_emergency(void *elem){
    if (!g_pos_f) return;
    emergency_t *e = (emergency_t*)elem;

    const char *type_desc = (e->type && e->type->emergency_desc) ? e->type->emergency_desc : "?";
    int         type_prio = (e->type) ? (int)e->type->priority : -1;

    if (!g_pos_first_em) fprintf(g_pos_f, ",\n");
    g_pos_first_em = false;

    fprintf(g_pos_f,
        "    { "
          "\"type_desc\": \"%s\", "
          "\"type_priority\": %d, "
          "\"id\": %d, "
          "\"priority\": %d, "
          "\"status\": \"%s\", "
          "\"x\": %d, "
          "\"y\": %d, "
          "\"time\": %" PRIdMAX ", "
          "\"tick_time\": %d, "
          "\"rescuer_count\": %d, "
          "\"how_many_times_was_paused\": %d, "
          "\"how_many_times_was_promoted\": %d "
        "}",
        type_desc,
        type_prio,
        e->id,
        (int)e->priority,
        emergency_status_to_string(e->status),
        e->x, e->y,
        (intmax_t)e->time,
        e->tick_time,
        e->rescuer_count,
        e->how_many_times_was_paused,
        e->how_many_times_was_promoted
    );
}

// ============================
//   ASCII MAP (POSITIONS_TXT)
// ============================

typedef struct {
    char  *grid;     // W * H
    int    W, H;
    int    map_w, map_h;   // dimensioni reali mappa
    double scale;          // target_w / map_w
} ascii_ctx_t;

static inline int clampi(int v, int lo, int hi){
    return (v < lo) ? lo : (v > hi ? hi : v);
}

static inline int scale_x(const ascii_ctx_t *ac, int X){
    int sx = (int)floor((double)X * ac->scale + 0.5);
    return clampi(sx, 0, ac->W - 1);
}

static inline int scale_y(const ascii_ctx_t *ac, int Y){
    int sy = (int)floor((double)Y * ac->scale + 0.5);
    return clampi(sy, 0, ac->H - 1);
}

static inline void canvas_clear(ascii_ctx_t *ac, char ch){
    for (int i = 0; i < ac->W * ac->H; ++i) ac->grid[i] = ch;
}

static inline void canvas_plot(ascii_ctx_t *ac, int sx, int sy, char ch){
    int idx = sy * ac->W + sx;
    char old = ac->grid[idx];
    if (old == ' ') ac->grid[idx] = ch;
    else if (old == ch) ac->grid[idx] = ch;       // idempotente
    else if ((old == POS_RESCUER_CHAR && ch == POS_EMERGENCY_CHAR) ||
             (old == POS_EMERGENCY_CHAR && ch == POS_RESCUER_CHAR))
        ac->grid[idx] = 'X';                      // collisione R+E
    else
        ac->grid[idx] = ch;                       // sovrascrivi genericamente
}

static void ascii_plot_rescuer_bases(ascii_ctx_t *ac){
    if (!ctx || !ctx->rescuers) return;
    for (int i = 0; i < ctx->rescuers->count; ++i){
        rescuer_type_t *rt = ctx->rescuers->types[i];
        if (!rt) continue;
        int sx = scale_x(ac, rt->x);
        int sy = scale_y(ac, rt->y);
        canvas_plot(ac, sx, sy, POS_RESCUER_CHAR);
    }
}

// callback per pq_map: disegna emergenze (E)
static ascii_ctx_t *g_ascii_plotter = NULL;

static void ascii_plot_emergency_cb(void *elem){
    if (!g_ascii_plotter) return;
    emergency_t *e = (emergency_t*)elem;
    int sx = scale_x(g_ascii_plotter, e->x);
    int sy = scale_y(g_ascii_plotter, e->y);
    canvas_plot(g_ascii_plotter, sx, sy, POS_EMERGENCY_CHAR);
}

// attive (slot worker)
static void ascii_plot_active_emergencies(ascii_ctx_t *ac){
    if (!ctx || !ctx->active_emergencies) return;
    for (int i = 0; i < WORKER_THREADS_NUMBER; ++i){
        emergency_t *e = ctx->active_emergencies->array[i];
        if (!e) continue;
        int sx = scale_x(ac, e->x);
        int sy = scale_y(ac, e->y);
        canvas_plot(ac, sx, sy, POS_EMERGENCY_CHAR);
    }
}

// ============================================
//   POSITIONS.json + POSITIONS.txt (ASCII)
// ============================================

static void ascii_write_with_borders(FILE *out, const ascii_ctx_t *ac) {
    // Bordi superiori
    for (int x = 0; x < ac->W + 2; ++x) fputc(POS_MAP_BORDER_CHAR, out);
    fputc('\n', out);

    for (int y = 0; y < ac->H; ++y) {
        fputc(POS_MAP_BORDER_CHAR, out);  // bordo sinistro
        for (int x = 0; x < ac->W; ++x) {
            fputc(ac->grid[y * ac->W + x], out);
        }
        fputc(POS_MAP_BORDER_CHAR, out);  // bordo destro
        fputc('\n', out);
    }

    // Bordi inferiori
    for (int x = 0; x < ac->W + 2; ++x) fputc(POS_MAP_BORDER_CHAR, out);
    fputc('\n', out);
}

bool json_visualizer_positions_dump(const char *json_path, const char *txt_path){
    bool ok_json = true, ok_txt = true;

    // --- JSON: positions.json ---
    if (json_path && *json_path) {
        g_pos_f = fopen(json_path, "w");
        if (!g_pos_f) return false;

        g_pos_first_em = true;

        fprintf(g_pos_f, "{\n");

        int w = (ctx && ctx->enviroment) ? ctx->enviroment->width  : 0;
        int h = (ctx && ctx->enviroment) ? ctx->enviroment->height : 0;
        fprintf(g_pos_f, "  \"map\": { \"width\": %d, \"height\": %d },\n", w, h);

        positions_dump_rescuer_types(g_pos_f);
        fprintf(g_pos_f, "  \"emergencies\": [\n");

        if (ctx->completed_emergencies)
            pq_map(ctx->completed_emergencies, positions_dump_one_emergency);

        if (ctx->canceled_emergencies)
            pq_map(ctx->canceled_emergencies, positions_dump_one_emergency);

        fprintf(g_pos_f, "\n  ]\n");
        fprintf(g_pos_f, "}\n");

        ok_json = (fclose(g_pos_f) == 0);
        g_pos_f = NULL;
    }

    // --- TXT: positions.txt (ASCII map) ---
    if (txt_path && *txt_path) {
        FILE *txt = fopen(txt_path, "w");
        if (!txt) return ok_json;

        int w = (ctx && ctx->enviroment) ? ctx->enviroment->width  : 0;
        int h = (ctx && ctx->enviroment) ? ctx->enviroment->height : 0;

        if (w <= 0 || h <= 0){
            fprintf(txt, "(no map size available)\n");
            ok_txt = (fclose(txt) == 0) && ok_txt;
            return ok_json && ok_txt;
        }

        ascii_ctx_t ac;
        ac.map_w = w;
        ac.map_h = h;

        ac.W = (w > POS_MAX_WIDTH) ? POS_MAX_WIDTH : w;
        if (ac.W <= 0) ac.W = 1;

        ac.scale = (double)ac.W / (double)w;
        ac.H = (int)floor((double)h * ac.scale + 0.5);
        if (ac.H <= 0) ac.H = 1;

        size_t cells = (size_t)ac.W * (size_t)ac.H;
        ac.grid = (char*)malloc(cells);
        if (!ac.grid){
            fprintf(txt, "(not enough memory for ASCII canvas %dx%d)\n", ac.W, ac.H);
            ok_txt = (fclose(txt) == 0) && ok_txt;
            return ok_json && ok_txt;
        }

        canvas_clear(&ac, ' ');

        // Plot rescuer bases
        ascii_plot_rescuer_bases(&ac);

        // Plot emergencies (attive + completed + canceled)
        ascii_plot_active_emergencies(&ac);
        g_ascii_plotter = &ac;
        if (ctx->completed_emergencies) pq_map(ctx->completed_emergencies, ascii_plot_emergency_cb);
        if (ctx->canceled_emergencies)  pq_map(ctx->canceled_emergencies,  ascii_plot_emergency_cb);
        g_ascii_plotter = NULL;

        // Stampa su file con bordi
        ascii_write_with_borders(txt, &ac);

        free(ac.grid);
        ok_txt = (fclose(txt) == 0) && ok_txt;
    }

    return ok_json && ok_txt;
}