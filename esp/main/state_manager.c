#include "state_manager.h"

const char *TAG = "state_manager";

system_state_t system_state = {
    .wifi_sta_connection_state = 0,
    .wifi_ap_connection_state = 0,
    .wifi_state = WIFI_STATE_NONE,
    .ap_ssid = {0},
    .ap_pass = {0},
    .ap_channel = 0,
    .sta_ssid = {0},
    .sta_pass = {0},
};

static esp_event_loop_handle_t custom_event_loop = NULL; // Custom event loop handle
ESP_EVENT_DEFINE_BASE(CUSTOM_EVENTS); // Define the event base for custom events


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
    char *buffer;
    size_t buffer_size = SSID_MAX_LEN + 1; // +1 for null terminator
    buffer = malloc(buffer_size);

    SYSTEM_STATE_GET(ap_ssid, buffer);
    ESP_LOGI(TAG, "Storing AP SSID: %s", buffer);
    store_string(AP_SSID_KEY, buffer);
    memset(buffer, 0, buffer_size); // Clear buffer for next use

    SYSTEM_STATE_GET(sta_ssid, buffer);
    ESP_LOGI(TAG, "Storing STA SSID: %s", buffer);
    store_string(STA_SSID_KEY, buffer);
    memset(buffer, 0, buffer_size); // Clear buffer for next use

    buffer_size = PASS_MAX_LEN + 1; // +1 for null terminator
    buffer = realloc(buffer, buffer_size);

    SYSTEM_STATE_GET(ap_pass, buffer);
    ESP_LOGI(TAG, "Storing AP Password: %s", buffer);
    store_string(AP_PASS_KEY, buffer);
    memset(buffer, 0, buffer_size); // Clear buffer for next use

    SYSTEM_STATE_GET(sta_pass, buffer);
    ESP_LOGI(TAG, "Storing STA Password: %s", buffer);
    store_string(STA_PASS_KEY, buffer);
    
    free(buffer); // Free buffer after use

    int32_t buffer_int = 0;
    system_state_get(&system_state.ap_channel, &buffer_int, sizeof(buffer_int));
    ESP_LOGI(TAG, "Storing AP Channel: %ld", buffer_int);
    store_int(AP_CHANNEL_KEY, buffer_int);
}

void read_running_config()
{
    char *buffer;
    size_t buffer_size = SSID_MAX_LEN + 1; // +1 for null terminator
    buffer = malloc(buffer_size);

    esp_err_t err = read_string(AP_SSID_KEY, buffer, &buffer_size);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "AP SSID found: %s", buffer);
        SYSTEM_STATE_SET(ap_ssid, buffer);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "AP SSID not found, using default: %s", CONFIG_DEFAULT_AP_SSID);
        SYSTEM_STATE_SET(ap_ssid, CONFIG_DEFAULT_AP_SSID);
    }
    memset(buffer, 0, buffer_size); // Clear buffer for next use

    err = read_string(STA_SSID_KEY, buffer, &buffer_size);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "STA SSID found: %s", buffer);
        SYSTEM_STATE_SET(sta_ssid, buffer);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "STA SSID not found, using default: %s", CONFIG_DEFAULT_STA_SSID);
        SYSTEM_STATE_SET(sta_ssid, CONFIG_DEFAULT_STA_SSID);
    }
    memset(buffer, 0, buffer_size); // Clear buffer for next use

    buffer_size = PASS_MAX_LEN + 1; // +1 for null terminator
    buffer = realloc(buffer, buffer_size);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to reallocate memory for buffer");
        return;
    }

    err = read_string(AP_PASS_KEY, buffer, &buffer_size);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "AP Password found: %s", buffer);
        SYSTEM_STATE_SET(ap_pass, buffer);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "AP Password not found, using default: %s", CONFIG_DEFAULT_AP_PASSWORD);
        SYSTEM_STATE_SET(ap_pass, CONFIG_DEFAULT_AP_PASSWORD);
    }
    memset(buffer, 0, buffer_size); // Clear buffer for next use

    err = read_string(STA_PASS_KEY, buffer, &buffer_size);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "STA Password found: %s", buffer);
        SYSTEM_STATE_SET(sta_pass, buffer);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "STA Password not found, using default: %s", CONFIG_DEFAULT_STA_PASSWORD);
        SYSTEM_STATE_SET(sta_pass, CONFIG_DEFAULT_STA_PASSWORD);
    }
    free(buffer); // Free buffer after use

    int32_t buffer_int = 0;
    err = read_int(AP_CHANNEL_KEY, &buffer_int);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "AP Channel found: %ld", buffer_int);
        SYSTEM_STATE_SET(ap_channel, buffer_int);
    }
    else if (err == ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGI(TAG, "AP Channel not found, using default: %d", CONFIG_DEFAULT_AP_CHANNEL);
        store_int(AP_CHANNEL_KEY, CONFIG_DEFAULT_AP_CHANNEL);
        buffer_int = CONFIG_DEFAULT_AP_CHANNEL;
        SYSTEM_STATE_SET(ap_channel, buffer_int);

    }
    // Validate channel range (1-13)
    if (buffer_int < 1 || buffer_int > 13)
    {
        ESP_LOGE(TAG, "Invalid AP channel: %ld. Setting to default: %d", buffer_int, 1);
        buffer_int = 1;
        SYSTEM_STATE_SET(ap_channel, buffer_int);
    }
    
}

static void application_task(void* args)
{
    // Wait to be started by the main task
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    while(1) {
        esp_event_loop_run(custom_event_loop, 100);
        vTaskDelay(10);
    }
}

// Initialize the event system
void events_init(void) {
    esp_event_loop_args_t loop_args = {
        .queue_size = 10, // Adjust the queue size as needed
        .task_name = "custom_evt_loop", // Name of the event loop task
        .task_stack_size = 3072, // Stack size for the event loop task
        .task_priority = uxTaskPriorityGet(NULL), // Priority for the event loop task
        .task_core_id = tskNO_AFFINITY // Core to run the event loop task
    };

    esp_err_t err = esp_event_loop_create(&loop_args, &custom_event_loop);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create custom event loop: %s", esp_err_to_name(err));
    }

    // Create the application task
    TaskHandle_t task_handle;
    ESP_LOGI(TAG, "starting application task");
    xTaskCreate(application_task, "application_task", 3072, NULL, uxTaskPriorityGet(NULL) + 1, &task_handle);

    // Start the application task to run the event handlers
    xTaskNotifyGive(task_handle);
}

// Post an event to the queue
void events_post(int32_t event_id, const void* event_data, size_t event_data_size) {
    if (custom_event_loop == NULL) {
        ESP_LOGE(TAG, "Custom event loop not initialized");
        return;
    }

    esp_err_t err = esp_event_post_to(custom_event_loop, CUSTOM_EVENTS, event_id, event_data, event_data_size, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post event: %s", esp_err_to_name(err));
    }
}

void events_subscribe(int32_t event_id, esp_event_handler_t event_handler, void* event_handler_arg) {
    if (custom_event_loop == NULL) {
        ESP_LOGE(TAG, "Custom event loop not initialized");
        return;
    }

    esp_err_t err = esp_event_handler_instance_register_with(custom_event_loop, CUSTOM_EVENTS, event_id,
        event_handler, event_handler_arg, NULL);
    if (err != ESP_OK) {    
        ESP_LOGE(TAG, "Failed to subscribe to event: %s", esp_err_to_name(err));
    }
}
