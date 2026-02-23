#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/split_peripheral_status_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/split/bluetooth/peripheral.h>
#include <zmk/usb.h>

#include "hammerbeam_custom_widget.h"
#include "util.h"

static sys_slist_t hb_widgets = SYS_SLIST_STATIC_INIT(&hb_widgets);

LV_IMG_DECLARE(hammer_beam_compact);

/* ── Compact BT connected icon (8x8) ─────────────────────────────── */
static void draw_bt_connected_icon(lv_obj_t *canvas, int x, int y) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    lv_point_t v_pts[] = {{x + 1, y}, {x + 1, y + 7}};
    lv_canvas_draw_line(canvas, v_pts, 2, &line_dsc);
    lv_point_t tr1[] = {{x + 1, y}, {x + 3, y + 2}};
    lv_canvas_draw_line(canvas, tr1, 2, &line_dsc);
    lv_point_t tr2[] = {{x + 3, y + 2}, {x + 1, y + 4}};
    lv_canvas_draw_line(canvas, tr2, 2, &line_dsc);
    lv_point_t br1[] = {{x + 1, y + 7}, {x + 3, y + 5}};
    lv_canvas_draw_line(canvas, br1, 2, &line_dsc);
    lv_point_t br2[] = {{x + 3, y + 5}, {x + 1, y + 3}};
    lv_canvas_draw_line(canvas, br2, 2, &line_dsc);

    lv_draw_rect_dsc_t dot_dsc;
    init_rect_dsc(&dot_dsc, LVGL_FOREGROUND);
    lv_canvas_draw_rect(canvas, x + 5, y + 2, 1, 1, &dot_dsc);
    lv_canvas_draw_rect(canvas, x + 5, y + 5, 1, 1, &dot_dsc);
}

/* ── Compact BT disconnected icon (8x8) ──────────────────────────── */
static void draw_bt_disconnected_icon(lv_obj_t *canvas, int x, int y) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);

    lv_point_t v_pts[] = {{x + 1, y}, {x + 1, y + 7}};
    lv_canvas_draw_line(canvas, v_pts, 2, &line_dsc);
    lv_point_t tr1[] = {{x + 1, y}, {x + 3, y + 2}};
    lv_canvas_draw_line(canvas, tr1, 2, &line_dsc);
    lv_point_t br1[] = {{x + 1, y + 7}, {x + 3, y + 5}};
    lv_canvas_draw_line(canvas, br1, 2, &line_dsc);
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

    lv_canvas_draw_rect(canvas, x, y, width, height, &border_dsc);
    int nub_h = height > 4 ? height - 4 : 2;
    int nub_y = y + (height - nub_h) / 2;
    lv_canvas_draw_rect(canvas, x + width, nub_y, 2, nub_h, &border_dsc);

    int inner_x = x + 1;
    int inner_y = y + 1;
    int inner_w = width - 2;
    int inner_h = height - 2;

    lv_canvas_draw_rect(canvas, inner_x, inner_y, inner_w, inner_h, &bg_dsc);
    int fill_w = (inner_w * level) / 100;
    if (fill_w > inner_w) fill_w = inner_w;
    if (fill_w < 0) fill_w = 0;

    if (fill_w > 0) {
        lv_canvas_draw_rect(canvas, inner_x, inner_y, fill_w, inner_h, &border_dsc);
    }
}

static void draw_hammerbeam_status_bar(struct zmk_widget_hammerbeam *widget) {
    lv_obj_t *canvas = widget->canvas;
    
    lv_draw_rect_dsc_t bg_dsc;
    init_rect_dsc(&bg_dsc, LVGL_BACKGROUND);
    lv_canvas_draw_rect(canvas, 0, 0, 32, 18, &bg_dsc);

    int bt_x = 2;
    int bt_y = 4;
    if (widget->state.connected) {
        draw_bt_connected_icon(canvas, bt_x, bt_y);
    } else {
        draw_bt_disconnected_icon(canvas, bt_x, bt_y);
    }

    int bat_bar_x = 12;
    int bat_bar_y = 5;
    int bat_bar_w = 16;
    int bat_bar_h = 6;
    draw_battery_bar(canvas, bat_bar_x, bat_bar_y, bat_bar_w, bat_bar_h, widget->state.battery);

    lv_draw_line_dsc_t sep_dsc;
    init_line_dsc(&sep_dsc, LVGL_FOREGROUND, 1);
    lv_point_t sep_pts[] = {{0, 17}, {32, 17}};
    lv_canvas_draw_line(canvas, sep_pts, 2, &sep_dsc);
}

struct battery_status_state {
    uint8_t level;
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    bool usb_present;
#endif
};

static void set_battery_status(struct zmk_widget_hammerbeam *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif
    widget->state.battery = state.level;
    draw_hammerbeam_status_bar(widget);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_hammerbeam *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&hb_widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_hb_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state);
ZMK_SUBSCRIPTION(widget_hb_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_hb_battery_status, zmk_usb_conn_state_changed);
#endif

struct peripheral_status_state {
    bool connected;
};

static struct peripheral_status_state get_state(const zmk_event_t *_eh) {
    return (struct peripheral_status_state){.connected = zmk_split_bt_peripheral_is_connected()};
}

static void set_connection_status(struct zmk_widget_hammerbeam *widget,
                                  struct peripheral_status_state state) {
    widget->state.connected = state.connected;
    draw_hammerbeam_status_bar(widget);
}

static void output_status_update_cb(struct peripheral_status_state state) {
    struct zmk_widget_hammerbeam *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&hb_widgets, widget, node) { set_connection_status(widget, state); }
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_hb_peripheral_status, struct peripheral_status_state,
                            output_status_update_cb, get_state)
ZMK_SUBSCRIPTION(widget_hb_peripheral_status, zmk_split_peripheral_status_changed);

int zmk_widget_hammerbeam_init(struct zmk_widget_hammerbeam *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, 32, 128);

    widget->canvas = lv_canvas_create(widget->obj);
    lv_obj_align(widget->canvas, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_canvas_set_buffer(widget->canvas, widget->cbuf, 32, 18, LV_IMG_CF_TRUE_COLOR);

    widget->art = lv_img_create(widget->obj);
    lv_img_set_src(widget->art, &hammer_beam_compact);
    lv_obj_align(widget->art, LV_ALIGN_TOP_LEFT, 0, 18);

    sys_slist_append(&hb_widgets, &widget->node);
    
    widget_hb_battery_status_init();
    widget_hb_peripheral_status_init();

    return 0;
}

lv_obj_t *zmk_widget_hammerbeam_obj(struct zmk_widget_hammerbeam *widget) { 
    return widget->obj; 
}
