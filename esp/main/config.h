#ifndef CONFIG_H
#define CONFIG_H

#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64

#define AP_SSID_KEY "as"
#define AP_PASS_KEY "ap"
#define AP_CHANNEL_KEY "ac"
#define STA_SSID_KEY "ss"
#define STA_PASS_KEY "sp"


// Button config
#define BUTTON_GPIO GPIO_NUM_0  // IO0 button
#define BUTTON_DEBOUNCE_TICK pdMS_TO_TICKS(90)
#define BUTTON_LONG_PRESS_TICK pdMS_TO_TICKS(3000)

// Task configurations
#define EVENT_LOOP_QUEUE_SIZE      10
#define EVENT_LOOP_TASK_STACK_SIZE 2048
#define EVENT_LOOP_TASK_PRIORITY   20
#define EVENT_LOOP_TASK_CORE       0

#define TASK_STATUS_LED_STACK_SIZE 2048
#define TASK_STATUS_LED_PRIORITY   8
#define TASK_STATUS_LED_CORE       tskNO_AFFINITY

#define TASK_BUTTON_STACK_SIZE     2048
#define TASK_BUTTON_PRIORITY       10
#define TASK_BUTTON_CORE           tskNO_AFFINITY

#define TASK_NTC_TEMP_STACK_SIZE   4096
#define TASK_NTC_TEMP_PRIORITY     5
#define TASK_NTC_TEMP_CORE         1

#define TASK_NTC_REPORT_STACK_SIZE 2048
#define TASK_NTC_REPORT_PRIORITY   3
#define TASK_NTC_REPORT_CORE       1

#define TASK_APP_STACK_SIZE        2048
#define TASK_APP_PRIORITY          18
#define TASK_APP_CORE              0

#endif // CONFIG_H