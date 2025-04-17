#include "state_manager.h"
#include "esp_log.h"
#include "cJSON.h"

bool test_file(const char *path);
void test_files(void);
void list_dir(const char *path);
void print_file(const char *path);
void log_system_state(void);

const char *TAG = "state_manager";

system_state_t system_state = {
    .wifi_sta_connection_state = 0,
    .wifi_ap_connection_state = 0,
    .wifi_state = WIFI_STATE_NONE,
    .ap_ssid = {0},
    .ap_pass = {0},
    .sta_ssid = {0},
    .sta_pass = {0},
    .sensor_mask = 0,
    .wifi_startup_mode = WIFI_STARTUP_MODE_NONE,
};

// Custom event loop handle
static esp_event_loop_handle_t custom_event_loop = NULL;

// Define the event base for custom events
ESP_EVENT_DEFINE_BASE(CUSTOM_EVENTS);

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// Semaphore for FATFS operations
static SemaphoreHandle_t fatfs_mutex = NULL;

// Mount path for the partition
const char *base_path = "/spiflash";

static const char *config_file_path = "/spiflash/config.json";

void system_initialize(void)
{
    memset(&system_state, 0, sizeof(system_state_t)); // Initialize system state to zero

    fatfs_mutex = xSemaphoreCreateMutex();
    if (fatfs_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create FATFS mutex");
        return;
    }

    system_state_t *state = calloc(1, sizeof(system_state_t));
    if (state == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for system_state_t");
        return;
    }
    memcpy(state, &system_state, sizeof(system_state_t));

    esp_err_t err = read_running_config_from_fatfs();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to read running config from FATFS: %s", esp_err_to_name(err));
        load_default_running_config();
        ESP_ERROR_CHECK(store_running_config_in_fatfs());
    }

    // Initialize NVS and read config
    nvs_initialize();
    //    read_running_config();
}

void system_shutdown(void)
{

    if (s_wl_handle != WL_INVALID_HANDLE)
    {
        unmount_fatfs();
    }

    if (fatfs_mutex != NULL)
    {

        vSemaphoreDelete(fatfs_mutex);
        fatfs_mutex = NULL;
    }

    // Todo: Other shutdown code...
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

/*
 * @deprecated
 */
void system_state_set(void *field_ptr, const void *value, size_t size)
{
    memcpy(field_ptr, value, size);
}

/*
 * @deprecated
 */
void system_state_get(const void *field_ptr, void *output, size_t size)
{
    memcpy(output, field_ptr, size);
}

esp_err_t load_default_running_config()
{
    ESP_LOGI(TAG, "Loading default running config");
    system_state.wifi_sta_connection_state = 0;
    system_state.wifi_ap_connection_state = 0;
    system_state.wifi_state = WIFI_STATE_NONE;
    snprintf(system_state.ap_ssid, sizeof(system_state.ap_ssid), CONFIG_DEFAULT_AP_SSID);
    snprintf(system_state.ap_pass, sizeof(system_state.ap_pass), CONFIG_DEFAULT_AP_PASSWORD);
    snprintf(system_state.sta_ssid, sizeof(system_state.sta_ssid), CONFIG_DEFAULT_STA_SSID);
    snprintf(system_state.sta_pass, sizeof(system_state.sta_pass), CONFIG_DEFAULT_STA_PASSWORD);
    system_state.sensor_mask = CONFIG_DEFAULT_SENSOR_MASK;
#ifdef CONFIG_DEFAULT_WIFI_STARTUP_MODE_STA
    system_state.wifi_startup_mode = WIFI_STARTUP_MODE_STA;
#else
    system_state.wifi_startup_mode = WIFI_STARTUP_MODE_AP;
#endif

    return ESP_OK;
}

esp_err_t store_running_config_in_fatfs()
{
    cJSON *json = cJSON_CreateObject();
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return ESP_FAIL;
    }
    cJSON_AddStringToObject(json, "ap_ssid", system_state.ap_ssid);
    cJSON_AddStringToObject(json, "ap_pass", system_state.ap_pass);
    cJSON_AddStringToObject(json, "sta_ssid", system_state.sta_ssid);
    cJSON_AddStringToObject(json, "sta_pass", system_state.sta_pass);
    cJSON_AddNumberToObject(json, "sensor_mask", system_state.sensor_mask);
    cJSON_AddNumberToObject(json, "wifi_startup_mode", system_state.wifi_startup_mode);
    char *json_string = cJSON_PrintUnformatted(json);
    if (json_string == NULL)
    {
        ESP_LOGE(TAG, "Failed to print JSON object");
        cJSON_Delete(json);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "JSON string: %s", json_string);

    esp_err_t err = mount_fatfs();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        cJSON_Delete(json);
        free(json_string);
        return err;
    }
    ESP_LOGI(TAG, "FATFS mounted successfully");

    int fd = open(config_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "Failed to open config file: %s", esp_err_to_name(errno));
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        cJSON_Delete(json);
        free(json_string);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Config file opened successfully");

    ssize_t bytes_written = write(fd, json_string, strlen(json_string));
    if (bytes_written < 0)
    {
        ESP_LOGE(TAG, "Failed to write to config file: %s", esp_err_to_name(errno));
        close(fd);
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        cJSON_Delete(json);
        free(json_string);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Config file written successfully");

    close(fd);
    free(json_string);
    cJSON_Delete(json);
    err = unmount_fatfs();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount FATFS: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "FATFS unmounted successfully");
    ESP_LOGI(TAG, "Stored running config to FATFS successfully");
    return ESP_OK;
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

    store_int(SENSOR_MASK_KEY, system_state.sensor_mask);
    ESP_LOGI(TAG, "Storing Sensor Mask: %d", system_state.sensor_mask);

    store_int(WIFI_STARTUP_MODE_KEY, system_state.wifi_startup_mode);
    ESP_LOGI(TAG, "Stored running config to NVS successfully");
}

