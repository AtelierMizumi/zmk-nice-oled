/*
 * peripheral_status_bar.c - Compact horizontal status bar for peripheral OLED
 *
 * Renders a single-row status bar with:
 *   - BT connection status icon (left, 8x8 pixels)
 *   - Battery level bar (center, proportional fill)
 *   - Battery percentage text (right)
 *
 * Designed for 32x128 nice!oled displays in portrait orientation.
 * This widget draws on the canvas BEFORE rotation, so coordinates
 * use the pre-rotation "landscape" coordinate system where:
 *   X axis = along the 128px dimension (becomes vertical after rotation)
 *   Y axis = along the 32px dimension (becomes horizontal after rotation)
 *
 * After rotate_canvas() in screen_peripheral.c, the status bar appears
 * as a horizontal strip at the TOP of the portrait display.
 */

#include "peripheral_status_bar.h"
#include <fonts.h>
#include <zephyr/kernel.h>

/* ── Compact BT connected icon (8x8) ─────────────────────────────── */
static void draw_bt_connected_icon(lv_obj_t *canvas, int x, int y) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    /* Draw a minimal Bluetooth rune shape:
     *    .#......
     *    .##.....
     *    .#.#..#.
     *    .#..##..
     *    .#..##..
     *    .#.#..#.
     *    .##.....
     *    .#......
     */
    /* Vertical line */
    lv_point_t v_pts[] = {{x + 1, y}, {x + 1, y + 7}};
    lv_canvas_draw_line(canvas, v_pts, 2, &line_dsc);

    /* Top-right diagonal */
    lv_point_t tr1[] = {{x + 1, y}, {x + 3, y + 2}};
    lv_canvas_draw_line(canvas, tr1, 2, &line_dsc);

    /* Top arrow return */
    lv_point_t tr2[] = {{x + 3, y + 2}, {x + 1, y + 4}};
    lv_canvas_draw_line(canvas, tr2, 2, &line_dsc);

    /* Bottom-right diagonal */
    lv_point_t br1[] = {{x + 1, y + 7}, {x + 3, y + 5}};
    lv_canvas_draw_line(canvas, br1, 2, &line_dsc);

    /* Bottom arrow return */
    lv_point_t br2[] = {{x + 3, y + 5}, {x + 1, y + 3}};
    lv_canvas_draw_line(canvas, br2, 2, &line_dsc);

    /* Connection dots */
    lv_draw_rect_dsc_t dot_dsc;
    init_rect_dsc(&dot_dsc, LVGL_FOREGROUND);
    lv_canvas_draw_rect(canvas, x + 5, y + 2, 1, 1, &dot_dsc);
    lv_canvas_draw_rect(canvas, x + 5, y + 5, 1, 1, &dot_dsc);
}

/* ── Compact BT disconnected icon (8x8) ──────────────────────────── */
static void draw_bt_disconnected_icon(lv_obj_t *canvas, int x, int y) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    /* Same BT rune but with X through it */
    /* Vertical line */
    lv_point_t v_pts[] = {{x + 1, y}, {x + 1, y + 7}};
    lv_canvas_draw_line(canvas, v_pts, 2, &line_dsc);

    /* Top-right diagonal */
    lv_point_t tr1[] = {{x + 1, y}, {x + 3, y + 2}};
    lv_canvas_draw_line(canvas, tr1, 2, &line_dsc);

    /* Bottom-right diagonal */
    lv_point_t br1[] = {{x + 1, y + 7}, {x + 3, y + 5}};
    lv_canvas_draw_line(canvas, br1, 2, &line_dsc);

    /* X slash across */
    lv_point_t x1[] = {{x + 4, y + 1}, {x + 7, y + 6}};
    lv_canvas_draw_line(canvas, x1, 2, &line_dsc);
    lv_point_t x2[] = {{x + 7, y + 1}, {x + 4, y + 6}};
    lv_canvas_draw_line(canvas, x2, 2, &line_dsc);
}

/* ── Battery bar ──────────────────────────────────────────────────── */
static void draw_battery_bar(lv_obj_t *canvas, int x, int y,
                             int width, int height, uint8_t level) {
    lv_draw_rect_dsc_t border_dsc;
    init_rect_dsc(&border_dsc, LVGL_FOREGROUND);

    lv_draw_rect_dsc_t bg_dsc;
    init_rect_dsc(&bg_dsc, LVGL_BACKGROUND);

    /* Battery outline */
    lv_canvas_draw_rect(canvas, x, y, width, height, &border_dsc);

    /* Battery terminal nub (right side) */
    int nub_h = height > 4 ? height - 4 : 2;
    int nub_y = y + (height - nub_h) / 2;
    lv_canvas_draw_rect(canvas, x + width, nub_y, 2, nub_h, &border_dsc);

    /* Inner fill area (1px border inset) */
    int inner_x = x + 1;
    int inner_y = y + 1;
    int inner_w = width - 2;
    int inner_h = height - 2;

    /* Clear inner area */
    lv_canvas_draw_rect(canvas, inner_x, inner_y, inner_w, inner_h, &bg_dsc);

    /* Fill proportional to battery level */
    int fill_w = (inner_w * level) / 100;
    if (fill_w > inner_w) fill_w = inner_w;
    if (fill_w < 0) fill_w = 0;

    if (fill_w > 0) {
        lv_canvas_draw_rect(canvas, inner_x, inner_y, fill_w, inner_h, &border_dsc);
    }
}

/* ── Main draw function ───────────────────────────────────────────── */
void draw_peripheral_status_bar(lv_obj_t *canvas, const struct status_state *state) {
    int bar_h = CONFIG_NICE_OLED_PERIPHERAL_STATUS_BAR_HEIGHT;

    /*
     * Pre-rotation canvas coordinate system:
     *   The canvas is CANVAS_HEIGHT x CANVAS_HEIGHT (square buffer).
     *   X goes along what will become the vertical axis (128px on OLED).
     *   Y goes along what will become the horizontal axis (32px on OLED).
     *
     * We draw the status bar in the top-left corner of the pre-rotation canvas.
     * After 90° rotation, this becomes the TOP of the portrait display.
     *
     * Layout: BT icon (8x8) at left, then battery bar, then % text
     */

    /* BT icon - draw at position suitable for pre-rotation canvas */
    int bt_x = 1;
    int bt_y = 1;

    if (state->connected) {
        draw_bt_connected_icon(canvas, bt_x, bt_y);
    } else {
        draw_bt_disconnected_icon(canvas, bt_x, bt_y);
    }

    /* Battery bar */
    int bat_bar_x = 11;  /* After BT icon (8px) + gap (2px) */
    int bat_bar_y = 2;
    int bat_bar_w = 16;
    int bat_bar_h = 6;

    draw_battery_bar(canvas, bat_bar_x, bat_bar_y, bat_bar_w, bat_bar_h, state->battery);

    /* Separator line below the status bar */
    lv_draw_line_dsc_t sep_dsc;
    init_line_dsc(&sep_dsc, LVGL_FOREGROUND, 1);
    lv_point_t sep_pts[] = {{0, bar_h - 1}, {CANVAS_WIDTH, bar_h - 1}};
    lv_canvas_draw_line(canvas, sep_pts, 2, &sep_dsc);
}
