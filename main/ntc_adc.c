#include "ntc_adc.h"
#include "esp_log.h"

static const char *TAG = "ntc_adc";

// Static variables for ADC handle and mutex
static adc_continuous_handle_t adc_handle;
static SemaphoreHandle_t channel_data_mutex;

// Array to store ADC channel data
static uint16_t channel_data[SENSOR_MAX_COUNT] = {0};

// Retrieve ADC data for a specific channel
uint16_t ntc_get_channel_data(uint8_t channel_index)
{
    if (channel_index >= SENSOR_MAX_COUNT)
    {
        return 0; // Invalid channel index
    }

    if (xSemaphoreTake(channel_data_mutex, portMAX_DELAY))
    {
        uint16_t sample = channel_data[channel_index];
        xSemaphoreGive(channel_data_mutex);
        return sample;
    }
    return 0; // Failed to take mutex
}

// Convert raw ADC value to temperature in Celsius
float ntc_adc_raw_to_temperature(uint16_t adc_raw)
{
    // Convert ADC value to voltage (12-bit ADC, 0 dB attenuation = 0–1.1V range)
    float voltage_mv = (adc_raw * 1100) / 4095.0; // Convert to mV

    // Calculate NTC resistance
    float R_ntc = R_FIXED * (V_SUPPLY / voltage_mv - 1.0);

    // Convert resistance to temperature using Steinhart-Hart equation
    float t_kelvin = 1.0 / ((1.0 / T0_KELVIN) + (1.0 / NTC_BETA) * log(R_ntc / NTC_R25));
    float temperature = t_kelvin - 273.15; // Convert to Celsius

    return temperature;
}

// Initialize the mutex for thread safety
void ntc_init_mutex()
{
    channel_data_mutex = xSemaphoreCreateMutex();
    if (channel_data_mutex == NULL)
    {
        printf("Failed to create mutex\n");
        abort();
    }
}

// Initialize the ADC
esp_err_t ntc_adc_initialize()
{
    if (channel_data_mutex == NULL)
    {
        ntc_init_mutex(); // Initialize mutex if not already done
    }

    // ADC configuration
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,
        .conv_frame_size = 256,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &adc_handle));

    // Configure channels
    adc_continuous_config_t channel_config = {
        .sample_freq_hz = SOC_ADC_SAMPLE_FREQ_THRES_LOW, // Sampling frequency
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };

    int pI = 0;
    adc_digi_pattern_config_t patterns[8] = {0};
    for (int i = 0; i < 8; i++)
    {
        if (system_state.sensor_mask & (1 << i)) // & LCD_SENSOR_DISPLAY_MASK
        {
            ESP_LOGI(TAG, "Initializing ADC channel %d - Pattern: %d", i, pI);
            // Add channel to the configuration
            patterns[pI].atten = ADC_ATTEN_DB_0;
            patterns[pI].channel = i;
            patterns[pI].unit = ADC_UNIT_1;
            patterns[pI].bit_width = ADC_BITWIDTH_12;
            pI++;
        }
    }
    channel_config.pattern_num = pI;
    channel_config.adc_pattern = patterns;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &channel_config));

    // Create temperature reading task
    xTaskCreatePinnedToCore(ntc_temperature_task, "temperature_task", TASK_NTC_TEMP_STACK_SIZE, NULL, TASK_NTC_TEMP_PRIORITY, NULL, TASK_NTC_TEMP_CORE);
    //xTaskCreatePinnedToCore(ntc_report_temperature_task, "report_temperature_task", TASK_NTC_REPORT_STACK_SIZE, NULL, TASK_NTC_REPORT_PRIORITY, NULL, TASK_NTC_REPORT_CORE);

    return ESP_OK;
}

// Start ADC in continuous mode
void ntc_adc_start()
{
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
}

// Stop ADC in continuous mode
void ntc_adc_stop()
{
    ESP_ERROR_CHECK(adc_continuous_stop(adc_handle));
}

// Process ADC data and update channel data
void ntc_adc_process_data()
{
    uint8_t buffer[256];
    adc_digi_output_data_t *data;

    while (1)
    {
        uint32_t read_size = 0;
        esp_err_t ret = adc_continuous_read(adc_handle, buffer, sizeof(buffer), &read_size, pdMS_TO_TICKS(1000));
        if (ret == ESP_OK)
        {
            for (int i = 0; i < read_size; i += sizeof(adc_digi_output_data_t))
            {
                data = (adc_digi_output_data_t *)&buffer[i];
                if (data->type1.channel >= SENSOR_MAX_COUNT)
                {
                    continue; // Skip invalid channels
                }
                
                if (xSemaphoreTake(channel_data_mutex, portMAX_DELAY))
                {
                    channel_data[data->type1.channel] = data->type1.data;
                    xSemaphoreGive(channel_data_mutex);
                }
            }
        }
    }
}

// Task to start ADC and process data
void ntc_temperature_task(void *pvParameter)
{
    ntc_adc_start();
    ntc_adc_process_data();
}

// Task to report temperature data to stdout
void ntc_report_temperature_task(void *pvParameter)
{
    while (1)
    {
        float temp = ntc_adc_raw_to_temperature(ntc_get_channel_data(1));
        printf("%.2f\n", temp);
        vTaskDelay(pdMS_TO_TICKS(100)); // Report every second
    }
}