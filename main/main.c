#include "esp_log.h"
#include "nvs_flash.h"
#include "wifi_manager.h"

static const char *TAG = "BLOCK_AD";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting ESP32-S3 Ad Blocker");

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(wifi_manager_start());

    while (wifi_manager_wait_for_ip(30000) != ESP_OK) {
        ESP_LOGW(TAG, "Waiting for WiFi...");
    }

    ESP_LOGI(TAG, "Network ready");
}
