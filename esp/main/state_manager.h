#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "nvs_flash.h"
#include "nvs_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "config.h"
#include "state_manager.h"
#include "esp_netif_ip_addr.h"

#define SYSTEM_STATE_SET(field, value) \
    system_state_set(&system_state.field, &value, sizeof(value))

#define SYSTEM_STATE_GET(field, output) \
    system_state_get(&system_state.field, &output, sizeof(output))

typedef enum {
    WIFI_STATE_NONE = 0,
    WIFI_STATE_STA,
    WIFI_STATE_AP,
    WIFI_STATE_TRANSITION
} wifi_state_t;

typedef struct
{
    uint8_t wifi_sta_connection_state; // see wifi_err_reason_t
    uint8_t wifi_ap_connection_state;  // see wifi_err_reason_t
    esp_ip4_addr_t wifi_current_ip;       // current IP address
    wifi_state_t wifi_state;
    char ap_ssid[SSID_MAX_LEN];
    char ap_pass[PASS_MAX_LEN];
    int32_t ap_channel;
    char sta_ssid[SSID_MAX_LEN];
    char sta_pass[PASS_MAX_LEN];
    int8_t sensor_mask;
} system_state_t;

// Declare an event base
ESP_EVENT_DECLARE_BASE(CUSTOM_EVENTS);        // declaration of the timer events family

// Event types
enum {
    EVENT_WIFI_STATE_CHANGED,           // Event for WiFi connection established
    //EVENT_WIFI_DISCONNECTED,            // Event for WiFi disconnection
    EVENT_BUTTON_LONG_PRESS,            // Event for button long press
    EVENT_BUTTON_SHORT_PRESS,           // Event for button short press
    EVENT_RESTART_REQUESTED,            // Event for restart requested
};

extern system_state_t system_state;

void test_system_state(void);

void log_system_state(void);

void dump_string(const char *buffer, size_t length);

void system_initialize(void);

void nvs_initialize();

void system_state_set(void *field_ptr, const void *value, size_t size);

void system_state_get(const void *field_ptr, void *output, size_t size);

void store_running_config();

void read_running_config();

// Function prototypes
void events_init(void);

void events_post(int32_t event_id, const void* event_data, size_t event_data_size);

void events_subscribe(int32_t event_id, esp_event_handler_t event_handler, void* event_handler_arg);

#endif // STATE_MANAGER_H