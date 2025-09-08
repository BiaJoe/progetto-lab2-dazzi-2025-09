// json_visualizer.c
#include <stdio.h>
#include <stdbool.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "server.h"

// Stato globale
static FILE *global_simulation_json_file   = NULL;
static bool  global_is_first_tick          = true;

static FILE *global_positions_json_file    = NULL;
static bool  global_positions_first_emergency = true;

typedef struct {
    char  *grid;          // width * height
    int    width, height; // canvas scaled size
    int    map_w, map_h;  // dimensioni vere
    double scale;         // width / map_w
} ascii_canvas_t;

static ascii_canvas_t *global_ascii_canvas = NULL;

extern server_context_t *ctx;


static const char* rescuer_status_to_string(rescuer_status_t s){
    switch (s){
        case IDLE:               return "IDLE";
        case EN_ROUTE_TO_SCENE:  return "EN_ROUTE_TO_SCENE";
        case ON_SCENE:           return "ON_SCENE";
        case RETURNING_TO_BASE:  return "RETURNING_TO_BASE";
        default:                 return "UNKNOWN";
    }
}

static const char* emergency_status_to_string(emergency_status_t s){
    switch (s){
        case WAITING:              return "WAITING";
        case ASSIGNED:             return "ASSIGNED";
        case ASKING_FOR_RESCUERS:  return "ASKING_FOR_RESCUERS";
        case WAITING_FOR_RESCUERS: return "WAITING_FOR_RESCUERS";
        case IN_PROGRESS:          return "IN_PROGRESS";
        case PAUSED:               return "PAUSED";
        case COMPLETED:            return "COMPLETED";
        case CANCELED:             return "CANCELED";
        case TIMEOUT:              return "TIMEOUT";
        default:                   return "UNKNOWN";
    }
}

static void write_tick_rescuers(FILE *out){
    fprintf(out, "      \"rescuers\": [\n");
    bool first = true;

    for (int i = 0; i < ctx->rescuers->count; i++){
        rescuer_type_t *rtype = ctx->rescuers->types[i];
        if (!rtype) continue;

        for (int j = 0; j < rtype->amount; j++){
            rescuer_digital_twin_t *twin = rtype->twins[j];
            if (!twin || twin->status == IDLE) continue;

            if (!first) fprintf(out, ",\n");
            first = false;

            fprintf(out,
                "        { \"id\": %d, \"type\": \"%s\", \"status\": \"%s\", \"x\": %d, \"y\": %d }",
                twin->id,
                rtype->rescuer_type_name ? rtype->rescuer_type_name : "?",
                rescuer_status_to_string(twin->status),
                twin->x, twin->y
            );
        }
    }
    fprintf(out, "\n      ]");
}

static void write_tick_emergencies(FILE *out){
    fprintf(out, "      \"emergencies\": [\n");
    bool first = true;

    for (int i = 0; i < WORKER_THREADS_NUMBER; i++){
        emergency_t *em = ctx->active_emergencies->array[i];
        if (!em) continue;

        if (!first) fprintf(out, ",\n");
        first = false;

        fprintf(out,
            "        { \"id\": %d, \"type\": \"%s\", \"status\": \"%s\", \"x\": %d, \"y\": %d }",
            em->id,
            (em->type && em->type->emergency_desc) ? em->type->emergency_desc : "?",
            emergency_status_to_string(em->status),
            em->x, em->y
        );
    }
    fprintf(out, "\n      ]");
}

bool json_visualizer_begin(const char *path, int width, int height){
    global_simulation_json_file = fopen(path, "w");
    if (!global_simulation_json_file) return false;
    global_is_first_tick = true;

    fprintf(global_simulation_json_file, "{\n");
    fprintf(global_simulation_json_file, "  \"width\": %d,\n",  width);
    fprintf(global_simulation_json_file, "  \"height\": %d,\n", height);
    fprintf(global_simulation_json_file, "  \"ticks\": [\n");
    fflush(global_simulation_json_file);
    return true;
}

bool json_visualizer_append_tick(int tick){
    if (!global_simulation_json_file) return false;

    if (!global_is_first_tick) fprintf(global_simulation_json_file, ",\n");
    global_is_first_tick = false;

    fprintf(global_simulation_json_file, "    {\n");
    fprintf(global_simulation_json_file, "      \"tick\": %d,\n", tick);
    write_tick_rescuers(global_simulation_json_file); fprintf(global_simulation_json_file, ",\n");
    write_tick_emergencies(global_simulation_json_file);
    fprintf(global_simulation_json_file, "\n    }");
    fflush(global_simulation_json_file);
    return true;
}

