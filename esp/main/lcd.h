#ifndef LCD_H
#define LCD_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "state_manager.h"
#include "config.h"

// I2C configuration
#define I2C_MASTER_NUM        I2C_NUM_0
#define I2C_MASTER_SDA_IO     14
#define I2C_MASTER_SCL_IO     15
#define I2C_MASTER_FREQ_HZ    100000
#define LCD_I2C_ADDRESS       0x27    // Adjust to your PCF8574 address

// LCD commands
#define WRITE_BIT           I2C_MASTER_WRITE

#define LCD_BACKLIGHT (1 << 3) // Backlight bit
#define LCD_ENABLE (1 << 2)   // Enable bit
#define LCD_ENABLE_OFF (0 << 2) // Enable off
#define LCD_RW (1 << 1)      // Read/Write bit
#define LCD_RW_WRITE (0 << 1) // Write mode
#define LCD_RW_READ (1 << 1)  // Read mode
#define LCD_RS (1 << 0)      // Register Select bit
#define LCD_RS_CMD (0 << 0)    // Command mode
#define LCD_RS_DATA (1 << 0)   // Data mode
#define LCD_DB7 (1 << 7) // Data bit 7
#define LCD_DB6 (1 << 6) // Data bit 6
#define LCD_DB5 (1 << 5) // Data bit 5
#define LCD_DB4 (1 << 4) // Data bit 4

#define LCD_FPS 2 // Frames per second
#define LCD_COLS 20
#define LCD_ROWS 4
#define LCD_ROW_OFFSET {0x00, 0x40, 0x14, 0x54} // Row offsets for 20x4 LCD
#define LCD_BUFFER_SIZE (LCD_COLS * LCD_ROWS)

typedef enum {
    LCD_SCREEN_SPLASH = 0,
    LCD_SCREEN_AP_MODE,
    LCD_SCREEN_TEMP_AND_STATUS,
    LCD_SCREEN_TEMP_AND_AVG,
    LCD_SCREEN_STATUS_1,
    LCD_SCREEN_STATUS_2,
    LCD_SCREEN_STATUS_3,
    LCD_SCREEN_MAX
} lcd_screen_state_t;

// Initialize the I2C master.
void i2c_initialize(void);

// Initialize the LCD.
void lcd_initialize(void);

// Set the cursor position on the LCD.
void lcd_set_cursor_position(uint8_t col, uint8_t row);

// Update the cursor position in the buffer.
void lcd_set_cursor(uint8_t col, uint8_t row);

// Write a single character to the buffer.
void lcd_write_character(char c);

// Write a string to the buffer.
void lcd_write_text(const char *str);

// Write a buffer to the LCD.
void lcd_write_buffer(const char *buffer, size_t size);

// Copy data to the LCD buffer at a specific position.
void lcd_copy_to_buffer(const char *data, size_t size, int8_t col, int8_t row);

// Clear the LCD buffer.
void lcd_clear_buffer(void);

// Render the buffer content to the LCD.
void lcd_render(void);

// Control the LCD backlight.
void lcd_toggle_backlight(bool state);

// Format temperature as a string.
void lcd_format_temperature(float temp, char *buffer, size_t buffer_size);

// Render the LCD cycle.
void lcd_render_cycle(void);

// Display an array of temperatures on the LCD.
void lcd_temperaure_screen(bool bottom_statistics);

// Display the status line on the LCD.
void lcd_status_line(void);

// Display a list of status messages on the LCD.
void lcd_status_screen(int8_t index);

// Display the splash screen on the LCD.
void lcd_splash_screen(void);

// Display the access point mode screen on the LCD.
void lcd_ap_mode_screen(void);

// Get the current screen state.
lcd_screen_state_t lcd_get_screen_state(void);

// Go to the next screen.
void lcd_next_screen(void);

// Set the current screen state.
void lcd_set_screen_state(lcd_screen_state_t state);

// Periodically update the LCD with temperature data.
void lcd_update_task(void *pvParameter);

#endif // LCD_H