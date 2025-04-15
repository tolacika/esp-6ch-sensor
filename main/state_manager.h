#ifndef STATE_MANAGER_H
#define STATE_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <stdio.h>
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
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_http_server.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)

typedef enum {
    WIFI_STATE_NONE = 0,
    WIFI_STATE_STA,
    WIFI_STATE_AP,
} wifi_state_t;

typedef enum {
    WIFI_STARTUP_MODE_NONE = 0,
    WIFI_STARTUP_MODE_STA,
    WIFI_STARTUP_MODE_AP,
} wifi_mode_enum;

typedef struct
{
    uint8_t wifi_sta_connection_state; // see wifi_err_reason_t
    uint8_t wifi_ap_connection_state;  // see wifi_err_reason_t
    esp_ip4_addr_t wifi_current_ip;       // current IP address
    wifi_state_t wifi_state;
    char ap_ssid[SSID_MAX_LEN];
    char ap_pass[PASS_MAX_LEN];
    char sta_ssid[SSID_MAX_LEN];
    char sta_pass[PASS_MAX_LEN];
    int8_t sensor_mask;
    wifi_mode_enum wifi_startup_mode;
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

void system_initialize(void);

void nvs_initialize();

void system_state_set(void *field_ptr, const void *value, size_t size);

void system_state_get(const void *field_ptr, void *output, size_t size);

void store_running_config_in_fatfs();

void store_running_config();

void read_running_config_from_fatfs();

void read_running_config();

void events_init(void);

void events_post(int32_t event_id, const void* event_data, size_t event_data_size);

void events_subscribe(int32_t event_id, esp_event_handler_t event_handler, void* event_handler_arg);

void fatfs_init(void);

esp_err_t send_file_from_fatfs(httpd_req_t *req, const char *file_path, const char *content_type);

#endif // STATE_MANAGER_H