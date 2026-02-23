#pragma once

#include <lvgl.h>
#include <zephyr/kernel.h>
#include "util.h"

struct zmk_widget_hammerbeam {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_obj_t *canvas;
    lv_obj_t *art;
    lv_color_t cbuf[32 * 18];
    struct {
        uint8_t battery;
        bool charging;
        bool connected;
    } state;
};

int zmk_widget_hammerbeam_init(struct zmk_widget_hammerbeam *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_hammerbeam_obj(struct zmk_widget_hammerbeam *widget);
