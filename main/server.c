#include "server.h"
#include "cJSON.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static const char *TAG = "http_server";

static bool match_uri(const char *uri, const char *match)
{
    // Check if the URI starts with the match string
    return strncmp(uri, match, strlen(match)) == 0;
}

static esp_err_t send_default_response(httpd_req_t *req)
{
    // Send a simple response
    const char *resp_str = "Hello from ESP32!";
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

static esp_err_t send_ok_response(httpd_req_t *req, const char *msg)
{
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, msg, strlen(msg));
    return ESP_OK;
}

static esp_err_t send_error_response(httpd_req_t *req, const char *status, const char *msg)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, msg, strlen(msg));
    return ESP_OK;
}

static esp_err_t send_302_redirect(httpd_req_t *req, const char *location, bool prefixIP)
{
    char location_buffer[128] = {0};
    if (prefixIP)
    {
        sniprintf(location_buffer, sizeof(location_buffer), "http://192.168.4.1%s", location);
    }
    else
    {
        strlcpy(location_buffer, location, sizeof(location_buffer));
    }
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_set_hdr(req, "Location", location_buffer);
    httpd_resp_send(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t dump_request(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Request method: %d", req->method);
    ESP_LOGI(TAG, "Request URI: %s", req->uri);
    ESP_LOGI(TAG, "Request content length: %d", req->content_len);

    return ESP_OK;
}

static esp_err_t send_favicon(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serve favicon.ico");

    return send_file_from_fatfs(req, "/favicon.ico", "image/x-icon");
}

static esp_err_t send_index_html(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Serve esp STA static HTML page");

    return send_file_from_fatfs(req, "/index.html", "text/html");
}

static esp_err_t get_post_field_value(const char *buffer, const char *field, char *value, size_t value_len)
{
    // iterate over the buffer to find the field
    // first, find the exact field name followd by '='
    // then copy the value until '&' or end of string
    char *buffer_ptr = (char *)buffer;
    size_t field_len = strlen(field);

    while (*buffer_ptr != '\0')
    {
        if (strncmp(buffer_ptr, field, field_len) == 0 && buffer_ptr[field_len] == '=')
        {
            buffer_ptr += field_len + 1;             // move pointer to the start of the value
            char *end_ptr = strchr(buffer_ptr, '&'); // find the end of the value
            if (end_ptr == NULL)
            {
                end_ptr = buffer_ptr + strlen(buffer_ptr); // set end pointer to the end of the string
            }
            size_t value_length = MIN(end_ptr - buffer_ptr, value_len - 1); // copy only up to value_len - 1 characters
            strncpy(value, buffer_ptr, value_length);
            value[value_length] = '\0'; // null-terminate the string
            return ESP_OK;
        }
        buffer_ptr++;
    }

    return ESP_FAIL; // field not found
}

static esp_err_t parse_ap_post_request(httpd_req_t *req)
{
    /* if esp is in AP mode, the only incoming data is the SSID and password of the STA network
     * the data is sent as a form-urlencoded string, e.g. ssid=myssid&pass=mypassword
     * we need to parse this string and save the values in the system_state structure
     * buffer lenght should be SSID_MAX_LEN + PASS_MAX_LEN + strlen("ssid=&password=") + N
     */
    char buffer[512];
    int ret = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
    if (ret <= 0)
    {
        ESP_LOGE(TAG, "Socket error: %d", ret);
        return send_error_response(req, "500 Internal Server Error", "Failed to receive data");
    }
    buffer[ret] = '\0'; // null-terminate the string
    char sta_ssid[SSID_MAX_LEN] = {0};
    char sta_pass[PASS_MAX_LEN] = {0};
    char ap_ssid[SSID_MAX_LEN] = {0};
    char ap_pass[PASS_MAX_LEN] = {0};
    char sensor_mask_str[4] = {0};
    int sensor_mask = 0;

    ESP_LOGI(TAG, "Received data: %s", buffer);

    if (get_post_field_value(buffer, "sta_ssid", sta_ssid, sizeof(sta_ssid)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SSID from POST data");
        return send_error_response(req, "400 Bad Request", "Invalid SSID format");
    }
    if (get_post_field_value(buffer, "sta_pass", sta_pass, sizeof(sta_pass)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get password from POST data");
        return send_error_response(req, "400 Bad Request", "Invalid password format");
    }
    if (get_post_field_value(buffer, "ap_ssid", ap_ssid, sizeof(ap_ssid)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get AP SSID from POST data");
        return send_error_response(req, "400 Bad Request", "Invalid AP SSID format");
    }
    if (get_post_field_value(buffer, "ap_pass", ap_pass, sizeof(ap_pass)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get AP password from POST data");
        return send_error_response(req, "400 Bad Request", "Invalid AP password format");
    }
    if (get_post_field_value(buffer, "sensor_mask", sensor_mask_str, sizeof(sensor_mask_str)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get sensor mask from POST data");
        return send_error_response(req, "400 Bad Request", "Invalid sensor mask format");
    }
    sensor_mask = atoi(sensor_mask_str);

    strlcpy(system_state.sta_ssid, sta_ssid, sizeof(system_state.sta_ssid));
    strlcpy(system_state.sta_pass, sta_pass, sizeof(system_state.sta_pass));
    strlcpy(system_state.ap_ssid, ap_ssid, sizeof(system_state.ap_ssid));
    strlcpy(system_state.ap_pass, ap_pass, sizeof(system_state.ap_pass));
    system_state.sensor_mask = sensor_mask;

    ESP_LOGI(TAG, "SSID: %s, Password: %s", system_state.sta_ssid, system_state.sta_pass);

    vTaskDelay(pdMS_TO_TICKS(100));

    store_running_config();
    ESP_LOGI(TAG, "Running config stored");
    esp_err_t err = send_ok_response(req, "STA credentials saved successfully. Restarting ESP32...");

    events_post(EVENT_RESTART_REQUESTED, NULL, 0);
    ESP_LOGI(TAG, "Restart requested");

    vTaskDelay(pdMS_TO_TICKS(2000)); // wait for the event to be processed
    esp_restart(); // restart the ESP32
    ESP_LOGI(TAG, "ESP32 restarted");

    return err;
}

static esp_err_t config_http_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Config handler Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
    dump_request(req);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        ESP_LOGE(TAG, "Failed to create JSON object");
        return send_error_response(req, "500 Internal Server Error", "Failed to create JSON object");
    }
    cJSON_AddStringToObject(root, "ap_ssid", system_state.ap_ssid);
    cJSON_AddStringToObject(root, "ap_pass", system_state.ap_pass);
    cJSON_AddStringToObject(root, "sta_ssid", system_state.sta_ssid);
    cJSON_AddStringToObject(root, "sta_pass", system_state.sta_pass);
    cJSON_AddNumberToObject(root, "sensor_mask", system_state.sensor_mask);

    return send_ok_response(req, cJSON_Print(root));
}

static esp_err_t settings_http_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Settings handler Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
    dump_request(req);

    if (req->method == HTTP_GET)
    {
        ESP_LOGI(TAG, "Serving settings.html");
        return send_index_html(req);
    }
    else if (req->method == HTTP_POST)
    {
        ESP_LOGI(TAG, "POST request received in settings handler");
        if (parse_ap_post_request(req) == ESP_OK)
        {
            return ESP_OK; // Response already sent in parse_ap_post_request
        }
    }

    return send_default_response(req);
}

static esp_err_t http_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP Handler Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
    dump_request(req);

    if (system_state.wifi_state == WIFI_STATE_AP)
    {
        if (match_uri(req->uri, "/favicon.ico"))
        {
            return send_favicon(req);
        }
        
        return send_302_redirect(req, "/settings.html", true);
    }

    return send_default_response(req);
}

void start_http_server(void)
{
    ESP_LOGI(TAG, "Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;

    ESP_LOGI(TAG, "Starting HTTP server...");
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t config_uri = {
            .uri = "/config*",
            .method = HTTP_ANY,
            .handler = config_http_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &config_uri);
        httpd_uri_t settings_uri = {
            .uri = "/settings*",
            .method = HTTP_ANY,
            .handler = settings_http_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &settings_uri);
        httpd_uri_t root_uri = {
            .uri = "/*",
            .method = HTTP_ANY,
            .handler = http_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &root_uri);
        ESP_LOGI(TAG, "HTTP server started successfully.");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start HTTP server.");
    }
}