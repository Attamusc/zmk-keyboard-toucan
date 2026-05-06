#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <stdio.h>
#include <string.h>

#include "layer.h"
#include "../assets/custom_fonts.h"
#include <zmk/physical_layouts.h>
#include <zmk/keymap.h>
#include <zmk/matrix.h>

// Divider line above layer zone
#define LAYER_DIVIDER_Y 34

// Max chars that fit in quinquefive_24 (29px/char × 4 = 116px < 144px screen)
#define MAX_LARGE_FONT_CHARS 4

void draw_layer_status(lv_obj_t *canvas, const struct status_state *state) {
    // Top divider line
    lv_draw_rect_dsc_t line_dsc;
    init_rect_dsc(&line_dsc, LVGL_FOREGROUND);
    lv_canvas_draw_rect(canvas, 0, LAYER_DIVIDER_Y, SCREEN_WIDTH, 1, &line_dsc);

    // Get layer name
    char fallback_layer_name[16];
    const char *layer_name = zmk_keymap_layer_name(zmk_keymap_layer_index_to_id(state->layer_index));

    if (layer_name == NULL || layer_name[0] == '\0') {
        sprintf(fallback_layer_name, "L#%" PRIu8, state->layer_index);
        layer_name = fallback_layer_name;
    }

    // Use large font if name fits, otherwise fall back to small font
    size_t name_len = strlen(layer_name);

    if (name_len <= MAX_LARGE_FONT_CHARS) {
        // Large font — vertically centered in the layer zone
        lv_draw_label_dsc_t label_dsc;
        init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_24, LV_TEXT_ALIGN_CENTER);
        lv_canvas_draw_text(canvas, 0, 65, SCREEN_WIDTH, &label_dsc, layer_name);
    } else {
        // Small font for longer names — still centered but positioned to feel prominent
        lv_draw_label_dsc_t label_dsc;
        init_label_dsc(&label_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);
        lv_canvas_draw_text(canvas, 0, 76, SCREEN_WIDTH, &label_dsc, layer_name);
    }

    // Layer index sub-text (small centered, below name)
    lv_draw_label_dsc_t sub_dsc;
    init_label_dsc(&sub_dsc, LVGL_FOREGROUND, &quinquefive_8, LV_TEXT_ALIGN_CENTER);

    char index_buf[12];
    snprintf(index_buf, sizeof(index_buf), "layer %d", state->layer_index);
    lv_canvas_draw_text(canvas, 0, 100, SCREEN_WIDTH, &sub_dsc, index_buf);
}
