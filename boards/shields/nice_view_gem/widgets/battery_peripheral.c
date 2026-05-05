#include <zephyr/kernel.h>
#include <stdio.h>
#include "battery_peripheral.h"
#include "../assets/custom_fonts.h"

// Right battery icon position (mirrors left battery on right side)
#define BATT_P_X 86
#define BATT_P_Y 4
#define BATT_P_W 32
#define BATT_P_H 12
#define BATT_P_BORDER 2
#define BATT_P_TERM_W 3
#define BATT_P_TERM_H 6
#define BATT_P_FILL_MAX (BATT_P_W - 2 * BATT_P_BORDER)

// Connection dot position
#define CONN_DOT_X (BATT_P_X + BATT_P_W + BATT_P_TERM_W + 4)
#define CONN_DOT_Y (BATT_P_Y + 3)
#define CONN_DOT_SIZE 5

// Percentage text position (second row)
#define PCT_P_Y 20

static void draw_battery_outline(lv_obj_t *canvas, int x, int y) {
    lv_draw_rect_dsc_t border_dsc;
    init_rect_dsc(&border_dsc, LVGL_FOREGROUND);

    lv_canvas_draw_rect(canvas, x, y, BATT_P_W, BATT_P_H, &border_dsc);

    lv_draw_rect_dsc_t clear_dsc;
    init_rect_dsc(&clear_dsc, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, x + BATT_P_BORDER, y + BATT_P_BORDER,
                        BATT_P_W - 2 * BATT_P_BORDER, BATT_P_H - 2 * BATT_P_BORDER, &clear_dsc);

    lv_canvas_draw_rect(canvas, x + BATT_P_W, y + (BATT_P_H - BATT_P_TERM_H) / 2,
                        BATT_P_TERM_W, BATT_P_TERM_H, &border_dsc);
}

static void draw_battery_fill(lv_obj_t *canvas, int x, int y, uint8_t level) {
    if (level == 0) return;

    int fill_w = (BATT_P_FILL_MAX * level) / 100;
    if (fill_w < 1) fill_w = 1;

    lv_draw_rect_dsc_t fill_dsc;
    init_rect_dsc(&fill_dsc, LVGL_FOREGROUND);
    lv_canvas_draw_rect(canvas, x + BATT_P_BORDER, y + BATT_P_BORDER,
                        fill_w, BATT_P_H - 2 * BATT_P_BORDER, &fill_dsc);
}

static void draw_connection_dot(lv_obj_t *canvas, bool connected) {
    if (connected) {
        // Filled dot = connected
        lv_draw_rect_dsc_t dot_dsc;
        init_rect_dsc(&dot_dsc, LVGL_FOREGROUND);
        lv_canvas_draw_rect(canvas, CONN_DOT_X, CONN_DOT_Y, CONN_DOT_SIZE, CONN_DOT_SIZE, &dot_dsc);
    } else {
        // Hollow dot = disconnected
        lv_draw_rect_dsc_t border_dsc;
        init_rect_dsc(&border_dsc, LVGL_FOREGROUND);
        lv_canvas_draw_rect(canvas, CONN_DOT_X, CONN_DOT_Y, CONN_DOT_SIZE, CONN_DOT_SIZE, &border_dsc);

        lv_draw_rect_dsc_t clear_dsc;
        init_rect_dsc(&clear_dsc, LVGL_BACKGROUND);
        lv_canvas_draw_rect(canvas, CONN_DOT_X + 1, CONN_DOT_Y + 1,
                            CONN_DOT_SIZE - 2, CONN_DOT_SIZE - 2, &clear_dsc);
    }
}

void draw_battery_peripheral_status(lv_obj_t *canvas, const struct status_state *state) {
    // "R" label
    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_LEFT);
    lv_canvas_draw_text(canvas, 76, BATT_P_Y + 1, 10, &label_dsc, "R");

    // Battery outline and fill
    draw_battery_outline(canvas, BATT_P_X, BATT_P_Y);
    draw_battery_fill(canvas, BATT_P_X, BATT_P_Y, state->battery_p);

    // Connection status dot
    draw_connection_dot(canvas, state->peripheral_connected);

    // Percentage text on second row
    char pct_buf[5];
    snprintf(pct_buf, sizeof(pct_buf), "%d%%", state->battery_p);
    lv_canvas_draw_text(canvas, BATT_P_X, PCT_P_Y, 40, &label_dsc, pct_buf);
}
