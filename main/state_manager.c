#include "state_manager.h"
#include "esp_log.h"
#include "cJSON.h"

void log_system_state(void);
void fatfs_test(void);

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

#define CHECK_FILE_EXTENSION(filename, ext) (strcasecmp(&filename[strlen(filename) - strlen(ext)], ext) == 0)

/* Set HTTP response content type according to file extension */
static esp_err_t send_content_type_from_file(httpd_req_t *req, const char *filepath)
{
    const char *type = "text/plain";
    if (CHECK_FILE_EXTENSION(filepath, ".html")) {
        type = "text/html";
    } else if (CHECK_FILE_EXTENSION(filepath, ".js")) {
        type = "application/javascript";
    } else if (CHECK_FILE_EXTENSION(filepath, ".css")) {
        type = "text/css";
    } else if (CHECK_FILE_EXTENSION(filepath, ".png")) {
        type = "image/png";
    } else if (CHECK_FILE_EXTENSION(filepath, ".ico")) {
        type = "image/x-icon";
    } else if (CHECK_FILE_EXTENSION(filepath, ".svg")) {
        type = "text/xml";
    }
    return httpd_resp_set_type(req, type);
}

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

    fatfs_test();

    // Initialize NVS and read config
    nvs_initialize();
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

esp_err_t file_exists_in_fatfs(const char *file_path)
{
    ESP_LOGI(TAG, "Checking if file exists: %s", file_path);

    char filename[256];
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
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "FATFS mounted successfully");

    FILE *fd = fopen(filename, "rb");
    if (fd == NULL)
    {
        ESP_LOGE(TAG, "File does not exist: %s", esp_err_to_name(errno));
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return ESP_FAIL;
    }
    fclose(fd);
    ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());

    return ESP_OK;
}

esp_err_t send_file_from_fatfs(httpd_req_t *req, const char *file_path)
{
    ESP_LOGI(TAG, "Serving file: %s", file_path);

    char filename[256];
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
    
    // Set the content type based on the file extension
    esp_err_t err_type = send_content_type_from_file(req, file_path);
    if (err_type != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set content type: %s", esp_err_to_name(err_type));
        fclose(fd);
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return err_type;
    }
    ESP_LOGI(TAG, "Content type set successfully");

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

void list_directory(const char *path)
{
    ESP_LOGI(TAG, "Listing directory: %s", path);

    DIR *dir = opendir(path);
    if (dir == NULL)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", esp_err_to_name(errno));
        ESP_ERROR_CHECK_WITHOUT_ABORT(unmount_fatfs());
        return;
    }
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        ESP_LOGI(TAG, "Found file: %s, Type: %d, %s", entry->d_name, entry->d_type, 
                 (entry->d_type == DT_DIR) ? "Directory" : "File");
    }
    closedir(dir);
}

void fatfs_test(void)
{
    ESP_LOGI(TAG, "Testing FAT filesystem with listing files recursively");
    ESP_LOGI(TAG, "Mounting FAT filesystem");
    esp_err_t err = mount_fatfs();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "FATFS mounted successfully");
    ESP_LOGI(TAG, "Listing files in FATFS");

    // List files in the FATFS
    list_directory(base_path);
    //list_directory("/spiflash/assets");

    ESP_LOGI(TAG, "Unmounting FAT filesystem");
    err = unmount_fatfs();
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to unmount FATFS: %s", esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "FATFS unmounted successfully");
    ESP_LOGI(TAG, "FATFS test completed");
}

static TimerHandle_t unmount_timer = NULL;

static void unmount_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Unmount timer expired, unmounting FATFS");
    if (s_wl_handle != WL_INVALID_HANDLE)
    {
        esp_err_t err = esp_vfs_fat_spiflash_unmount_rw_wl(base_path, s_wl_handle);
        s_wl_handle = WL_INVALID_HANDLE;
        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to unmount FATFS in timer callback (%s)", esp_err_to_name(err));
        }
        else
        {
            ESP_LOGI(TAG, "FATFS unmounted successfully in timer callback");
        }
    }
}

esp_err_t mount_fatfs(void)
{
    if (s_wl_handle != WL_INVALID_HANDLE)
    {
        ESP_LOGI(TAG, "FATFS already mounted, disabling unmount timer");
        if (unmount_timer != NULL)
        {
            xTimerStop(unmount_timer, 0);
        }
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Mounting FAT filesystem");
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = false,
        .allocation_unit_size = CONFIG_WL_SECTOR_SIZE,
        .use_one_fat = false,
    };
    esp_err_t ret = esp_vfs_fat_spiflash_mount_rw_wl(base_path, "storage", &mount_config, &s_wl_handle);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "FATFS mounted successfully at %s", base_path);
    return ESP_OK;
}

esp_err_t unmount_fatfs(void)
{
    if (s_wl_handle == WL_INVALID_HANDLE)
    {
        ESP_LOGI(TAG, "FATFS already unmounted");
        return ESP_OK;
    }

    if (unmount_timer == NULL)
    {
        unmount_timer = xTimerCreate("UnmountTimer", pdMS_TO_TICKS(2000), pdFALSE, NULL, unmount_timer_callback);
        if (unmount_timer == NULL)
        {
            ESP_LOGE(TAG, "Failed to create unmount timer");
            return ESP_FAIL;
        }
    }

    ESP_LOGI(TAG, "Starting unmount timer for 2000 ms");
    if (xTimerStart(unmount_timer, 0) != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to start unmount timer");
        return ESP_FAIL;
    }

    return ESP_OK;
}