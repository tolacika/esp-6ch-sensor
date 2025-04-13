#ifndef SERVER_H
#define SERVER_H

#include "esp_http_server.h"
#include "esp_log.h"
#include "config.h"
#include "state_manager.h"

void start_http_server(void);

#endif // SERVER_H