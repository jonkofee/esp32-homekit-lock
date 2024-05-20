/**

   Copyright 2024 Achim Pieters | StudioPieters®

   Permission is hereby granted, free of charge, to any person obtaining a copy
   of this software and associated documentation files (the "Software"), to deal
   in the Software without restriction, including without limitation the rights
   to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
   copies of the Software, and to permit persons to whom the Software is
   furnished to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be included in all
   copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
   IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
   FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT. IN NO EVENT SHALL THE
   AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
   WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
   CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

   for more information visit https://www.studiopieters.nl

 **/

#include <stdio.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <homekit/homekit.h>
#include <homekit/characteristics.h>

// WiFi setup
void on_wifi_ready();

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
        if (event_base == WIFI_EVENT && (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED)) {
                ESP_LOGI("WIFI_EVENT", "STA start");
                esp_wifi_connect();
        } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
                ESP_LOGI("IP_EVENT", "WiFI ready");
                on_wifi_ready();
        }
}

static void wifi_init() {
        ESP_ERROR_CHECK(esp_netif_init());
        ESP_ERROR_CHECK(esp_event_loop_create_default());
        esp_netif_create_default_wifi_sta();

        ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
        ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

        wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));
        ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

        wifi_config_t wifi_config = {
                .sta = {
                        .ssid = CONFIG_ESP_WIFI_SSID,
                        .password = CONFIG_ESP_WIFI_PASSWORD,
                },
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());
}

// Possible values for characteristic CURRENT_DOOR_STATE:
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED 1
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING 2
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING 3
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_STOPPED 4
#define HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_UNKNOWN 255

#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN 0
#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED 1
#define HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_UNKNOWN 255

// LED control
#define LED_GPIO CONFIG_ESP_LED_GPIO
#define LIGHTBULB_RELAY_GPIO CONFIG_ESP_LIGHTBULB_RELAY_GPIO
#define LOCK_RELAY_GPIO CONFIG_ESP_LOCK_RELAY_GPIO
uint32_t door_operation_start_time = 0;
#define MAX_DOOR_OPERATION_TIME CONFIG_ESP_DELAY  // seconds in milliseconds

void lightbulb_relay_on_set(homekit_value_t value);
homekit_value_t lightbulb_relay_on_get();

homekit_characteristic_t lightbulb_on_state = HOMEKIT_CHARACTERISTIC_(ON, false, .getter=lightbulb_relay_on_get, .setter=lightbulb_relay_on_set);

void lightbulb_relay_write(bool on) {
    gpio_set_level(LIGHTBULB_RELAY_GPIO, on ? 1 : 0);
}

homekit_value_t lightbulb_relay_on_get() {
    return lightbulb_on_state.value;
}

void lightbulb_relay_on_set(homekit_value_t value) {
    if (value.format != homekit_format_bool) {
        printf("Invalid value format: %d\n", value.format);
        return;
    }

    lightbulb_on_state.value = value;
    homekit_characteristic_notify(&lightbulb_on_state, lightbulb_on_state.value);
    lightbulb_relay_write(lightbulb_on_state.value.bool_value);
}

void led_write(bool on) {
        gpio_set_level(LED_GPIO, on ? 1 : 0);
}

void lock_relay_write(bool on) {
        gpio_set_level(LOCK_RELAY_GPIO, on ? 1 : 0);
}

// Forward declaration
void garage_door_target_state_set(homekit_value_t value);

// Garage Door Opener Characteristics
homekit_characteristic_t garage_door_obstruction_detected = HOMEKIT_CHARACTERISTIC_(OBSTRUCTION_DETECTED,false);
homekit_characteristic_t garage_door_current_state = HOMEKIT_CHARACTERISTIC_(CURRENT_DOOR_STATE,HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED);
homekit_characteristic_t garage_door_target_state = HOMEKIT_CHARACTERISTIC_(TARGET_DOOR_STATE,HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED,.setter = garage_door_target_state_set);

void garage_door_target_state_set(homekit_value_t value);

