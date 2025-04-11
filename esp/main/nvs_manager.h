#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "config.h"
#include "esp_log.h"

esp_err_t store_string(const char* key, const char* value);
esp_err_t read_string(const char* key, char* value, size_t *max_len);
esp_err_t store_int(const char* key, int32_t value);
esp_err_t read_int(const char* key, int32_t* value);

#endif