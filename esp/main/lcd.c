#include "lcd.h"
#include "ntc_adc.h"
#include <string.h>
#include "esp_netif.h"

static const char *TAG = "I2C_LCD";
static const uint8_t COMMAND_8BIT_MODE = 0b00110000;
static const uint8_t COMMAND_4BIT_MODE = 0b00100000;
static const uint8_t INIT_COMMANDS[] = {
    0b00101000, // Function set: 4-bit mode, 2 lines, 5x8 dots
    0b00001100, // Display control: display on, cursor off, blink off
    0b00000001, // Clear display
    0b00000110, // Entry mode set: increment cursor, no shift
    0b00000010, // Set cursor to home position
    0b10000000  // Set cursor to first line
};

static i2c_master_dev_handle_t i2c_device_handle = NULL;
static i2c_master_bus_handle_t i2c_bus_handle = NULL;
static uint8_t lcd_backlight_status = LCD_BACKLIGHT;

static char lcd_buffer[LCD_BUFFER_SIZE]; // 80-byte buffer for the LCD
static char status_line_buffer[LCD_COLS];

static uint8_t cursor_col = 0;
static uint8_t cursor_row = 0;

static lcd_screen_state_t lcd_screen_state = LCD_SCREEN_SPLASH;

static esp_err_t i2c_send_with_toggle(uint8_t data)
{
    // Helper function to toggle the enable bit
    uint8_t data_with_enable = data | LCD_ENABLE;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_handle, &data_with_enable, 1, -1));
    vTaskDelay(pdMS_TO_TICKS(1));

    data_with_enable &= ~LCD_ENABLE;
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_handle, &data_with_enable, 1, -1));
    vTaskDelay(pdMS_TO_TICKS(1));

    return ESP_OK;
}

static esp_err_t i2c_send_4bit_data(uint8_t data, uint8_t rs)
{
    // Send a byte of data to the LCD in 4-bit mode
    uint8_t nibbles[2] = {
        (data & 0xF0) | rs | lcd_backlight_status | LCD_RW_WRITE,
        ((data << 4) & 0xF0) | rs | lcd_backlight_status | LCD_RW_WRITE};
    ESP_ERROR_CHECK(i2c_send_with_toggle(nibbles[0]));
    ESP_ERROR_CHECK(i2c_send_with_toggle(nibbles[1]));

    return ESP_OK;
}

static void handle_wifi_state_change()
{
    if (system_state.wifi_state == WIFI_STATE_TRANSITION)
    {
        memcpy(status_line_buffer, "Wifi: Connecting", 16);
    }
    else if (system_state.wifi_state == WIFI_STATE_AP)
    {
        memcpy(status_line_buffer, "Wifi: AP mode", 13);
    }
    else if (system_state.wifi_state == WIFI_STATE_STA)
    {
        switch (system_state.wifi_sta_connection_state)
        {
        case 0:
            esp_ip4_addr_t *ip_addr = &system_state.wifi_current_ip;
            sprintf(status_line_buffer, "IP: " IPSTR, IP2STR(ip_addr));
            ESP_LOGI(TAG, "WiFi connected, IP: " IPSTR, IP2STR(ip_addr));
            break;
        case 201: // WIFI_REASON_NO_AP_FOUND
            memcpy(status_line_buffer, "Wifi: Invalid SSID", 18);
            break;
        case 202: // WIFI_REASON_AUTH_FAIL
            memcpy(status_line_buffer, "Wifi: Auth failed", 17);
            break;
        default:
            char reason_buffer[4];
            memcpy(status_line_buffer, "Wifi: disconn.", 14);
            sprintf(reason_buffer, "%3d", system_state.wifi_sta_connection_state);
            memcpy(status_line_buffer + 15, reason_buffer, 3);
            break;
        }
    }
    else if (system_state.wifi_state == WIFI_STATE_NONE)
    {
        memcpy(status_line_buffer, "Wifi: No connection", 19);
    }
}

static void lcd_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    if (base == CUSTOM_EVENTS)
    {
        switch (id)
        {
        case EVENT_BUTTON_SHORT_PRESS:
            ESP_LOGI(TAG, "Short button press detected");
            if (lcd_screen_state != LCD_SCREEN_AP_MODE)
            {
                lcd_next_screen(); // Cycle to the next screen
            }
            break;
        case EVENT_BUTTON_LONG_PRESS:
            ESP_LOGI(TAG, "Long button press detected");
            lcd_screen_state = LCD_SCREEN_AP_MODE; // Set screen state to AP mode
            break;
        case EVENT_WIFI_STATE_CHANGED:
            handle_wifi_state_change(); // Handle WiFi state change
            break;
        default:
            break;
        }
    }
}

