/*
 * One key press per two-finger swipe.
 *
 * The trackpad reports a two-finger swipe as scroll (INPUT_REL_HWHEEL / INPUT_REL_WHEEL, one axis per report).
 * Each axis is summed; the first to reach `threshold` fires its binding once and the rest of the swipe is swallowed.
 * A swipe ends when the fingers lift (INPUT_BTN_TOUCH release) or no scroll arrives for `gap-ms`.
 * Every scroll event that reaches this processor is consumed, so nothing scrolls while it is in the chain.
 */

#define DT_DRV_COMPAT zmk_input_processor_swipe_keys

#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <drivers/input_processor.h>
#include <zmk/behavior.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

/* bindings order, by the direction the fingers move */
enum { SWIPE_UP, SWIPE_RIGHT, SWIPE_DOWN, SWIPE_LEFT };

struct swipe_keys_config {
    int32_t threshold;
    int32_t gap_ms;
    struct zmk_behavior_binding bindings[4];
};

struct swipe_keys_data {
    int32_t x;
    int32_t y;
    bool fired;
    int64_t last_ms;
};

static void swipe_keys_reset(struct swipe_keys_data *data) {
    data->x = 0;
    data->y = 0;
    data->fired = false;
}

static int swipe_keys_handle_event(const struct device *dev, struct input_event *event,
                                   uint32_t param1, uint32_t param2,
                                   struct zmk_input_processor_state *state) {
    const struct swipe_keys_config *cfg = dev->config;
    struct swipe_keys_data *data = dev->data;

    if (event->type == INPUT_EV_KEY && event->code == INPUT_BTN_TOUCH && event->value == 0) {
        swipe_keys_reset(data);
        return ZMK_INPUT_PROC_CONTINUE;
    }
    if (event->type != INPUT_EV_REL ||
        (event->code != INPUT_REL_HWHEEL && event->code != INPUT_REL_WHEEL) || event->value == 0) {
        return ZMK_INPUT_PROC_CONTINUE;
    }

    int64_t now = k_uptime_get();
    if (now - data->last_ms > cfg->gap_ms) {
        swipe_keys_reset(data);
    }
    data->last_ms = now;

    int32_t value = event->value;
    /* zeroed as well as stopped: a layer override without process-next turns STOP back into CONTINUE */
    event->value = 0;
    if (data->fired) {
        return ZMK_INPUT_PROC_STOP;
    }

    /* HWHEEL > 0: fingers moved right. WHEEL > 0: fingers moved up (the driver's invert-scroll-y). */
    if (event->code == INPUT_REL_HWHEEL) {
        data->x += value;
    } else {
        data->y += value;
    }

    int dir;
    if (abs(data->x) >= cfg->threshold && abs(data->x) >= abs(data->y)) {
        dir = data->x > 0 ? SWIPE_RIGHT : SWIPE_LEFT;
    } else if (abs(data->y) >= cfg->threshold) {
        dir = data->y > 0 ? SWIPE_UP : SWIPE_DOWN;
    } else {
        return ZMK_INPUT_PROC_STOP;
    }

    data->fired = true;
    struct zmk_behavior_binding_event bev = {
        .position = INT32_MAX,
        .timestamp = now,
    };
    zmk_behavior_invoke_binding(&cfg->bindings[dir], bev, true);
    zmk_behavior_invoke_binding(&cfg->bindings[dir], bev, false);
    return ZMK_INPUT_PROC_STOP;
}

static const struct zmk_input_processor_driver_api swipe_keys_driver_api = {
    .handle_event = swipe_keys_handle_event,
};

#define SWIPE_KEYS_BINDING(n, idx)                                                                 \
    {                                                                                              \
        .behavior_dev = DEVICE_DT_NAME(DT_INST_PHANDLE_BY_IDX(n, bindings, idx)),                  \
        .param1 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, idx, param1), (0),          \
                              (DT_INST_PHA_BY_IDX(n, bindings, idx, param1))),                     \
        .param2 = COND_CODE_0(DT_INST_PHA_HAS_CELL_AT_IDX(n, bindings, idx, param2), (0),          \
                              (DT_INST_PHA_BY_IDX(n, bindings, idx, param2))),                     \
    }

#define SWIPE_KEYS_INST(n)                                                                         \
    BUILD_ASSERT(DT_INST_PROP_LEN(n, bindings) == 4, "bindings: up, right, down, left");           \
    static const struct swipe_keys_config swipe_keys_config_##n = {                                \
        .threshold = DT_INST_PROP(n, threshold),                                                   \
        .gap_ms = DT_INST_PROP(n, gap_ms),                                                         \
        .bindings = {SWIPE_KEYS_BINDING(n, 0), SWIPE_KEYS_BINDING(n, 1),                           \
                     SWIPE_KEYS_BINDING(n, 2), SWIPE_KEYS_BINDING(n, 3)},                          \
    };                                                                                             \
    static struct swipe_keys_data swipe_keys_data_##n;                                             \
    DEVICE_DT_INST_DEFINE(n, NULL, NULL, &swipe_keys_data_##n, &swipe_keys_config_##n,             \
                          POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                        \
                          &swipe_keys_driver_api);

DT_INST_FOREACH_STATUS_OKAY(SWIPE_KEYS_INST)
