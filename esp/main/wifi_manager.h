#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "config.h"
#include "nvs_manager.h"
#include "state_manager.h"
#include "esp_log.h"

//#define WIFI_CONNECTED_BIT BIT0
#define WIFI_AP_SSID_KEY "ap_ssid"
#define WIFI_AP_PASS_KEY "ap_pass"
#define WIFI_AP_MAX_CONN 4
#define WIFI_AP_CHANNEL 1

//extern EventGroupHandle_t wifi_event_group;

void wifi_initialize(void);
void wifi_connect_sta(void);
//void wifi_connect_ap(void);
//void wifi_connect_init(void);
//static void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

#endif // WIFI_MANAGER_H