esp_err_t read_running_config_from_fatfs()
{
    esp_err_t err = mount_fatfs();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return err;
    }
    ESP_LOGI(TAG, "FATFS mounted successfully");

    int fd = open(config_file_path, O_RDONLY);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "Failed to open config file: %s", esp_err_to_name(errno));
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Config file opened successfully");

    char buffer[CONFIG_FILE_MAX_LEN];
    ssize_t bytes_read = read(fd, buffer, sizeof(buffer) - 1);
    if (bytes_read < 0)
    {
        ESP_LOGE(TAG, "Failed to read config file: %s", esp_err_to_name(errno));
        close(fd);
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return ESP_FAIL;
    }
    buffer[bytes_read] = '\0'; // Null-terminate the string
    ESP_LOGI(TAG, "Config file read successfully");
    ESP_LOGI(TAG, "Config file content: %s", buffer);
    close(fd);

    err = unmount_fatfs();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount FATFS: %s", esp_err_to_name(err));
        return err;
    }
    ESP_LOGI(TAG, "FATFS unmounted successfully");

    ESP_LOGI(TAG, "Parsing JSON from config file");
    cJSON *json = cJSON_Parse(buffer);
    if (json == NULL)
    {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "JSON parsed successfully");

    char *ap_ssid = cJSON_GetObjectItem(json, "ap_ssid")->valuestring;
    char *ap_pass = cJSON_GetObjectItem(json, "ap_pass")->valuestring;
    char *sta_ssid = cJSON_GetObjectItem(json, "sta_ssid")->valuestring;
    char *sta_pass = cJSON_GetObjectItem(json, "sta_pass")->valuestring;
    int sensor_mask = cJSON_GetObjectItem(json, "sensor_mask")->valueint;
    int wifi_startup_mode = cJSON_GetObjectItem(json, "wifi_startup_mode")->valueint;
    ESP_LOGI(TAG, "Parsed JSON values: ap_ssid=%s, ap_pass=%s, sta_ssid=%s, sta_pass=%s, sensor_mask=%d, wifi_startup_mode=%d",
             ap_ssid, ap_pass, sta_ssid, sta_pass, sensor_mask, wifi_startup_mode);

    strlcpy(system_state.ap_ssid, ap_ssid, sizeof(system_state.ap_ssid));
    strlcpy(system_state.ap_pass, ap_pass, sizeof(system_state.ap_pass));
    strlcpy(system_state.sta_ssid, sta_ssid, sizeof(system_state.sta_ssid));
    strlcpy(system_state.sta_pass, sta_pass, sizeof(system_state.sta_pass));
    system_state.sensor_mask = sensor_mask;
    system_state.wifi_startup_mode = wifi_startup_mode;
    ESP_LOGI(TAG, "Stored running config from FATFS successfully");
    cJSON_Delete(json);

    log_system_state();

    return ESP_OK;
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
    memset(buffer, 0, buffer_size);

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
    memset(buffer, 0, buffer_size);
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

