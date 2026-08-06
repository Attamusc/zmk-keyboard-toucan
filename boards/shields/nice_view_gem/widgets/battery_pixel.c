#include <zephyr/kernel.h>
#include <stdio.h>
#include "battery.h"
#include "../assets/custom_fonts.h"

// Battery icon dimensions and position
#define BATT_X 12
#define BATT_Y 4
#define BATT_W 32
#define BATT_H 12
#define BATT_BORDER 2
#define BATT_TERM_W 3
#define BATT_TERM_H 6
#define BATT_FILL_MAX (BATT_W - 2 * BATT_BORDER)

// Bolt icon position (drawn when charging)
#define BOLT_X (BATT_X + BATT_W + BATT_TERM_W + 2)

// Percentage text position (second row, below battery icon)
#define PCT_Y 20

static void draw_battery_outline(lv_obj_t *canvas, int x, int y) {
    lv_draw_rect_dsc_t border_dsc;
    init_rect_dsc(&border_dsc, LVGL_FOREGROUND);

    // Main outline
    lv_canvas_draw_rect(canvas, x, y, BATT_W, BATT_H, &border_dsc);

    // Clear interior
    lv_draw_rect_dsc_t clear_dsc;
    init_rect_dsc(&clear_dsc, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, x + BATT_BORDER, y + BATT_BORDER,
                        BATT_W - 2 * BATT_BORDER, BATT_H - 2 * BATT_BORDER, &clear_dsc);

    // Terminal nub on right
    lv_canvas_draw_rect(canvas, x + BATT_W, y + (BATT_H - BATT_TERM_H) / 2,
                        BATT_TERM_W, BATT_TERM_H, &border_dsc);
}

static void draw_battery_fill(lv_obj_t *canvas, int x, int y, uint8_t level) {
    if (level == 0) return;

    int fill_w = (BATT_FILL_MAX * level) / 100;
    if (fill_w < 1) fill_w = 1;

    lv_draw_rect_dsc_t fill_dsc;
    init_rect_dsc(&fill_dsc, LVGL_FOREGROUND);
    lv_canvas_draw_rect(canvas, x + BATT_BORDER, y + BATT_BORDER,
                        fill_w, BATT_H - 2 * BATT_BORDER, &fill_dsc);
}

static void draw_bolt(lv_obj_t *canvas, int x, int y) {
    // Simple 5x9 lightning bolt drawn with rectangles
    lv_draw_rect_dsc_t dsc;
    init_rect_dsc(&dsc, LVGL_FOREGROUND);

    lv_canvas_draw_rect(canvas, x + 2, y + 0, 3, 1, &dsc);
    lv_canvas_draw_rect(canvas, x + 1, y + 1, 3, 1, &dsc);
    lv_canvas_draw_rect(canvas, x + 0, y + 2, 3, 1, &dsc);
    lv_canvas_draw_rect(canvas, x + 0, y + 3, 5, 1, &dsc);
    lv_canvas_draw_rect(canvas, x + 2, y + 4, 3, 1, &dsc);
    lv_canvas_draw_rect(canvas, x + 3, y + 5, 3, 1, &dsc);
    lv_canvas_draw_rect(canvas, x + 2, y + 6, 3, 1, &dsc);
    lv_canvas_draw_rect(canvas, x + 1, y + 7, 3, 1, &dsc);
    lv_canvas_draw_rect(canvas, x + 0, y + 8, 3, 1, &dsc);
}

void draw_battery_status(lv_obj_t *canvas, const struct status_state *state) {
    // "L" label
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
    lv_canvas_draw_text(canvas, 2, BATT_Y + 1, 10, &label_dsc, "L");

    // Battery outline and fill
    draw_battery_outline(canvas, BATT_X, BATT_Y);
    draw_battery_fill(canvas, BATT_X, BATT_Y, state->battery);

    // Charging bolt
    if (state->charging) {
        draw_bolt(canvas, BOLT_X, BATT_Y + 1);
    }

    // Percentage text on second row
    char pct_buf[5];
    snprintf(pct_buf, sizeof(pct_buf), "%d%%", state->battery);
    lv_canvas_draw_text(canvas, BATT_X, PCT_Y, 40, &label_dsc, pct_buf);
}
