#include <stdint.h>

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_event.h"
#include "esp_https_ota.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "esp_app_desc.h"

static const char *TAG = "OTA_NODE";

extern const uint8_t ota_dev_ca_crt_start[]
    asm("_binary_ota_dev_ca_crt_start");

extern const uint8_t ota_dev_ca_crt_end[]
    asm("_binary_ota_dev_ca_crt_end");

#define WIFI_CONNECTED_BIT BIT0

static EventGroupHandle_t wifi_event_group;

static void wifi_event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START) {

        ESP_LOGI(TAG, "Wi-Fi started. Connecting...");
        ESP_ERROR_CHECK(esp_wifi_connect());

    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {

        ESP_LOGW(TAG, "Wi-Fi disconnected. Retrying...");
        esp_wifi_connect();

    } else if (event_base == IP_EVENT &&
               event_id == IP_EVENT_STA_GOT_IP) {

        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        ESP_LOGI(
            TAG,
            "Connected! IP address: " IPSTR,
            IP2STR(&event->ip_info.ip)
        );

        xEventGroupSetBits(
            wifi_event_group,
            WIFI_CONNECTED_BIT
        );
    }
}

static void wifi_init(void)
{
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(
        esp_event_loop_create_default()
    );

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg =
        WIFI_INIT_CONFIG_DEFAULT();

    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &wifi_event_handler,
            NULL
        )
    );

    ESP_ERROR_CHECK(
        esp_event_handler_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &wifi_event_handler,
            NULL
        )
    );

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_OTA_NODE_WIFI_SSID,
            .password = CONFIG_OTA_NODE_WIFI_PASSWORD,
        },
    };

    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );
}

static int compare_versions(
    const char *current,
    const char *available)
{
    int current_major = 0;
    int current_minor = 0;
    int current_patch = 0;

    int available_major = 0;
    int available_minor = 0;
    int available_patch = 0;

    if (sscanf(
            current,
            "%d.%d.%d",
            &current_major,
            &current_minor,
            &current_patch) != 3) {

        return -1;
    }

    if (sscanf(
            available,
            "%d.%d.%d",
            &available_major,
            &available_minor,
            &available_patch) != 3) {

        return -1;
    }

    if (available_major != current_major) {
        return available_major > current_major ? 1 : -1;
    }

    if (available_minor != current_minor) {
        return available_minor > current_minor ? 1 : -1;
    }

    if (available_patch != current_patch) {
        return available_patch > current_patch ? 1 : -1;
    }

    return 0;
}

static void ota_check(void)
{
#if CONFIG_OTA_NODE_AUTO_UPDATE

    if (strlen(CONFIG_OTA_NODE_FIRMWARE_URL) == 0) {
        ESP_LOGW(TAG, "OTA enabled but firmware URL is empty.");
        return;
    }

    const esp_app_desc_t *current_app =
        esp_app_get_description();

    ESP_LOGI(
        TAG,
        "Current firmware version: %s",
        current_app->version
    );

    esp_http_client_config_t http_config = {
    .url = CONFIG_OTA_NODE_FIRMWARE_URL,
    .cert_pem = (const char *)ota_dev_ca_crt_start,
    .timeout_ms = 10000,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &http_config,
    };

    esp_https_ota_handle_t ota_handle = NULL;

    ESP_LOGI(TAG, "Checking for OTA firmware...");

    esp_err_t err =
        esp_https_ota_begin(
            &ota_config,
            &ota_handle
        );

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "OTA connection failed: %s",
            esp_err_to_name(err)
        );
        return;
    }

    esp_app_desc_t new_app;

    err = esp_https_ota_get_img_desc(
        ota_handle,
        &new_app
    );

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "Could not read OTA image information: %s",
            esp_err_to_name(err)
        );

        esp_https_ota_abort(ota_handle);
        return;
    }

    ESP_LOGI(
        TAG,
        "Available firmware version: %s",
        new_app.version
    );

    int version_result =
        compare_versions(
            current_app->version,
            new_app.version
        );

    if (version_result == -2) {
    ESP_LOGE(
        TAG,
        "Invalid OTA version format."
    );

    esp_https_ota_abort(ota_handle);
    return;
    }

    if (version_result == -1) {
        ESP_LOGI(
            TAG,
            "Available firmware is older than the current firmware."
        );

        esp_https_ota_abort(ota_handle);
        return;
    }

    if (version_result == 0) {
        ESP_LOGI(
            TAG,
            "Firmware is already up to date."
        );

        esp_https_ota_abort(ota_handle);
        return;
    }

    ESP_LOGI(
        TAG,
        "New firmware found. Starting OTA..."
    );

    while (true) {
        err = esp_https_ota_perform(ota_handle);

        if (err == ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            int image_size =
                esp_https_ota_get_image_size(
                    ota_handle
                );

            int image_read =
                esp_https_ota_get_image_len_read(
                    ota_handle
                );

            if (image_size > 0) {
                ESP_LOGI(
                    TAG,
                    "OTA progress: %d / %d bytes",
                    image_read,
                    image_size
                );
            }

            continue;
        }

        break;
    }

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "OTA download failed: %s",
            esp_err_to_name(err)
        );

        esp_https_ota_abort(ota_handle);
        return;
    }

    err = esp_https_ota_finish(ota_handle);

    if (err != ESP_OK) {
        ESP_LOGE(
            TAG,
            "OTA finalization failed: %s",
            esp_err_to_name(err)
        );
        return;
    }

    ESP_LOGI(
        TAG,
        "OTA successful. Restarting..."
    );

    esp_restart();

#else

    ESP_LOGI(
        TAG,
        "Automatic OTA is disabled."
    );

#endif
}

void app_main(void)
{
    const esp_app_desc_t *app_desc =
        esp_app_get_description();

    ESP_LOGI(TAG, "=================================");
    ESP_LOGI(TAG, "OTA Node starting - OTA UPDATE TEST 1.0.2");
    ESP_LOGI(
        TAG,
        "Firmware version: %s",
        app_desc->version
    );
    ESP_LOGI(TAG, "=================================");

    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {

        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    wifi_init();

    ESP_LOGI(
        TAG,
        "Waiting for Wi-Fi connection..."
    );

    xEventGroupWaitBits(
        wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdFALSE,
        portMAX_DELAY
    );

    ESP_LOGI(TAG, "Network is ready.");

    ota_check();
}


