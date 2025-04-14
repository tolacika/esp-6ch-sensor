#include "fatfs_manager.h"
#include "esp_log.h"
#include <sys/errno.h>

static const char *TAG = "fatfs_manager";

// Handle of the wear levelling library instance
static wl_handle_t s_wl_handle = WL_INVALID_HANDLE;

// Mount path for the partition
const char *base_path = "/spiflash";

void list_dir(const char *path)
{
    ESP_LOGI(TAG, "Listing files in %s:", path);

    DIR *dir = opendir(path);
    if (!dir)
    {
        ESP_LOGE(TAG, "Failed to open directory: %s", strerror(errno));
        return;
    }

    printf("%s:\n", path);
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        printf(
            "    %s: %s\n",
            (entry->d_type == DT_DIR)
                ? "directory"
                : "file     ",
            entry->d_name);
    }

    closedir(dir);
}

void test_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0) {
        ESP_LOGI(TAG, "%s - file exists", path);
        return;
    } else {
        ESP_LOGI(TAG, "%s - file does not exist", path);
    }
}

void test_files(void)
{
    ESP_LOGI(TAG, "Testing file existence");

    test_file("/spiflash/index.html");
    test_file("/spiflash/favicon.ico");
}

/*

static esp_err_t send_index_html(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serve esp STA static HTML page");

    FILE *file = fopen("/spiflash/index.html", "rb");
    if (file == NULL)
    {
        ESP_LOGE(TAG, "Failed to open index.html file");
        return send_error_response(req, "500 Internal Server Error", "Failed to open index.html file");
    }

    fseek(file, 0, SEEK_SET);

    char buffer[512] = {0};
    size_t bytes_read = 0;

    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0)
    {
        httpd_resp_send_chunk(req, buffer, bytes_read);
    }
    fclose(file);

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, NULL, 0); // Send the last chunk to indicate end of response
    return ESP_OK;
}*/

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

    int file = open(filename, O_RDONLY, 0);
    if (file < 0)
    {
        ESP_LOGE(TAG, "Failed to open file: %s", filename);
        return ESP_FAIL;
    }

    char *buffer = calloc(1, SCRATCH_BUFSIZE);
    if (buffer == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate buffer for file read");
        close(file);
        return ESP_FAIL;
    }
    ssize_t bytes_read = 0;

    do
    {
        bytes_read = read(file, buffer, SCRATCH_BUFSIZE);
        if (bytes_read == -1) {
            ESP_LOGE(TAG, "Failed to read file : %s", file_path);
        } else if (bytes_read > 0) {
            /* Send the buffer contents as HTTP response chunk */
            if (httpd_resp_send_chunk(req, buffer, bytes_read) != ESP_OK) {
                close(file);
                ESP_LOGE(TAG, "File sending failed!");
                /* Abort sending file */
                httpd_resp_sendstr_chunk(req, NULL);
                /* Respond with 500 Internal Server Error */
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                return ESP_FAIL;
            }
        }
    } while (bytes_read > 0);
    close(file);

    httpd_resp_set_type(req, content_type);
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

void fatfs_init(void)
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
}