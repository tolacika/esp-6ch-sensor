#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "config.h"
#include "state_manager.h"

void button_init(void);

#endif // BUTTON_MANAGER_H