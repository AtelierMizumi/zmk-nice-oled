#pragma once

#include <lvgl.h>
#include "util.h"

/**
 * Compact horizontal status bar for peripheral OLED screen.
 * Draws BT connection icon + battery bar on a single row.
 *
 * Layout (on the pre-rotation canvas, coordinates are in the
 * "landscape" frame before rotate_canvas() is called):
 *
 *   [BT icon 8x8] [2px gap] [battery bar 18x6] [battery % text]
 *
 * Total height: CONFIG_NICE_OLED_PERIPHERAL_STATUS_BAR_HEIGHT (default 10px)
 */
void draw_peripheral_status_bar(lv_obj_t *canvas, const struct status_state *state);
