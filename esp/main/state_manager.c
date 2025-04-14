#include "state_manager.h"
#include "esp_log.h"

const char *TAG = "state_manager";
const char *TEST_TAG = "test_system_state";

system_state_t system_state = {
    .wifi_sta_connection_state = 0,
    .wifi_ap_connection_state = 0,
    .wifi_state = WIFI_STATE_NONE,
    .ap_ssid = {0},
    .ap_pass = {0},
    .ap_channel = 0,
    .sta_ssid = {0},
    .sta_pass = {0},
    .sensor_mask = 0,
    .wifi_startup_mode = WIFI_STARTUP_MODE_NONE,
};

static esp_event_loop_handle_t custom_event_loop = NULL; // Custom event loop handle
ESP_EVENT_DEFINE_BASE(CUSTOM_EVENTS);                    // Define the event base for custom events

void log_system_state(void)
{
    ESP_LOGI(TEST_TAG, "Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());

    system_state_t *state = malloc(sizeof(system_state_t));
    if (state == NULL)
    {
        ESP_LOGE(TEST_TAG, "Failed to allocate memory for system_state_t");
        return;
    }
    memcpy(state, &system_state, sizeof(system_state_t));
    ESP_LOGI(TEST_TAG, "wifi_sta_connection_state: %d", state->wifi_sta_connection_state);
    ESP_LOGI(TEST_TAG, "wifi_ap_connection_state: %d", state->wifi_ap_connection_state);
    // ESP_LOGI(TEST_TAG, "wifi_current_ip: %d.%d.%d.%d", IP2STR(state->wifi_current_ip));
    ESP_LOGI(TEST_TAG, "wifi_state: %d", state->wifi_state);
    ESP_LOGI(TEST_TAG, "ap_ssid: %s", state->ap_ssid);
    ESP_LOGI(TEST_TAG, "ap_pass: %s", state->ap_pass);
    ESP_LOGI(TEST_TAG, "ap_channel: %ld", state->ap_channel);
    ESP_LOGI(TEST_TAG, "sta_ssid: %s", state->sta_ssid);
    ESP_LOGI(TEST_TAG, "sta_pass: %s", state->sta_pass);
    ESP_LOGI(TEST_TAG, "sizeof(state): %d", sizeof(&state));

    free(state);
}

void dump_string(const char *buffer, size_t length)
{
    printf("Buffer length: %zu\n", length);
    printf("Buffer content:\n");
    for (size_t i = 0; i < length; i++)
    {
        printf("%02X ", (unsigned char)buffer[i]);
        if ((i + 1) % 8 == 0)
        {
            printf(" ");
        }
        if ((i + 1) % 16 == 0)
        {
            for (size_t j = i - 15; j <= i; j++)
            {
                if (buffer[j] >= 32 && buffer[j] <= 126) // Printable ASCII range
                {
                    printf("%c", buffer[j]);
                }
                else if (buffer[j] == '\n')
                {
                    printf("\\n");
                }
                else if (buffer[j] == '\r')
                {
                    printf("\\r");
                }
                else if (buffer[j] == '\t')
                {
                    printf("\\t");
                }
                else if (buffer[j] == '\0')
                {
                    printf("\\0");
                }
                else
                {
                    printf(".");
                }
            }
            printf("\n");
        }
    }
    if (length % 16 != 0)
    {
        printf("\n");
    }
}

/*void test_system_state(void)
{
    log_system_state();

    vTaskDelay(pdMS_TO_TICKS(100));

    char buffer[SSID_MAX_LEN];
    memcpy(buffer, system_state.ap_ssid, SSID_MAX_LEN);
    ESP_LOGI(TEST_TAG, "SSIDa: %s", buffer);

    dump_string(buffer, SSID_MAX_LEN);

    vTaskDelay(pdMS_TO_TICKS(100));

    store_running_config();

    ESP_LOGI(TEST_TAG, "Stored running config");

    vTaskDelay(pdMS_TO_TICKS(100));

    log_system_state();

    vTaskDelay(pdMS_TO_TICKS(1000));

    read_running_config();

    vTaskDelay(pdMS_TO_TICKS(100));

    log_system_state();
}*/

void system_initialize(void)
{
    memset(&system_state, 0, sizeof(system_state_t)); // Initialize system state to zero
    // Initialize NVS and read config
    nvs_initialize();
    read_running_config();
}

void nvs_initialize()
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void system_state_set(void *field_ptr, const void *value, size_t size)
{
    memcpy(field_ptr, value, size);
}

void system_state_get(const void *field_ptr, void *output, size_t size)
{
    memcpy(output, field_ptr, size);
}

void store_running_config()
{
    ESP_LOGI(TAG, "Storing running config to NVS");

    store_string(AP_SSID_KEY, system_state.ap_ssid);
    ESP_LOGI(TAG, "Storing AP SSID: %s", system_state.ap_ssid);

    store_string(STA_SSID_KEY, system_state.sta_ssid);
    ESP_LOGI(TAG, "Storing STA SSID: %s", system_state.sta_ssid);

    store_string(AP_PASS_KEY, system_state.ap_pass);
    ESP_LOGI(TAG, "Storing AP Password: %s", system_state.ap_pass);

    store_string(STA_PASS_KEY, system_state.sta_pass);
    ESP_LOGI(TAG, "Storing STA Password: %s", system_state.sta_pass);

    store_int(AP_CHANNEL_KEY, system_state.ap_channel);
    ESP_LOGI(TAG, "Storing AP Channel: %ld", system_state.ap_channel);

    store_int(SENSOR_MASK_KEY, system_state.sensor_mask);
    ESP_LOGI(TAG, "Storing Sensor Mask: %d", system_state.sensor_mask);

    store_int(WIFI_STARTUP_MODE_KEY, system_state.wifi_startup_mode);
    ESP_LOGI(TAG, "Stored running config to NVS successfully");
}

void read_running_config()
{
    char *buffer;
    size_t buffer_size = SSID_MAX_LEN;
    buffer = calloc(buffer_size, sizeof(char));
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for buffer");
        return;
    }
    esp_err_t err = read_string(AP_SSID_KEY, buffer, &buffer_size);
    if (err == ESP_OK)
    {
        if (buffer[0] == 0)
        {
            ESP_LOGI(TAG, "AP SSID found, but empty. Using default: %s", CONFIG_DEFAULT_AP_SSID);
            strlcpy(system_state.ap_ssid, CONFIG_DEFAULT_AP_SSID, sizeof(system_state.ap_ssid));
        }
        else
        {
            ESP_LOGI(TAG, "AP SSID found: %s", buffer);
            strlcpy(system_state.ap_ssid, buffer, sizeof(system_state.ap_ssid));
        }
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "AP SSID not found, using default: %s", CONFIG_DEFAULT_AP_SSID);
        strlcpy(system_state.ap_ssid, CONFIG_DEFAULT_AP_SSID, sizeof(system_state.ap_ssid));
    }
    else
    {
        ESP_LOGE(TAG, "Error reading AP SSID: %s", esp_err_to_name(err));
    }

    buffer_size = SSID_MAX_LEN;
    memset(buffer, 0, buffer_size); // Clear buffer for next use

    err = read_string(STA_SSID_KEY, buffer, &buffer_size);
    if (err == ESP_OK)
    {
        if (buffer[0] == 0)
        {
            ESP_LOGI(TAG, "STA SSID found, but empty. Using default: %s", CONFIG_DEFAULT_STA_SSID);
            strlcpy(system_state.sta_ssid, CONFIG_DEFAULT_STA_SSID, sizeof(system_state.sta_ssid));
        }
        else
        {
            ESP_LOGI(TAG, "STA SSID found: %s", buffer);
            strlcpy(system_state.sta_ssid, buffer, sizeof(system_state.sta_ssid));
        }
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "STA SSID not found, using default: %s", CONFIG_DEFAULT_STA_SSID);
        strlcpy(system_state.sta_ssid, CONFIG_DEFAULT_STA_SSID, sizeof(system_state.sta_ssid));
    }
    else
    {
        ESP_LOGE(TAG, "Error reading STA SSID: %s", esp_err_to_name(err));
    }

    free(buffer);
    buffer_size = PASS_MAX_LEN;
    buffer = calloc(buffer_size, sizeof(char));
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for buffer");
        return;
    }

    err = read_string(AP_PASS_KEY, buffer, &buffer_size);
    if (err == ESP_OK)
    {
        if (buffer[0] == 0)
        {
            ESP_LOGI(TAG, "AP Password found, but empty. Using default: %s", CONFIG_DEFAULT_AP_PASSWORD);
            strlcpy(system_state.ap_pass, CONFIG_DEFAULT_AP_PASSWORD, sizeof(system_state.ap_pass));
        }
        else
        {
            ESP_LOGI(TAG, "AP Password found: %s", buffer);
            strlcpy(system_state.ap_pass, buffer, sizeof(system_state.ap_pass));
        }
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "AP Password not found, using default: %s", CONFIG_DEFAULT_AP_PASSWORD);
        strlcpy(system_state.ap_pass, CONFIG_DEFAULT_AP_PASSWORD, sizeof(system_state.ap_pass));
    }
    else
    {
        ESP_LOGE(TAG, "Error reading AP Password: %s", esp_err_to_name(err));
    }

    buffer_size = PASS_MAX_LEN;
    memset(buffer, 0, buffer_size); // Clear buffer for next use
    err = read_string(STA_PASS_KEY, buffer, &buffer_size);
    if (err == ESP_OK)
    {
        if (buffer[0] == 0)
        {
            ESP_LOGI(TAG, "STA Password found, but empty. Using default: %s", CONFIG_DEFAULT_STA_PASSWORD);
            strlcpy(system_state.sta_pass, CONFIG_DEFAULT_STA_PASSWORD, sizeof(system_state.sta_pass));
        }
        else
        {
            ESP_LOGI(TAG, "STA Password found: %s", buffer);
            strlcpy(system_state.sta_pass, buffer, sizeof(system_state.sta_pass));
        }
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "STA Password not found, using default: %s", CONFIG_DEFAULT_STA_PASSWORD);
        strlcpy(system_state.sta_pass, CONFIG_DEFAULT_STA_PASSWORD, sizeof(system_state.sta_pass));
    }
    else
    {
        ESP_LOGE(TAG, "Error reading STA Password: %s", esp_err_to_name(err));
    }
    free(buffer); // Free buffer after use

    int32_t buffer_int = 0;
    err = read_int(AP_CHANNEL_KEY, &buffer_int);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "AP Channel found: %ld", buffer_int);
        system_state.ap_channel = buffer_int;
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "AP Channel not found, using default: %d", CONFIG_DEFAULT_AP_CHANNEL);
        system_state.ap_channel = CONFIG_DEFAULT_AP_CHANNEL;
    }
    else
    {
        ESP_LOGE(TAG, "Error reading AP Channel: %s", esp_err_to_name(err));
    }
    // Validate channel range (1-13)
    if (system_state.ap_channel < 1 || system_state.ap_channel > 13)
    {
        ESP_LOGE(TAG, "Invalid AP channel: %ld. Setting to default: %d", system_state.ap_channel, 1);
        system_state.ap_channel = 1;
    }

    err = read_int(SENSOR_MASK_KEY, &buffer_int);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Sensor Mask found: %ld", buffer_int);
        system_state.sensor_mask = buffer_int;
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "Sensor Mask not found, using default: %d", 0b00111111);
        system_state.sensor_mask = 0b00111111;
    }
    else
    {
        ESP_LOGE(TAG, "Error reading Sensor Mask: %s", esp_err_to_name(err));
    }

    err = read_int(WIFI_STARTUP_MODE_KEY, &buffer_int);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "WiFi Startup Mode found: %ld", buffer_int);
        system_state.wifi_startup_mode = buffer_int;
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "WiFi Startup Mode not found, using default: %d", WIFI_STARTUP_MODE_AP);
        system_state.wifi_startup_mode = WIFI_STARTUP_MODE_AP;
    }
    else
    {
        ESP_LOGE(TAG, "Error reading WiFi Startup Mode: %s", esp_err_to_name(err));
    }
    // Validate startup mode
    if (system_state.wifi_startup_mode != WIFI_STARTUP_MODE_STA &&
        system_state.wifi_startup_mode != WIFI_STARTUP_MODE_AP)
    {
        ESP_LOGE(TAG, "Invalid WiFi Startup Mode: %d. Setting to default: %d", system_state.wifi_startup_mode, WIFI_STARTUP_MODE_AP);
        system_state.wifi_startup_mode = WIFI_STARTUP_MODE_AP;
    }

    // ESP_LOGI(TAG, "AP SSID:");
    // dump_string(system_state.ap_ssid, SSID_MAX_LEN);
    // ESP_LOGI(TAG, "AP Password:");
    // dump_string(system_state.ap_pass, PASS_MAX_LEN);
    // ESP_LOGI(TAG, "STA SSID:");
    // dump_string(system_state.sta_ssid, SSID_MAX_LEN);
    // ESP_LOGI(TAG, "STA Password:");
    // dump_string(system_state.sta_pass, PASS_MAX_LEN);
}

