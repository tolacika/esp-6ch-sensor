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

void app_main(void)
{
    system_initialize();

    events_init();

    wifi_initialize();

    status_led_init();

    button_init();

    ntc_init_mutex();

    i2c_initialize();
    lcd_initialize();

    ntc_adc_initialize();

    lcd_set_screen_state(LCD_SCREEN_TEMP_AND_STATUS);
}
