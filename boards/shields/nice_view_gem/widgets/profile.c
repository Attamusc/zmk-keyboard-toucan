#include <zephyr/kernel.h>
#include "profile.h"

// Profile dots position (right side of bottom zone)
#define PROFILE_X 90
#define PROFILE_Y 143
#define PROFILE_DOT_SIZE 8
#define PROFILE_DOT_GAP 10

static void draw_inactive_profiles(lv_obj_t *canvas, const struct status_state *state) {
    for (int i = 0; i < 5; i++) {
        int x = PROFILE_X + i * PROFILE_DOT_GAP;

        // Draw outline
        lv_draw_rect_dsc_t border_dsc;
        init_rect_dsc(&border_dsc, LVGL_FOREGROUND);
        lv_canvas_draw_rect(canvas, x, PROFILE_Y, PROFILE_DOT_SIZE, PROFILE_DOT_SIZE, &border_dsc);

        // Clear interior
        lv_draw_rect_dsc_t clear_dsc;
        init_rect_dsc(&clear_dsc, LVGL_BACKGROUND);
        lv_canvas_draw_rect(canvas, x + 1, PROFILE_Y + 1,
                            PROFILE_DOT_SIZE - 2, PROFILE_DOT_SIZE - 2, &clear_dsc);
    }
}

static void draw_active_profile(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);

    int x = PROFILE_X + state->active_profile_index * PROFILE_DOT_GAP;
    lv_canvas_draw_rect(canvas, x, PROFILE_Y, PROFILE_DOT_SIZE, PROFILE_DOT_SIZE, &rect_white_dsc);
}

void draw_profile_status(lv_obj_t *canvas, const struct status_state *state) {
    draw_inactive_profiles(canvas, state);
    draw_active_profile(canvas, state);
}