esp_err_t send_file_from_fatfs(httpd_req_t *req, const char *file_path, const char *content_type)
{
    ESP_LOGI(TAG, "Serving file: %s", file_path);

    char filename[128];
    if (file_path[0] == '/')
    {
        snprintf(filename, sizeof(filename), "%s%s", base_path, file_path);
    }
    else
    {
        snprintf(filename, sizeof(filename), "%s/%s", base_path, file_path);
    }
    ESP_LOGI(TAG, "Full file path: %s", filename);

    esp_err_t err = mount_fatfs();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS: %s", esp_err_to_name(err));
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return err;
    }
    ESP_LOGI(TAG, "FATFS mounted successfully");

    FILE *fd = fopen(filename, "rb");
    if (fd == NULL)
    {
        ESP_LOGE(TAG, "Failed to open file: %s", esp_err_to_name(errno));
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "File opened successfully");
    httpd_resp_set_type(req, content_type);
    char *buffer = calloc(1, SCRATCH_BUFSIZE);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate buffer for file read");
        fclose(fd);
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return ESP_FAIL;
    }
    size_t bytes_read;
    do
    {
        bytes_read = fread(buffer, 1, SCRATCH_BUFSIZE, fd);
        if (bytes_read > 0)
        {
            ESP_LOGI(TAG, "Sending %d bytes", bytes_read);
            if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK)
            {
                fclose(fd);
                ESP_LOGE(TAG, "File sending failed!");
                ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (bytes_read > 0);

    fclose(fd);
    ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());

    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

/*void fatfs_init(void)
{
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = false,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false,
    };
    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find FATFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize FATFS (%s)", esp_err_to_name(ret));
        }
        return;
    }
    ESP_LOGI(TAG, "FATFS mounted successfully at %s", base_path);

    test_files();
    list_dir(base_path);
}*/

esp_err_t mount_fatfs(void)
{
    // Take the mutex
    if (xSemaphoreTake(fatfs_mutex, pdMS_TO_TICKS(20000)) != pdTRUE)
    {
        ESP_LOGE(TAG, "Failed to take FATFS mutex");
        return ESP_ERR_TIMEOUT;
    }

    ESP_LOGI(TAG, "Mounting FAT filesystem");
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = false, // Set to true if you want to auto-format on failure
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false,
    };
    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find FATFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize FATFS (%s)", esp_err_to_name(ret));
        }
        // Release the mutex on failure
        xSemaphoreGive(fatfs_mutex);
        return ret;
    }
    ESP_LOGI(TAG, "FATFS mounted successfully at %s", base_path);
    return ESP_OK; // Mutex will be released after the corresponding unmount
}

esp_err_t unmount_fatfs(void)
{
    ESP_LOGI(TAG, "Unmounting FAT filesystem");

    esp_err_t err = esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle);
    s_wl_handle = WL_INVALID_HANDLE; // Mark handle as invalid
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount FATFS (%s)", esp_err_to_name(err));
    }

    // Release the mutex
    xSemaphoreGive(fatfs_mutex);
    return err;
}