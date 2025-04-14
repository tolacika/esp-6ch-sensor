#include "status_led.h"
#include "esp_log.h"

static const int16_t state_bit_masks[LED_MAX_STATES] = {
    [LED_OK] = 0xFFFF,
    [LED_FAST_BLINK] = 0xAAAA,
    [LED_SLOW_BLINK] = 0xCCCC,
    [LED_OFF] = 0x0000,
    [LED_ERROR] = 0xCC00,
    [LED_THREE_BLINK] = 0xA800,
};

static led_state_t current_led_state = LED_OFF;
static int8_t led_iterator = 0;

void status_led_init(void)
{
    gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT); // Set GPIO pin as output
    current_led_state = LED_OFF;                      // Initialize to OFF state
    led_iterator = 0;                                 // Initialize iterator
    gpio_set_level(GPIO_NUM_2, 0);                    // Ensure LED is off initially
    // Create the task
    xTaskCreatePinnedToCore(status_led_task, "status_led_task", TASK_STATUS_LED_STACK_SIZE, NULL, TASK_STATUS_LED_PRIORITY, NULL, TASK_STATUS_LED_CORE);
    // Subscribe to events
    events_subscribe(EVENT_WIFI_STATE_CHANGED, status_led_event_handler, NULL);
    events_subscribe(EVENT_BUTTON_SHORT_PRESS, status_led_event_handler, NULL);
    events_subscribe(EVENT_BUTTON_LONG_PRESS, status_led_event_handler, NULL);

    current_led_state = LED_OK; // Set initial state to OK
}

void status_led_set(led_state_t led_state)
{
    if (led_state >= LED_MAX_STATES)
    {
        led_state = LED_OFF; // Default to OFF if invalid state
    }
    current_led_state = led_state;
}

void status_led_task(void *pvParameter)
{
    while (1)
    {
        gpio_set_level(GPIO_NUM_2, (state_bit_masks[current_led_state] >> led_iterator) & 0x01);
        led_iterator = (led_iterator + 1) % 16; // Cycle through the bits
        vTaskDelay(pdMS_TO_TICKS(1000 / 16));   // Adjust delay as needed
    }
}

void status_led_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    ESP_LOGI("status_led", "Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());

    switch (id)
    {
    case EVENT_WIFI_STATE_CHANGED:
        if (system_state.wifi_sta_connection_state == 0)
        {
            status_led_set(LED_OK); // Set to OK state when connected
        }
        else
        {
            status_led_set(LED_ERROR); // Set to ERROR state when disconnected
        }
        break;
    case EVENT_BUTTON_SHORT_PRESS:
        // Toggle between SLOW and FAST blink on short press
        if (current_led_state == LED_SLOW_BLINK)
        {
            status_led_set(LED_FAST_BLINK);
        }
        else
        {
            status_led_set(LED_SLOW_BLINK);
        }
        break;
    case EVENT_BUTTON_LONG_PRESS:
        status_led_set(LED_THREE_BLINK); // Set to three blink on long press
        break;
    default:
        break;
    }
}