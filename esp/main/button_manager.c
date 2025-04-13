#include "button_manager.h"

static const char *TAG = "button_interrupt";

static QueueHandle_t gpio_evt_queue = NULL;
static TickType_t press_start_tick = 0; 
static TickType_t last_isr_tick = 0;
static TimerHandle_t long_press_timer = NULL;
static bool long_press_detected = false;

// Interrupt service routine (ISR)
static void IRAM_ATTR gpio_isr_handler(void *arg) {
    uint32_t gpio_num = (uint32_t)arg;
    TickType_t now_tick = xTaskGetTickCountFromISR();

    // Debounce: Ignore interrupts within X ms
    if (now_tick - last_isr_tick > BUTTON_DEBOUNCE_TICK) {
        last_isr_tick = now_tick;
        xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
    }
}

// Timer callback to check for long press
static void long_press_timer_callback(TimerHandle_t xTimer) {
    TickType_t now_tick = xTaskGetTickCount();
    TickType_t press_duration = now_tick - press_start_tick;

    if (long_press_detected) {
        return;
    }

    if (press_duration >= BUTTON_LONG_PRESS_TICK && gpio_get_level(BUTTON_GPIO) == 0) {
        ESP_LOGD(TAG, "Long press detected in advance on GPIO %d", BUTTON_GPIO);
        long_press_detected = true;

        // status_led_set(LED_THREE_BLINK);
        // Handle long press action here
        events_post(EVENT_BUTTON_LONG_PRESS, NULL, 0);  // Post long press event
    }
}

// Task to handle button press events
static void button_task(void *arg) {
    uint32_t io_num;
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // Check the button state
            if (gpio_get_level(io_num) == BUTTON_GPIO) {
                press_start_tick = xTaskGetTickCount();
                long_press_detected = false;

                // Start the long press timer
                xTimerStart(long_press_timer, 0);
            } else {
                int64_t press_duration = xTaskGetTickCount() - press_start_tick;

                xTimerStop(long_press_timer, 0);

                if (!long_press_detected) {  // If long press wasn't already detected
                    if (press_duration >= BUTTON_LONG_PRESS_TICK) {  // Long press
                        ESP_LOGD(TAG, "Long press detected on GPIO %ld", io_num);
                        events_post(EVENT_BUTTON_LONG_PRESS, NULL, 0);  // Post long press event
                    } else if (press_duration >= BUTTON_DEBOUNCE_TICK) {  // Short press
                        ESP_LOGD(TAG, "Short press detected on GPIO %ld", io_num);
                        events_post(EVENT_BUTTON_SHORT_PRESS, NULL, 0);  // Post short press event
                    }
                }
            }
        }
    }
}

void button_init(void) {
    // Configure the button GPIO as input with pull-up
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,  // Trigger on falling edge (button press)
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    gpio_config(&io_conf);

    // Create a queue to handle GPIO events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Create the long press timer
    long_press_timer = xTimerCreate("LongPressTimer", pdMS_TO_TICKS(100), pdTRUE, NULL, long_press_timer_callback);

    // Start the button task
    xTaskCreatePinnedToCore(button_task, "button_task", TASK_BUTTON_STACK_SIZE, NULL, TASK_BUTTON_PRIORITY, NULL, TASK_BUTTON_CORE);

    // Install the ISR service
    gpio_install_isr_service(ESP_INTR_FLAG_LEVEL3);

    // Attach the interrupt handler
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);

    ESP_LOGI(TAG, "Button interrupt initialized on GPIO %d", BUTTON_GPIO);
}