#include "esp_log.h"
#include "led_control.h"
#include "wifi_manager.h"
#include "http_server.h"

static const char *TAG = "MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "RGB LED Controller starting…");

    led_rgb_init();
    wifi_manager_init();
    wifi_manager_wait_connected();
    http_server_start();

    ESP_LOGI(TAG, "Ready open http://<IP> in your browser");
}
