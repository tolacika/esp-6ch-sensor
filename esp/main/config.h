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
#define BUTTON_DEBOUNCE_TIME_US 90000  // 90ms debounce time
#define BUTTON_LONG_PRESS_TIME_US 3000000  // 3 seconds long press time

#endif // CONFIG_H