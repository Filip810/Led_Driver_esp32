#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LED_R_GPIO 25
#define LED_G_GPIO 26
#define LED_B_GPIO 27

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t brightness;
    bool on;
    int32_t timer_remaining_s;
} led_rgb_state_t;

void led_rgb_init(void);
void led_rgb_set_color(uint8_t r, uint8_t g, uint8_t b);
void led_rgb_set_brightness(uint8_t brightness);
void led_rgb_set_power(bool on);
void led_rgb_timer_start(uint32_t seconds);
void led_rgb_timer_stop(void);
void led_rgb_get_state(led_rgb_state_t *out);
