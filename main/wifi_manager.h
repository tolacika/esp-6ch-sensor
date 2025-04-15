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

#define WIFI_AP_SSID_KEY "ap_ssid"
#define WIFI_AP_PASS_KEY "ap_pass"
#define WIFI_AP_MAX_CONN 4
#define WIFI_AP_CHANNEL 1

void wifi_initialize(void);
void wifi_connect(void);
void wifi_switch_mode(void);

#endif // WIFI_MANAGER_H
