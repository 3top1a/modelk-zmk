#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zmk/event_manager.h>
#include <zmk/events/activity_state_changed.h>
#include <zmk/hid_indicators.h>
#include <zmk/events/hid_indicators_changed.h>
#include <zmk/keymap.h>
#include <zmk/physical_layouts.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

// GPIO configuration for Num Lock LED
// ** Requires LEDs to be named 'numlock_led' **
static const struct gpio_dt_spec numlock_led = GPIO_DT_SPEC_GET(DT_NODELABEL(numlock_led), gpios);
static const struct gpio_dt_spec capslock_led = GPIO_DT_SPEC_GET(DT_NODELABEL(capslock_led), gpios);
static const struct gpio_dt_spec bluetooth_led = GPIO_DT_SPEC_GET(DT_NODELABEL(bluetooth_led), gpios);

// Global variables to store the current state
static bool numlock_active = false;
static bool capslock_active = false;
static bool is_idle = false;

// Helper to set the state of a LED
static int set_led(bool state, struct gpio_dt_spec led) {
    int err = gpio_pin_set_dt(&led, state ? 1 : 0);
    if (err) {
        LOG_ERR("Error setting GPIO pin: %d", err);
        return err;
    }

    LOG_INF("Set pin %d to %s", led.pin, state ? "ON" : "OFF");
    return 0;
}

// Function to apply states to LEDs
static void apply_led_state(void) {
    // If idle and the LED should be on, turn it off during idle
    // TODO add Not pairing condition
    if (is_idle) {
        set_led(false, numlock_led);
        set_led(false, capslock_led);
        set_led(false, bluetooth_led);
        return;
    }

    set_led(numlock_active, numlock_led);
    set_led(capslock_active, capslock_led);
}

// Get HID indicators and set the states
// Does not apply to LEDs, use apply_led_state() right after
static void parse_hid_indicator_flags(void) {
    zmk_hid_indicators_t flags = zmk_hid_indicators_get_current_profile();

    // Update Num Lock state
    numlock_active = (flags & 0x01) ? true : false;
    capslock_active = (flags & 0x02) ? true : false;

    LOG_INF("NUMLOCK is %s", numlock_active ? "ON" : "OFF");
    LOG_INF("CAPSLOCK is %s", capslock_active ? "ON" : "OFF");
}

// Callback when the keyboard enters idle
static void on_idle(void) {
    // printk("Keyboard is idle. Updating Num Lock LED state.\n");
    is_idle = true;
    apply_led_state();
}

// Callback when the keyboard wakes up from idle
static void on_wake_up(void) {
    // printk("Keyboard has woken up from idle. Restoring Num Lock LED state.\n");
    is_idle = false;

    parse_hid_indicator_flags();
    apply_led_state();
}

// Callback for the activity state listener
static int on_activity_state_changed(const zmk_event_t *ev) {
    const struct zmk_activity_state_changed *activity_event = as_zmk_activity_state_changed(ev);

    if (!activity_event) {
        return -1;
    }

    if (activity_event->state == ZMK_ACTIVITY_IDLE) {
        on_idle();
    } else if (activity_event->state == ZMK_ACTIVITY_ACTIVE) {
        on_wake_up();
    }

    return ZMK_EV_EVENT_BUBBLE;
}

// Listener for idle and wake-up state
ZMK_LISTENER(activity_state_changed_listener, on_activity_state_changed);
ZMK_SUBSCRIPTION(activity_state_changed_listener, zmk_activity_state_changed);

// Listener to handle changes in Num Lock state
static int led_locks_listener_cb(const zmk_event_t *eh) {
    parse_hid_indicator_flags();
    apply_led_state(); // Update immediately

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(led_indicators_listener, led_locks_listener_cb);
ZMK_SUBSCRIPTION(led_indicators_listener, zmk_hid_indicators_changed);

// Initialize the LED and work structures on boot
static int leds_init(void) {
    if (!gpio_is_ready_dt(&numlock_led)) {
        LOG_ERR("Error: GPIO device %s is not ready", numlock_led.port->name);
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&capslock_led)) {
        LOG_ERR("Error: GPIO device %s is not ready", numlock_led.port->name);
        return -ENODEV;
    }
    if (!gpio_is_ready_dt(&bluetooth_led)) {
        LOG_ERR("Error: GPIO device %s is not ready", numlock_led.port->name);
        return -ENODEV;
    }

    int err = gpio_pin_configure_dt(&numlock_led, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_ERR("Error configuring GPIO pin: %d", err);
        return err;
    }
    err = gpio_pin_configure_dt(&capslock_led, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_ERR("Error configuring GPIO pin: %d", err);
        return err;
    }
    err = gpio_pin_configure_dt(&bluetooth_led, GPIO_OUTPUT_INACTIVE);
    if (err) {
        LOG_ERR("Error configuring GPIO pin: %d", err);
        return err;
    }

    // Initialize the LED state
    is_idle = false;
    numlock_active = false;
    capslock_active = false;
    apply_led_state();

    LOG_INF("Num Lock LED initialized successfully");
    return 0;
}

// Register leds_init to run at boot
SYS_INIT(leds_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
