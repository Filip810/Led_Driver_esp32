#include "led_control.h"

#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_MODE LEDC_LOW_SPEED_MODE
#define LEDC_FREQ_HZ 5000
#define LEDC_RES LEDC_TIMER_12_BIT
#define LEDC_MAX_DUTY 4095u

/* Set to 1 for common-anode RGB LED.
   Set to 0 for common-cathode */
#define LED_COMMON_ANODE 1

static const char *TAG = "LED_RGB";

static led_rgb_state_t s_state = {
    .r = 255,
    .g = 255,
    .b = 255,
    .brightness = 100,
    .on = false,
    .timer_remaining_s = -1,
};
static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_timer_task = NULL;

/* ── internal helpers ─────────────────────────────────────────────────────── */

static void apply_pwm(void)
{
    float scale = s_state.on ? (s_state.brightness / 100.0f) : 0.0f;

    uint32_t dr = (uint32_t)(s_state.r / 255.0f * scale * LEDC_MAX_DUTY);
    uint32_t dg = (uint32_t)(s_state.g / 255.0f * scale * LEDC_MAX_DUTY);
    uint32_t db = (uint32_t)(s_state.b / 255.0f * scale * LEDC_MAX_DUTY);

#if LED_COMMON_ANODE
    dr = LEDC_MAX_DUTY - dr;
    dg = LEDC_MAX_DUTY - dg;
    db = LEDC_MAX_DUTY - db;
#endif

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_0, dr);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_0);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_1, dg);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_1);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL_2, db);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL_2);
}

/* ── countdown task ───────────────────────────────────────────────────────── */

static void timer_task(void *arg)
{
    uint32_t total_s = (uint32_t)(uintptr_t)arg;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.on = true;
    s_state.timer_remaining_s = (int32_t)total_s;
    apply_pwm();
    xSemaphoreGive(s_mutex);

    for (uint32_t i = 0; i < total_s; i++)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));

        xSemaphoreTake(s_mutex, portMAX_DELAY);
        s_state.timer_remaining_s = (int32_t)(total_s - i - 1);
        xSemaphoreGive(s_mutex);
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.on = false;
    s_state.timer_remaining_s = -1;
    s_timer_task = NULL;
    apply_pwm();
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Timer expired — LED off");
    vTaskDelete(NULL);
}

/* ── public API ───────────────────────────────────────────────────────────── */

void led_rgb_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    ledc_timer_config_t tmr = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_RES,
        .freq_hz = LEDC_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tmr));

    const int gpios[3] = {LED_R_GPIO, LED_G_GPIO, LED_B_GPIO};
    for (int ch = 0; ch < 3; ch++)
    {
        ledc_channel_config_t chan = {
            .speed_mode = LEDC_MODE,
            .channel = ch,
            .timer_sel = LEDC_TIMER,
            .intr_type = LEDC_INTR_DISABLE,
            .gpio_num = gpios[ch],
            .duty = 0,
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&chan));
    }

    ESP_LOGI(TAG, "RGB PWM ready — R=GPIO%d G=GPIO%d B=GPIO%d @ %dHz 12-bit",
             LED_R_GPIO, LED_G_GPIO, LED_B_GPIO, LEDC_FREQ_HZ);
}

void led_rgb_set_color(uint8_t r, uint8_t g, uint8_t b)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.r = r;
    s_state.g = g;
    s_state.b = b;
    apply_pwm();
    xSemaphoreGive(s_mutex);
}

void led_rgb_set_brightness(uint8_t brightness)
{
    if (brightness > 100)
        brightness = 100;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_state.brightness = brightness;
    apply_pwm();
    xSemaphoreGive(s_mutex);
}

void led_rgb_set_power(bool on)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    /* Turning off while a timer is active: cancel the timer first */
    if (!on && s_timer_task)
    {
        TaskHandle_t t = s_timer_task;
        s_timer_task = NULL;
        s_state.timer_remaining_s = -1;
        xSemaphoreGive(s_mutex);
        vTaskDelete(t);
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }

    s_state.on = on;
    apply_pwm();
    xSemaphoreGive(s_mutex);
}

void led_rgb_timer_start(uint32_t seconds)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_timer_task)
    {
        TaskHandle_t t = s_timer_task;
        s_timer_task = NULL;
        s_state.timer_remaining_s = -1;
        xSemaphoreGive(s_mutex);
        vTaskDelete(t);
        xSemaphoreTake(s_mutex, portMAX_DELAY);
    }
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Timer started — %lu s", (unsigned long)seconds);
    xTaskCreate(timer_task, "led_tmr", 2048, (void *)(uintptr_t)seconds, 4, &s_timer_task);
}

void led_rgb_timer_stop(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_timer_task)
    {
        TaskHandle_t t = s_timer_task;
        s_timer_task = NULL;
        s_state.timer_remaining_s = -1;
        xSemaphoreGive(s_mutex);
        vTaskDelete(t);
        return;
    }
    xSemaphoreGive(s_mutex);
}

void led_rgb_get_state(led_rgb_state_t *out)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *out = s_state;
    xSemaphoreGive(s_mutex);
}