void garage_door_target_state_set(homekit_value_t value) {
        lock_relay_write(true);
        door_operation_start_time = esp_timer_get_time() / 1000; // Convert to milliseconds
        vTaskDelay(pdMS_TO_TICKS(MAX_DOOR_OPERATION_TIME)); // Simulate door operation time
        lock_relay_write(false);

//        garage_door_target_state.value = HOMEKIT_UINT8(HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_OPEN);
//        homekit_characteristic_notify(&garage_door_target_state, garage_door_target_state.value);
        vTaskDelay(pdMS_TO_TICKS(500)); // Simulate door operation time

        ESP_LOGI("CURRENT OPENING", "");
        garage_door_current_state.value = HOMEKIT_UINT8(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPENING);
        homekit_characteristic_notify(&garage_door_current_state, garage_door_current_state.value);
        vTaskDelay(pdMS_TO_TICKS(500)); // Simulate door operation time
        ESP_LOGI("CURRENT OPEN", "");
        garage_door_current_state.value = HOMEKIT_UINT8(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_OPEN);
        homekit_characteristic_notify(&garage_door_current_state, garage_door_current_state.value);

        vTaskDelay(pdMS_TO_TICKS(500)); // Simulate door operation time
        ESP_LOGI("TARGET CLOSED", "");
        garage_door_target_state.value = HOMEKIT_UINT8(HOMEKIT_CHARACTERISTIC_TARGET_DOOR_STATE_CLOSED);
        homekit_characteristic_notify(&garage_door_target_state, garage_door_target_state.value);
        vTaskDelay(pdMS_TO_TICKS(500)); // Simulate door operation time
        ESP_LOGI("CURRENT CLOSING", "");
        garage_door_current_state.value = HOMEKIT_UINT8(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSING);
        homekit_characteristic_notify(&garage_door_current_state, garage_door_current_state.value);
        vTaskDelay(pdMS_TO_TICKS(500)); // Simulate door operation time
        ESP_LOGI("CURRENT CLOSED", "");
        garage_door_current_state.value = HOMEKIT_UINT8(HOMEKIT_CHARACTERISTIC_CURRENT_DOOR_STATE_CLOSED);
        homekit_characteristic_notify(&garage_door_current_state, garage_door_current_state.value);
}


// All GPIO Settings
void gpio_init() {
        gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
        led_write(false);

        gpio_set_direction(LIGHTBULB_RELAY_GPIO, GPIO_MODE_OUTPUT);
        lightbulb_relay_write(false);

        gpio_set_direction(LOCK_RELAY_GPIO, GPIO_MODE_OUTPUT);
        lock_relay_write(false);
}

// Accessory identification
void accessory_identify_task(void *args) {
        for (int i = 0; i < 3; i++) {
                for (int j = 0; j < 2; j++) {
                        led_write(true);
                        lightbulb_relay_write(true);
                        vTaskDelay(pdMS_TO_TICKS(100));
                        led_write(false);
                        lightbulb_relay_write(false);
                        vTaskDelay(pdMS_TO_TICKS(100));
                }
                vTaskDelay(pdMS_TO_TICKS(250));
        }
        led_write(false);
        lightbulb_relay_write(false);
        vTaskDelete(NULL);
}

void accessory_identify(homekit_value_t _value) {
        ESP_LOGI("ACCESSORY_IDENTIFY", "Accessory identify");
        xTaskCreate(accessory_identify_task, "Accessory identify", 2048, NULL, 2, NULL);
}

// HomeKit characteristics
#define DEVICE_NAME "Калитка"
#define DEVICE_MANUFACTURER "jonkofee"
#define DEVICE_SERIAL "NLDA4SQN1466"
#define DEVICE_MODEL "SD466NL/A"
#define FW_VERSION "0.0.1"

homekit_characteristic_t name = HOMEKIT_CHARACTERISTIC_(NAME, DEVICE_NAME);
homekit_characteristic_t manufacturer = HOMEKIT_CHARACTERISTIC_(MANUFACTURER, DEVICE_MANUFACTURER);
homekit_characteristic_t serial = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, DEVICE_SERIAL);
homekit_characteristic_t model = HOMEKIT_CHARACTERISTIC_(MODEL, DEVICE_MODEL);
homekit_characteristic_t revision = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION, FW_VERSION);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
homekit_accessory_t *accessories[] = {
        HOMEKIT_ACCESSORY(.id = 1, .category = homekit_accessory_category_garage_door_openers, .services = (homekit_service_t*[]) {
                HOMEKIT_SERVICE(
                        ACCESSORY_INFORMATION,
                        .characteristics = (homekit_characteristic_t*[]) {
                        &name,
                        &manufacturer,
                        &serial,
                        &model,
                        &revision,
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, accessory_identify),
                        NULL
                },
                        ),
                HOMEKIT_SERVICE(
                        GARAGE_DOOR_OPENER,
                        .primary = true,
                        .characteristics = (homekit_characteristic_t*[]) {
                        HOMEKIT_CHARACTERISTIC(NAME, "Калитка"),
                        &garage_door_current_state,
                        &garage_door_target_state,
                        &garage_door_obstruction_detected,
                        NULL
                },
                        ),
                HOMEKIT_SERVICE(
                        LIGHTBULB,
                        .primary = false,
                        .characteristics = (homekit_characteristic_t*[]) {
                        HOMEKIT_CHARACTERISTIC(NAME, "Свет"),
                        &lightbulb_on_state,
                        NULL
                },
                        ),
                NULL
        }),
        NULL
};
#pragma GCC diagnostic pop

homekit_server_config_t config = {
        .accessories = accessories,
        .password = CONFIG_ESP_SETUP_CODE,
        .setupId = CONFIG_ESP_SETUP_ID,
};

void on_wifi_ready() {
        homekit_server_init(&config);
}

void app_main(void) {
        esp_err_t ret = nvs_flash_init();
        if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
                ESP_ERROR_CHECK(nvs_flash_erase());
                ret = nvs_flash_init();
        }
        ESP_ERROR_CHECK(ret);

        wifi_init();
        gpio_init();
}