bool json_visualizer_end(void){
    if (!global_simulation_json_file) return false;
    fprintf(global_simulation_json_file, "\n  ]\n");
    fprintf(global_simulation_json_file, "}\n");
    bool ok = (fclose(global_simulation_json_file) == 0);
    global_simulation_json_file = NULL;
    return ok;
}

static void write_positions_rescuer_types(FILE *out){
    fprintf(out, "  \"rescuer_types\": [\n");
    bool first = true;

    for (int i = 0; i < ctx->rescuers->count; i++){
        rescuer_type_t *rtype = ctx->rescuers->types[i];
        if (!rtype) continue;

        if (!first) fprintf(out, ",\n");
        first = false;

        fprintf(out,
            "    { \"name\": \"%s\", \"amount\": %d, \"x\": %d, \"y\": %d }",
            rtype->rescuer_type_name ? rtype->rescuer_type_name : "?",
            rtype->amount, rtype->x, rtype->y
        );
    }
    fprintf(out, "\n  ],\n");
}

static void write_positions_one_emergency(void *elem){
    if (!global_positions_json_file) return;

    emergency_t *em = (emergency_t*)elem;
    const char *type_desc = (em->type && em->type->emergency_desc) ? em->type->emergency_desc : "?";
    int         type_prio = em->type ? (int)em->type->priority : -1;

    if (!global_positions_first_emergency) fprintf(global_positions_json_file, ",\n");
    global_positions_first_emergency = false;

    fprintf(global_positions_json_file,
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
        type_desc, type_prio, em->id, (int)em->priority,
        emergency_status_to_string(em->status),
        em->x, em->y, (intmax_t)em->time, em->tick_time,
        em->rescuer_count, em->how_many_times_was_paused, em->how_many_times_was_promoted
    );
}

// ======== ASCII canvas ========
static inline int clampi(int v, int lo, int hi){
    return (v < lo) ? lo : (v > hi ? hi : v);
}

static inline int canvas_scale_x(const ascii_canvas_t *ac, int X){
    int sx = (int)floor((double)X * ac->scale + 0.5);
    return clampi(sx, 0, ac->width - 1);
}

static inline int canvas_scale_y(const ascii_canvas_t *ac, int Y){
    int sy = (int)floor((double)Y * ac->scale + 0.5);
    return clampi(sy, 0, ac->height - 1);
}

static inline void canvas_clear(ascii_canvas_t *ac, char ch){
    for (int i = 0; i < ac->width * ac->height; ++i) ac->grid[i] = ch;
}

static inline void canvas_plot(ascii_canvas_t *ac, int sx, int sy, char ch){
    int idx = sy * ac->width + sx;
    char prev = ac->grid[idx];

    if (prev == ' ' || prev == ch) { ac->grid[idx] = ch; return; }

    if ((prev == POS_RESCUER_CHAR && ch == POS_EMERGENCY_CHAR) ||
        (prev == POS_EMERGENCY_CHAR && ch == POS_RESCUER_CHAR)) {
        ac->grid[idx] = 'X'; // collisione
    } else {
        ac->grid[idx] = ch;
    }
}

static void canvas_plot_rescuer_bases(ascii_canvas_t *ac){
    if (!ctx || !ctx->rescuers) return;
    for (int i = 0; i < ctx->rescuers->count; ++i){
        rescuer_type_t *rtype = ctx->rescuers->types[i];
        if (!rtype) continue;
        int sx = canvas_scale_x(ac, rtype->x);
        int sy = canvas_scale_y(ac, rtype->y);
        canvas_plot(ac, sx, sy, POS_RESCUER_CHAR);
    }
}

static void canvas_plot_emergency_cb(void *elem){
    if (!global_ascii_canvas) return;
    emergency_t *em = (emergency_t*)elem;
    int sx = canvas_scale_x(global_ascii_canvas, em->x);
    int sy = canvas_scale_y(global_ascii_canvas, em->y);
    canvas_plot(global_ascii_canvas, sx, sy, POS_EMERGENCY_CHAR);
}