static void application_task(void *args)
{
    ESP_LOGI(TAG, "task Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
    // Wait to be started by the main task
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while (1)
    {
        esp_event_loop_run(custom_event_loop, 100);
        vTaskDelay(10);
    }
}

// Initialize the event system
void events_init(void)
{
    ESP_LOGI(TAG, "init Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());

    esp_event_loop_args_t loop_args = {
        .queue_size = EVENT_LOOP_QUEUE_SIZE,
        .task_name = "custom_evt_loop",
        .task_stack_size = EVENT_LOOP_TASK_STACK_SIZE,
        .task_priority = EVENT_LOOP_TASK_PRIORITY,
        .task_core_id = EVENT_LOOP_TASK_CORE,
    };

    esp_err_t err = esp_event_loop_create(&loop_args, &custom_event_loop);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to create custom event loop: %s", esp_err_to_name(err));
    }

    // Create the application task
    TaskHandle_t task_handle;
    ESP_LOGI(TAG, "starting application task");
    xTaskCreatePinnedToCore(application_task, "application_task", TASK_APP_STACK_SIZE, NULL, TASK_APP_PRIORITY, &task_handle, TASK_APP_CORE);

    // Start the application task to run the event handlers
    xTaskNotifyGive(task_handle);
}

// Post an event to the queue
void events_post(int32_t event_id, const void *event_data, size_t event_data_size)
{
    if (custom_event_loop == NULL)
    {
        ESP_LOGE(TAG, "Custom event loop not initialized");
        return;
    }

    esp_err_t err = esp_event_post_to(custom_event_loop, CUSTOM_EVENTS, event_id, event_data, event_data_size, 0);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to post event: %s", esp_err_to_name(err));
    }
}

void events_subscribe(int32_t event_id, esp_event_handler_t event_handler, void *event_handler_arg)
{
    if (custom_event_loop == NULL)
    {
        ESP_LOGE(TAG, "Custom event loop not initialized");
        return;
    }

    esp_err_t err = esp_event_handler_instance_register_with(custom_event_loop, CUSTOM_EVENTS, event_id,
                                                             event_handler, event_handler_arg, NULL);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to subscribe to event: %s", esp_err_to_name(err));
    }
}
