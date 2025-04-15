#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <dirent.h>
#include "esp_err.h"
#include "esp_log.h"
#include "state_manager.h"

static const char *TAG = "proto";

void log_system_state(void)
{
    ESP_LOGI(TAG, "Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());

    system_state_t *state = malloc(sizeof(system_state_t));
    if (state == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for system_state_t");
        return;
    }
    memcpy(state, &system_state, sizeof(system_state_t));
    ESP_LOGI(TAG, "wifi_sta_connection_state: %d", state->wifi_sta_connection_state);
    ESP_LOGI(TAG, "wifi_ap_connection_state: %d", state->wifi_ap_connection_state);
    ESP_LOGI(TAG, "wifi_state: %d", state->wifi_state);
    ESP_LOGI(TAG, "ap_ssid: %s", state->ap_ssid);
    ESP_LOGI(TAG, "ap_pass: %s", state->ap_pass);
    ESP_LOGI(TAG, "sta_ssid: %s", state->sta_ssid);
    ESP_LOGI(TAG, "sta_pass: %s", state->sta_pass);
    ESP_LOGI(TAG, "sizeof(state): %d", sizeof(&state));

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

bool test_file(const char *path)
{
    struct stat st;
    if (stat(path, &st) == 0)
    {
        ESP_LOGI(TAG, "%s - file exists", path);
        return true;
    }
    else
    {
        ESP_LOGI(TAG, "%s - file does not exist", path);
    }
    return false;
}

void test_files(void)
{
    ESP_LOGI(TAG, "Testing file existence");

    test_file("/spiflash/index.html");
    test_file("/spiflash/favicon.ico");
}

void print_file(const char* path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
    {
        ESP_LOGE(TAG, "Failed to open file: %s", path);
        return;
    }
    char buffer[256];
    ssize_t bytes_read;
    while ((bytes_read = read(fd, buffer, sizeof(buffer) - 1)) > 0)
    {
        if (bytes_read < 0)
        {
            ESP_LOGE(TAG, "Failed to read file: %s", strerror(errno));
            close(fd);
            return;
        }
        buffer[bytes_read] = '\0'; // Null-terminate the string
        ESP_LOGI(TAG, "Read %zd bytes: %s", bytes_read, buffer);
    }

    close(fd);
}