static void canvas_plot_active_emergencies(ascii_canvas_t *ac){
    if (!ctx || !ctx->active_emergencies) return;
    for (int i = 0; i < WORKER_THREADS_NUMBER; ++i){
        emergency_t *em = ctx->active_emergencies->array[i];
        if (!em) continue;
        int sx = canvas_scale_x(ac, em->x);
        int sy = canvas_scale_y(ac, em->y);
        canvas_plot(ac, sx, sy, POS_EMERGENCY_CHAR);
    }
}

static void ascii_write_with_borders(FILE *out, const ascii_canvas_t *ac){
    for (int x = 0; x < ac->width + 2; ++x) fputc(POS_MAP_BORDER_CHAR, out);
    fputc('\n', out);

    for (int y = 0; y < ac->height; ++y) {
        fputc(POS_MAP_BORDER_CHAR, out);
        for (int x = 0; x < ac->width; ++x)
            fputc(ac->grid[y * ac->width + x], out);
        fputc(POS_MAP_BORDER_CHAR, out);
        fputc('\n', out);
    }

    for (int x = 0; x < ac->width + 2; ++x) fputc(POS_MAP_BORDER_CHAR, out);
    fputc('\n', out);
}





bool json_visualizer_positions_dump(const char *json_path, const char *txt_path){
    bool ok_json = true, ok_txt = true;

    // JSON
    if (json_path && *json_path) {
        global_positions_json_file = fopen(json_path, "w");
        if (!global_positions_json_file) return false;

        global_positions_first_emergency = true;

        fprintf(global_positions_json_file, "{\n");

        int map_w = (ctx && ctx->enviroment) ? ctx->enviroment->width  : 0;
        int map_h = (ctx && ctx->enviroment) ? ctx->enviroment->height : 0;
        fprintf(global_positions_json_file, "  \"map\": { \"width\": %d, \"height\": %d },\n", map_w, map_h);

        write_positions_rescuer_types(global_positions_json_file);
        fprintf(global_positions_json_file, "  \"emergencies\": [\n");

        if (ctx->completed_emergencies) pq_map(ctx->completed_emergencies, write_positions_one_emergency);
        if (ctx->canceled_emergencies)  pq_map(ctx->canceled_emergencies,  write_positions_one_emergency);

        fprintf(global_positions_json_file, "\n  ]\n");
        fprintf(global_positions_json_file, "}\n");

        ok_json = (fclose(global_positions_json_file) == 0);
        global_positions_json_file = NULL;
    }

    // TXT (ASCII)
    if (txt_path && *txt_path) {
        FILE *txt = fopen(txt_path, "w");
        if (!txt) return ok_json;

        int map_w = (ctx && ctx->enviroment) ? ctx->enviroment->width  : 0;
        int map_h = (ctx && ctx->enviroment) ? ctx->enviroment->height : 0;

        if (map_w <= 0 || map_h <= 0){
            fprintf(txt, "(no map size available)\n");
            ok_txt = (fclose(txt) == 0) && ok_txt;
            return ok_json && ok_txt;
        }

        ascii_canvas_t canvas;
        canvas.map_w = map_w;
        canvas.map_h = map_h;

        canvas.width  = (map_w > POS_MAX_WIDTH) ? POS_MAX_WIDTH : map_w;
        if (canvas.width <= 0) canvas.width = 1;

        canvas.scale  = (double)canvas.width / (double)map_w;
        canvas.height = (int)floor((double)map_h * canvas.scale + 0.5);
        if (canvas.height <= 0) canvas.height = 1;

        size_t cells = (size_t)canvas.width * (size_t)canvas.height;
        canvas.grid = (char*)malloc(cells);
        if (!canvas.grid){
            fprintf(txt, "(not enough memory for ASCII canvas %dx%d)\n", canvas.width, canvas.height);
            ok_txt = (fclose(txt) == 0) && ok_txt;
            return ok_json && ok_txt;
        }

        canvas_clear(&canvas, ' ');
        canvas_plot_rescuer_bases(&canvas);
        canvas_plot_active_emergencies(&canvas);

        global_ascii_canvas = &canvas;
        if (ctx->completed_emergencies) pq_map(ctx->completed_emergencies, canvas_plot_emergency_cb);
        if (ctx->canceled_emergencies)  pq_map(ctx->canceled_emergencies,  canvas_plot_emergency_cb);
        global_ascii_canvas = NULL;

        ascii_write_with_borders(txt, &canvas);

        free(canvas.grid);
        ok_txt = (fclose(txt) == 0) && ok_txt;
    }

    return ok_json && ok_txt;
}