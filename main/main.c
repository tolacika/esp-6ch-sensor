/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include "config.h"
#include "state_manager.h"
#include "nvs_manager.h"
#include "wifi_manager.h"
#include "status_led.h"
#include "button_manager.h"
#include "ntc_adc.h"
#include "lcd.h"
#include "server.h"

/* Todos:
 * - Deinitialize modules on shutdown event
 * - Merge fatfs with nvs_manager, maybe use it instead of nvs for storing config
 * - Update frontend source from CDN.
*/

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(3000));
    ESP_LOGI("main", "Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
    
    system_initialize();

    vTaskDelay(pdMS_TO_TICKS(100));

    events_init();

    vTaskDelay(pdMS_TO_TICKS(100));

    wifi_initialize();

    status_led_init();

    button_init();

    vTaskDelay(pdMS_TO_TICKS(100));

    ntc_init_mutex();

    i2c_initialize();
    lcd_initialize();

    vTaskDelay(pdMS_TO_TICKS(100));

    ntc_adc_initialize();

    vTaskDelay(pdMS_TO_TICKS(3000));

    lcd_set_screen_state(LCD_SCREEN_START_SCREEN);

    wifi_connect();

    vTaskDelay(pdMS_TO_TICKS(100));

    start_http_server();
}