void i2c_initialize(void)
{
    // Initialize the I2C master
    i2c_master_bus_config_t i2c_bus_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_MASTER_NUM,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true};
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle));
    ESP_LOGI(TAG, "I2C bus initialized");

    i2c_device_config_t i2c_device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = LCD_I2C_ADDRESS,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ};
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus_handle, &i2c_device_config, &i2c_device_handle));
    ESP_LOGI(TAG, "I2C device added");
    vTaskDelay(pdMS_TO_TICKS(50)); // Wait for LCD to power up
    ESP_LOGI(TAG, "I2C device initialized");
}

static void lcd_init_cycle(void)
{
    // Initialize the LCD
    ESP_ERROR_CHECK(i2c_send_with_toggle(lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));
    ESP_ERROR_CHECK(i2c_send_with_toggle(COMMAND_8BIT_MODE | lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));
    ESP_ERROR_CHECK(i2c_send_with_toggle(COMMAND_8BIT_MODE | lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));
    ESP_ERROR_CHECK(i2c_send_with_toggle(COMMAND_8BIT_MODE | lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));
    ESP_ERROR_CHECK(i2c_send_with_toggle(COMMAND_4BIT_MODE | lcd_backlight_status | LCD_ENABLE_OFF | LCD_RW_WRITE | LCD_RS_CMD));

    for (uint8_t i = 0; i < sizeof(INIT_COMMANDS); i++)
    {
        ESP_ERROR_CHECK(i2c_send_4bit_data(INIT_COMMANDS[i], LCD_RS_CMD));
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    lcd_toggle_backlight(true);
    lcd_clear_buffer();
}

void lcd_initialize(void)
{
    memset(status_line_buffer, ' ', LCD_COLS);
    memcpy(status_line_buffer, "Wifi: Init...", 13);

    lcd_init_cycle(); // Initialize the LCD

    lcd_render();
    lcd_render_cycle();

    events_subscribe(EVENT_BUTTON_SHORT_PRESS, lcd_event_handler, NULL); // Subscribe to button short press event
    events_subscribe(EVENT_BUTTON_LONG_PRESS, lcd_event_handler, NULL);  // Subscribe to button long press event
    events_subscribe(EVENT_WIFI_STATE_CHANGED, lcd_event_handler, NULL); // Subscribe to WiFi connected event

    // Create LCD update task
    xTaskCreatePinnedToCore(lcd_update_task, "lcd_update_task", 4096, NULL, 5, NULL, 0);
}

void lcd_set_screen_state(lcd_screen_state_t state)
{
    // Set the current screen state
    if (state < LCD_SCREEN_MAX)
    {
        lcd_screen_state = state;
    }
    else
    {
        lcd_screen_state = LCD_SCREEN_TEMP_AND_STATUS; // Default to temperature and average screen
    }
    lcd_clear_buffer(); // Clear the buffer for the new screen
    lcd_render_cycle(); // Render the new screen
}

void lcd_next_screen(void)
{
    // Cycle through the screens
    if (lcd_screen_state >= LCD_SCREEN_TEMP_AND_STATUS)
    {
        lcd_screen_state++;
    }
    if (lcd_screen_state >= LCD_SCREEN_MAX)
    {
        lcd_screen_state = LCD_SCREEN_TEMP_AND_STATUS; // Loop back to the first screen
    }
    lcd_clear_buffer(); // Clear the buffer for the new screen
    lcd_render_cycle(); // Render the new screen
}

lcd_screen_state_t lcd_get_screen_state(void)
{
    // Get the current screen state
    return lcd_screen_state;
}

void lcd_set_cursor_position(uint8_t col, uint8_t row)
{
    // Set the cursor position on the LCD
    if (col >= LCD_COLS)
        col = LCD_COLS - 1;
    if (row >= LCD_ROWS)
        row = LCD_ROWS - 1;

    static const uint8_t row_offsets[] = LCD_ROW_OFFSET;
    uint8_t data = 0x80 | (col + row_offsets[row]);
    ESP_ERROR_CHECK(i2c_send_4bit_data(data, LCD_RS_CMD));
}

void lcd_set_cursor(uint8_t col, uint8_t row)
{
    // Update the cursor position in the buffer
    if (col < LCD_COLS && row < LCD_ROWS)
    {
        cursor_col = col;
        cursor_row = row;
    }
}

void lcd_write_character(char c)
{
    // Write a single character to the buffer
    if (cursor_col < LCD_COLS && cursor_row < LCD_ROWS)
    {
        lcd_buffer[cursor_row * LCD_COLS + cursor_col] = c;
        cursor_col++;
        if (cursor_col >= LCD_COLS)
        {
            cursor_col = 0;
            cursor_row = (cursor_row + 1) % LCD_ROWS;
        }
    }
}

void lcd_write_text(const char *str)
{
    // Write a string to the buffer
    while (*str)
    {
        lcd_write_character(*str);
        str++;
    }
}

void lcd_write_buffer(const char *buffer, size_t size)
{
    // Write a buffer to the LCD
    for (size_t i = 0; i < size; i++)
    {
        lcd_write_character(buffer[i]);
    }
}

void lcd_copy_to_buffer(const char *data, size_t size, int8_t col, int8_t row)
{
    // Copy data to the LCD buffer at a specific position
    if (col < 0 || col >= LCD_COLS || row < 0 || row >= LCD_ROWS)
    {
        return;
    }
    if (size > LCD_COLS - col)
    {
        size = LCD_COLS - col;
    }

    while (size-- && *data)
    {
        lcd_buffer[row * LCD_COLS + col] = *data++;
        col++;
    }
    lcd_set_cursor(col, row); // Update the cursor position
}

void lcd_clear_buffer(void)
{
    memset(lcd_buffer, ' ', LCD_BUFFER_SIZE);
    lcd_set_cursor(0, 0);
}

void lcd_render(void)
{
    // Render the buffer content to the LCD
    for (uint8_t row = 0; row < LCD_ROWS; row++)
    {
        lcd_set_cursor_position(0, row);
        for (uint8_t col = 0; col < LCD_COLS; col++)
        {
            ESP_ERROR_CHECK(i2c_send_4bit_data(lcd_buffer[row * LCD_COLS + col], LCD_RS_DATA));
        }
    }
}

void lcd_toggle_backlight(bool state)
{
    // Control the LCD backlight
    if (state)
    {
        lcd_backlight_status |= LCD_BACKLIGHT;
    }
    else
    {
        lcd_backlight_status &= ~LCD_BACKLIGHT;
    }
    ESP_ERROR_CHECK(i2c_master_transmit(i2c_device_handle, &lcd_backlight_status, 1, -1));
}

void lcd_format_temperature(float temp, char *buffer, size_t buffer_size)
{
    // Format temperature as a string
    if (buffer_size < 5)
    {
        return; // Buffer too small
    }
    if (temp < 0)
    {
        buffer[0] = '-';
        temp = -temp;
    }
    else
    {
        buffer[0] = temp >= 100 ? ((int)(temp / 100) % 10) + '0' : ' ';
    }
    buffer[1] = temp < 10 ? ' ' : (int)(temp / 10) % 10 + '0';
    buffer[2] = (int)temp % 10 + '0';
    buffer[3] = '.';
    buffer[4] = (int)(temp * 10) % 10 + '0';
}

static bool isRendering = false;

void lcd_render_cycle()
{
    if (isRendering)
    {
        return; // Prevent re-entrance
    }
    isRendering = true; // Set rendering flag
    switch (lcd_screen_state)
    {
    case LCD_SCREEN_SPLASH:
        lcd_splash_screen();
        break;
    case LCD_SCREEN_AP_MODE:
        lcd_ap_mode_screen();
        break;
    case LCD_SCREEN_TEMP_AND_AVG:
        lcd_temperaure_screen(true);
        break;
    case LCD_SCREEN_TEMP_AND_STATUS:
        lcd_temperaure_screen(false);
        lcd_status_line();
        break;
    case LCD_SCREEN_STATUS_1:
    case LCD_SCREEN_STATUS_2:
    case LCD_SCREEN_STATUS_3:
        lcd_status_screen(lcd_screen_state - LCD_SCREEN_STATUS_1);
        break;
    default:
        break;
    }
    lcd_render();        // Render the current screen
    isRendering = false; // Clear rendering flag
}

void lcd_splash_screen(void)
{
    // Display the splash screen on the LCD
    lcd_clear_buffer();
    lcd_set_cursor(0, 0);
    lcd_write_text("   Splash Screen    ");
    lcd_set_cursor(0, 1);
    lcd_write_text("LCD Temperature test");
    lcd_set_cursor(0, 2);
    lcd_write_text("   Splash Screen    ");
    lcd_set_cursor(0, 3);
    lcd_write_text("LCD Test            ");
}

void lcd_ap_mode_screen(void)
{
    char *ap_ssid = system_state.ap_ssid;
    char *ap_pass = system_state.ap_pass;

    // Display the AP mode screen on the LCD
    lcd_clear_buffer();
    lcd_set_cursor(0, 0);
    lcd_write_text(" AP Mode - SSID:");
    lcd_copy_to_buffer(ap_ssid, 20, 0, 1);
    lcd_copy_to_buffer(ap_pass, 20, 0, 2);
    lcd_set_cursor(0, 3);
    lcd_write_text("IP: 192.168.4.1");
}

void lcd_status_screen(int8_t index)
{
    // running_config_t *running_config = get_running_config(); // Get the running configuration
    //  Display the status screen on the LCD
    lcd_clear_buffer();
    lcd_set_cursor(0, 0);
    switch (index)
    {
    case 0:
        lcd_write_text("  Wifi STA: ");
        lcd_set_cursor(0, 1);
        lcd_write_text("SSID:");
        lcd_copy_to_buffer(system_state.sta_ssid, 15, 5, 1);
        lcd_set_cursor(0, 2);
        lcd_write_text("Pass:");
        lcd_copy_to_buffer(system_state.sta_pass, 15, 5, 2);
        lcd_set_cursor(0, 3);
        lcd_write_text("IP:");
        if (system_state.wifi_current_ip.addr == 0)
        {
            lcd_copy_to_buffer("No IP", 5, 3, 3);
        }
        else
        {
            char ip_buffer[18];
            sprintf(ip_buffer, IPSTR, IP2STR(&system_state.wifi_current_ip));
            lcd_copy_to_buffer(ip_buffer, 15, 3, 3);
        }
        break;
    case 1:
        lcd_write_text("  Wifi AP: ");
        lcd_set_cursor(0, 1);
        lcd_write_text("SSID:");
        lcd_copy_to_buffer(system_state.ap_ssid, 15, 5, 1);
        lcd_set_cursor(0, 2);
        lcd_write_text("Pass:");
        lcd_copy_to_buffer(system_state.ap_pass, 15, 5, 2);
        lcd_set_cursor(0, 3);
        lcd_write_text("Status: Disabled");
        break;
    case 2:

        break;
    default:
        break;
    }

    /*EventBits_t bits = xEventGroupGetBits(get_event_group());
    if (bits & WIFI_CONNECTED_BIT) {
        lcd_write_text("Connected");
    } else {
        lcd_write_text("Disconnected");
    }*/
}

void lcd_status_line(void)
{
    // Display the status line on the LCD
    memcpy(lcd_buffer + 3 * LCD_COLS, status_line_buffer, LCD_COLS);
}

void lcd_temperaure_screen(bool bottom_statistics)
{
    // Display temperature data on the LCD
    lcd_clear_buffer();

    char bgBuffer[] = "T1:     C  T4:     C";
    lcd_set_cursor(0, 0);
    lcd_copy_to_buffer(bgBuffer, strlen(bgBuffer), 0, 0);
    bgBuffer[1] = '2';
    bgBuffer[12] = '5';
    lcd_set_cursor(0, 1);
    lcd_copy_to_buffer(bgBuffer, strlen(bgBuffer), 0, 1);
    bgBuffer[1] = '3';
    bgBuffer[12] = '6';
    lcd_set_cursor(0, 2);
    lcd_copy_to_buffer(bgBuffer, strlen(bgBuffer), 0, 2);
    if (bottom_statistics)
    {
        static const char avgBuffer[] = "     C<     C<     C";
        lcd_set_cursor(0, 3);
        lcd_copy_to_buffer(avgBuffer, strlen(avgBuffer), 0, 3);
    }

    char buffer[6] = {0};
    float min_temp = 200.0;
    float max_temp = -20.0;
    float avg_temp = 0.0;

    for (int i = 0; i < 6; i++)
    {
        float temp = ntc_adc_raw_to_temperature(ntc_get_channel_data(i));
        if (bottom_statistics && temp < min_temp)
        {
            min_temp = temp;
        }
        if (bottom_statistics && temp > max_temp)
        {
            max_temp = temp;
        }
        if (bottom_statistics)
        {
            avg_temp += temp;
        }
        lcd_set_cursor(i < 3 ? 3 : 14, i % 3);
        lcd_format_temperature(temp, buffer, sizeof(buffer));
        lcd_write_text(buffer);
    }

    if (!bottom_statistics)
    {
        return; // No statistics to display
    }

    avg_temp /= 6.0;
    lcd_set_cursor(0, 3);
    lcd_format_temperature(min_temp, buffer, sizeof(buffer));
    lcd_write_text(buffer);

    lcd_set_cursor(7, 3);
    lcd_format_temperature(avg_temp, buffer, sizeof(buffer));
    lcd_write_text(buffer);

    lcd_set_cursor(14, 3);
    lcd_format_temperature(max_temp, buffer, sizeof(buffer));
    lcd_write_text(buffer);
}

void lcd_update_task(void *pvParameter)
{
    // Periodically update the LCD
    while (1)
    {
        lcd_render_cycle();
        vTaskDelay(pdMS_TO_TICKS(500)); // Update every second
    }
}