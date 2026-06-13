#include "unity.h"
#include "led_control.h"
#include "driver/gpio.h"

/* ── helpers ──────────────────────────────────────────────────────────────── */

static void reset_gpio(void)
{
    gpio_reset_pin(LED_GPIO);
}

/* ── test cases ───────────────────────────────────────────────────────────── */

TEST_CASE("led_control_init configures GPIO as output", "[led_control]")
{
    reset_gpio();
    led_control_init();

    gpio_io_type_t direction;
    TEST_ASSERT_EQUAL(ESP_OK, gpio_get_drive_capability(LED_GPIO, NULL));

    /* After init the pin must be low */
    TEST_ASSERT_EQUAL_INT(0, gpio_get_level(LED_GPIO));
}

TEST_CASE("gpio set_level toggles pin state", "[led_control]")
{
    reset_gpio();
    led_control_init();

    gpio_set_level(LED_GPIO, 1);
    TEST_ASSERT_EQUAL_INT(1, gpio_get_level(LED_GPIO));

    gpio_set_level(LED_GPIO, 0);
    TEST_ASSERT_EQUAL_INT(0, gpio_get_level(LED_GPIO));
}
