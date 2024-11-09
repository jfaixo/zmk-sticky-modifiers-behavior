/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#define DT_DRV_COMPAT zmk_behavior_sticky_modifiers

#include <zephyr/device.h>
#include <drivers/behavior.h>
#include <zephyr/logging/log.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/behavior.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>
#include <zmk/behavior_queue.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)

struct sticky_modifiers_state {
    // Current instant state of all modifier keys
    uint8_t modifiers_pressed;
    // Modifiers ready to be triggered
    uint8_t modifiers_accumulated;
    // If true, the modifiers are used as normal keys, disabling the sticky behavior
    bool normal_mode;
};

struct sticky_modifiers_state sticky_modifiers_state = {};

static inline void raise_modifiers(uint8_t modifiers_to_raise, bool pressed, int64_t timestamp) {
    while (modifiers_to_raise != 0) {
        uint32_t modifier_index = __builtin_ctz(modifiers_to_raise);
        modifiers_to_raise ^= 1 << modifier_index;

        uint32_t encoded = (HID_USAGE_KEY << 16) | (HID_USAGE_KEY_KEYBOARD_LEFTCONTROL + modifier_index);
        LOG_DBG("raising: 0x%01X", encoded);
        raise_zmk_keycode_state_changed_from_encoded(encoded, pressed, timestamp);
    }
}

static int behavior_sticky_modifiers_init(const struct device *dev) { return 0; };

static int on_sticky_modifiers_binding_pressed(struct zmk_behavior_binding *binding,
                                                struct zmk_behavior_binding_event event) {
    uint8_t modifier_index = binding->param1 & 0xF;
    uint8_t modifier_bit = 1 << modifier_index;
    sticky_modifiers_state.modifiers_pressed |= modifier_bit;
    LOG_DBG("sticky modifier pressed: 0x%01X", sticky_modifiers_state.modifiers_pressed);

    if (sticky_modifiers_state.normal_mode) {
        LOG_DBG("normal mode modifier pressed: 0x%01X", sticky_modifiers_state.modifiers_pressed);
        raise_zmk_keycode_state_changed_from_encoded(binding->param1, true, event.timestamp);
    }
    else if ((sticky_modifiers_state.modifiers_accumulated & modifier_bit) != 0) {
        // Special case of modifier double tap. Entering normal mode
        LOG_DBG("Entering normal mode");
        sticky_modifiers_state.normal_mode = true;
        sticky_modifiers_state.modifiers_accumulated = 0;

        // Raise and lower the key a first time
        raise_modifiers(modifier_bit, true, event.timestamp);
        raise_modifiers(modifier_bit, false, event.timestamp);

        // ...and raise it again
        raise_modifiers(modifier_bit, true, event.timestamp);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

static int on_sticky_modifiers_binding_released(struct zmk_behavior_binding *binding,
                                                 struct zmk_behavior_binding_event event) {
    uint8_t modifier_index = binding->param1 & 0xF;
    uint8_t modifier_bit = 1 << modifier_index;
    sticky_modifiers_state.modifiers_pressed ^= modifier_bit;

    if (sticky_modifiers_state.normal_mode) {
        raise_zmk_keycode_state_changed_from_encoded(binding->param1, false, event.timestamp);
        LOG_DBG("normal mode modifier released: 0x%01X", sticky_modifiers_state.modifiers_pressed);

        if (sticky_modifiers_state.modifiers_pressed == 0) {
            LOG_DBG("Exiting normal mode");
            sticky_modifiers_state.normal_mode = false;
            sticky_modifiers_state.modifiers_accumulated = 0;
        }
    }
    else {
        // Load the modifier to the accumulator
        sticky_modifiers_state.modifiers_accumulated |= modifier_bit;
        LOG_DBG("accumulating: 0x%01X", sticky_modifiers_state.modifiers_accumulated);
    }

    return ZMK_BEHAVIOR_OPAQUE;
}

/* #region Keycode state listener */

static int sticky_modifiers_keycode_state_changed_listener(const zmk_event_t *eh);

ZMK_LISTENER(behavior_sticky_modifiers, sticky_modifiers_keycode_state_changed_listener);
ZMK_SUBSCRIPTION(behavior_sticky_modifiers, zmk_keycode_state_changed);

static int sticky_modifiers_keycode_state_changed_listener(const zmk_event_t *eh) {
    struct zmk_keycode_state_changed *ev = as_zmk_keycode_state_changed(eh);

    if (ev == NULL || !ev->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (sticky_modifiers_state.modifiers_pressed != 0 && !sticky_modifiers_state.normal_mode) {
        // Some modifiers are currently held down, switch to "normal mode" and use OSM keys as standard keys
        LOG_DBG("Entering normal mode");
        sticky_modifiers_state.normal_mode = true;
        sticky_modifiers_state.modifiers_accumulated = 0;

        // Loop over currently held modifiers and press them
        raise_modifiers(sticky_modifiers_state.modifiers_pressed, true, ev->timestamp);
    }
    else if (sticky_modifiers_state.modifiers_accumulated != 0) {
        LOG_DBG("Trigger OSM behavior");
        // Loop over accumulated modifiers and press them
        int64_t timestamp = ev->timestamp;

        // Store & clear to avoid reentrancy issues
        uint8_t modifiers_accumulated = sticky_modifiers_state.modifiers_accumulated;
        sticky_modifiers_state.modifiers_accumulated = 0;

        raise_modifiers(modifiers_accumulated, true, timestamp);

        // Release the event
        struct zmk_keycode_state_changed_event duped_ev = copy_raised_zmk_keycode_state_changed(ev);
        ZMK_EVENT_RELEASE(duped_ev);

        // Release modifiers and clear the accumulator
        raise_modifiers(modifiers_accumulated, false, timestamp);

        return ZMK_EV_EVENT_HANDLED;
    }

    return ZMK_EV_EVENT_BUBBLE;
}

/* #endregion */

static const struct behavior_driver_api behavior_sticky_modifiers_driver_api = {
    .binding_pressed = on_sticky_modifiers_binding_pressed,
    .binding_released = on_sticky_modifiers_binding_released,
};

#define KP_INST(n)                                                                                 \
    BEHAVIOR_DT_INST_DEFINE(n, behavior_sticky_modifiers_init, NULL, NULL, NULL, POST_KERNEL,     \
                            CONFIG_KERNEL_INIT_PRIORITY_DEFAULT,                                   \
                            &behavior_sticky_modifiers_driver_api);

DT_INST_FOREACH_STATUS_OKAY(KP_INST)

#endif /* DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT) */