#ifndef NTC_ADC_H
#define NTC_ADC_H

#include <stdio.h>
#include <math.h>
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h" // Include for GPIO functionality
#include "config.h"
#include "state_manager.h"

// Constants for NTC thermistor calculations
#define R_FIXED 1000.0             // 1kΩ fixed resistor
#define V_SUPPLY 3300.0            // Supply voltage in mV
#define NTC_BETA 3950.0            // Beta value for NTC thermistor
#define NTC_R25 100000.0           // Resistance at 25°C in ohms
#define T0_KELVIN 298.15           // 25°C in Kelvin

/**
 * @brief Initialize the ADC for continuous sampling.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t ntc_adc_initialize();

/**
 * @brief Initialize the mutex for thread safety.
 */
void ntc_init_mutex();

/**
 * @brief Start the ADC in continuous mode.
 */
void ntc_adc_start();

/**
 * @brief Stop the ADC in continuous mode.
 */
void ntc_adc_stop();

/**
 * @brief Process ADC data and update channel data.
 */
void ntc_adc_process_data();

/**
 * @brief Retrieve the ADC data for a specific channel.
 * @param channel_index Index of the channel (0-5).
 * @return ADC data for the channel, or -1 on error.
 */
uint16_t ntc_get_channel_data(uint8_t channel_index);

/**
 * @brief Convert raw ADC value to temperature in Celsius.
 * @param adc_raw Raw ADC value.
 * @return Temperature in Celsius.
 */
float ntc_adc_raw_to_temperature(uint16_t adc_raw);

/**
 * @brief Task to start ADC and process temperature data.
 * @param pvParameter Task parameter (unused).
 */
void ntc_temperature_task(void *pvParameter);

/**
 * @brief Task to report temperature data to stdout.
 * @param pvParameter Task parameter (unused).
 */
void ntc_report_temperature_task(void *pvParameter);


#endif // NTC_ADC_H