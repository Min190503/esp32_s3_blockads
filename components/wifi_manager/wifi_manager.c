#include "wifi_manager.h"

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_RETRY_DELAY_US (3 * 1000000ULL)

static const char *TAG = "WIFI_MANAGER";

static EventGroupHandle_t s_wifi_event_group;
static esp_timer_handle_t s_retry_timer;
static bool s_started = false;
static int s_retry_count = 0;

static void retry_timer_callback(void *arg)
{
    s_retry_count++;

    ESP_LOGI(TAG, "WiFi reconnect attempt: %lu", (unsigned long)s_retry_count);

    esp_err_t err = esp_wifi_connect();

    if (err != ESP_OK)
    {
        ESP_LOGW(TAG, "Reconnect error: %s", esp_err_to_name(err));

        esp_err_t time_err = esp_timer_start_once(s_retry_timer, WIFI_RETRY_DELAY_US);

        if (time_err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to schedule retry");
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Starting WiFi connection");
        esp_wifi_connect();
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "WiFi disconnected");

        if (!esp_timer_is_active(s_retry_timer))
        {
            esp_err_t err = esp_timer_start_once(s_retry_timer, WIFI_RETRY_DELAY_US);

            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Cannot schedule reconnect");
            }
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = event_data;

        if (esp_timer_is_active(s_retry_timer))
        {
            esp_timer_stop(s_retry_timer);
        }

        s_retry_count = 0;

        ESP_LOGI(TAG, "IP Address: " IPSTR, IP2STR(&event->ip_info.ip));

        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t wifi_manager_start(void)
{
    if (s_started)
    {
        return ESP_ERR_INVALID_STATE;
    }

    if (CONFIG_BLOCK_AD_WIFI_SSID[0] == '\0')
    {
        ESP_LOGE(TAG, "WiFi SSID is not configured");
        return ESP_ERR_INVALID_ARG;
    }

    s_wifi_event_group = xEventGroupCreate();

    if (s_wifi_event_group == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_wifi_set_storage(WIFI_STORAGE_RAM));

    esp_timer_create_args_t retry_timer_args = {
        .callback = retry_timer_callback,
        .name = "wifi_retry"};

    ESP_ERROR_CHECK(
        esp_timer_create(&retry_timer_args, &s_retry_timer));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT,
        ESP_EVENT_ANY_ID,
        wifi_event_handler,
        NULL));

    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT,
        IP_EVENT_STA_GOT_IP,
        wifi_event_handler,
        NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_BLOCK_AD_WIFI_SSID,
            .password = CONFIG_BLOCK_AD_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false}}};

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    ESP_ERROR_CHECK(
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    s_started = true;

    return ESP_OK;
}

bool wifi_manager_is_connected(void)
{
    if (s_wifi_event_group == NULL)
    {
        return false;
    }

    return (xEventGroupGetBits(s_wifi_event_group) & WIFI_CONNECTED_BIT) != 0;
}

esp_err_t wifi_manager_wait_for_ip(uint32_t timeout_ms)
{
    if (s_wifi_event_group == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(timeout_ms));

    return (bits & WIFI_CONNECTED_BIT)
               ? ESP_OK
               : ESP_ERR_TIMEOUT;
}
