#include <zephyr/kernel.h>
#include "output.h"
#include "../assets/custom_fonts.h"

// Bottom divider line
#define OUTPUT_DIVIDER_Y 130

// Output status position
#define OUTPUT_TEXT_X 8
#define OUTPUT_TEXT_Y 142

// BLE connected dot position
#define BLE_DOT_X 38
#define BLE_DOT_Y 144
#define BLE_DOT_SIZE 5

static void draw_connected_dot(lv_obj_t *canvas, bool connected) {
    if (connected) {
        lv_draw_rect_dsc_t dot_dsc;
        init_rect_dsc(&dot_dsc, LVGL_FOREGROUND);
        lv_canvas_draw_rect(canvas, BLE_DOT_X, BLE_DOT_Y, BLE_DOT_SIZE, BLE_DOT_SIZE, &dot_dsc);
    } else {
        // Hollow dot = not connected
        lv_draw_rect_dsc_t border_dsc;
        init_rect_dsc(&border_dsc, LVGL_FOREGROUND);
        lv_canvas_draw_rect(canvas, BLE_DOT_X, BLE_DOT_Y, BLE_DOT_SIZE, BLE_DOT_SIZE, &border_dsc);

        lv_draw_rect_dsc_t clear_dsc;
        init_rect_dsc(&clear_dsc, LVGL_BACKGROUND);
        lv_canvas_draw_rect(canvas, BLE_DOT_X + 1, BLE_DOT_Y + 1,
                            BLE_DOT_SIZE - 2, BLE_DOT_SIZE - 2, &clear_dsc);
    }
}

void draw_output_status(lv_obj_t *canvas, const struct status_state *state) {
    // Bottom divider line
    lv_draw_rect_dsc_t line_dsc;
    init_rect_dsc(&line_dsc, LVGL_FOREGROUND);
    lv_canvas_draw_rect(canvas, 0, OUTPUT_DIVIDER_Y, SCREEN_WIDTH, 1, &line_dsc);

    lv_draw_label_dsc_t label_dsc;
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    switch (state->selected_endpoint.transport) {
    case ZMK_TRANSPORT_USB:
        lv_canvas_draw_text(canvas, OUTPUT_TEXT_X, OUTPUT_TEXT_Y, 30, &label_dsc, "USB");
        // USB is always "connected"
        draw_connected_dot(canvas, true);
        break;
    case ZMK_TRANSPORT_BLE:
        lv_canvas_draw_text(canvas, OUTPUT_TEXT_X, OUTPUT_TEXT_Y, 30, &label_dsc, "BLE");
        draw_connected_dot(canvas, state->active_profile_connected);
        break;
    default:
        lv_canvas_draw_text(canvas, OUTPUT_TEXT_X, OUTPUT_TEXT_Y, 30, &label_dsc, "---");
        draw_connected_dot(canvas, false);
        break;
    }
#endif
}
