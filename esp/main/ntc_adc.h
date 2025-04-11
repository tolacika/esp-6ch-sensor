#ifndef NTC_ADC_H
#define NTC_ADC_H

#include <stdio.h>
#include <math.h>
#include "esp_adc/adc_continuous.h"
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h" // Include for GPIO functionality

// ADC channel definitions
#define ADC_USED_CHANNEL_1  ADC_CHANNEL_0  // ADC1 Channel 0
#define ADC_USED_CHANNEL_2  ADC_CHANNEL_3  // ADC1 Channel 3
#define ADC_USED_CHANNEL_3  ADC_CHANNEL_6  // ADC1 Channel 6
#define ADC_USED_CHANNEL_4  ADC_CHANNEL_7  // ADC1 Channel 7
#define ADC_USED_CHANNEL_5  ADC_CHANNEL_4  // ADC1 Channel 4
#define ADC_USED_CHANNEL_6  ADC_CHANNEL_5  // ADC1 Channel 5

// Constants for NTC thermistor calculations
#define R_FIXED 10000.0            // 10kΩ fixed resistor
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
int ntc_get_channel_data(int channel_index);

/**
 * @brief Convert raw ADC value to temperature in Celsius.
 * @param adc_raw Raw ADC value.
 * @return Temperature in Celsius.
 */
float ntc_adc_raw_to_temperature(int adc_raw);

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