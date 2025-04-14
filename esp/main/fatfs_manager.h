#ifndef FATFS_MANAGER_H
#define FATFS_MANAGER_H

#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_vfs.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "esp_http_server.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + 128)
#define SCRATCH_BUFSIZE (10240)


void fatfs_init(void);

esp_err_t send_file_from_fatfs(httpd_req_t *req, const char *file_path, const char *content_type);

#endif // FATFS_MANAGER_H