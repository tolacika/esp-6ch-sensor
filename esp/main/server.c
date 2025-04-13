#include "server.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static const char *TAG = "http_server";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");
extern const char favicon_ico_start[] asm("_binary_favicon_ico_start");
extern const char favicon_ico_end[] asm("_binary_favicon_ico_end");

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

static esp_err_t dump_request(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Request method: %d", req->method);
    ESP_LOGI(TAG, "Request URI: %s", req->uri);
    ESP_LOGI(TAG, "Request content length: %d", req->content_len);
    // dump content
    /*char buf[100];
    int ret, remaining = req->content_len;
    while (remaining > 0)
    {
        ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1));
        if (ret <= 0)
        {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT)
            {
                continue;
            }
            ESP_LOGE(TAG, "Socket error: %d", ret);
            return ESP_FAIL;
        }
        buf[ret] = '\0';
        ESP_LOGI(TAG, "Received data: %s", buf);
        remaining -= ret;
    }*/

    return ESP_OK;
}

static esp_err_t send_index_html(httpd_req_t *req)
{
    if (match_uri(req->uri, "/favicon.ico"))
    {
        ESP_LOGI(TAG, "Serve favicon.ico");
        const uint32_t favicon_len = favicon_ico_end - favicon_ico_start;
        httpd_resp_set_type(req, "image/x-icon");
        httpd_resp_send(req, favicon_ico_start, favicon_len);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Serve esp STA static HTML page");
    const uint32_t root_len = index_html_end - index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start, root_len);
    return ESP_OK;
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
            buffer_ptr += field_len + 1; // move pointer to the start of the value
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
    if (match_uri(req->uri, "/sta/save"))
    {
        /* if esp is in AP mode, the only incoming data is the SSID and password of the STA network
         * the data is sent as a form-urlencoded string, e.g. ssid=myssid&pass=mypassword
         * we need to parse this string and save the values in the system_state structure
         * buffer lenght should be SSID_MAX_LEN + PASS_MAX_LEN + strlen("ssid=&password=") + N 
         */
        char buffer[SSID_MAX_LEN + PASS_MAX_LEN + 30];
        int ret = httpd_req_recv(req, buffer, sizeof(buffer) - 1);
        if (ret <= 0)
        {
            ESP_LOGE(TAG, "Socket error: %d", ret);
            return send_error_response(req, "500 Internal Server Error", "Failed to receive data");
        }
        char ssid[SSID_MAX_LEN + 1] = {0};
        char pass[PASS_MAX_LEN + 1] = {0};
        buffer[ret] = '\0'; // null-terminate the string
        ESP_LOGI(TAG, "Received data: %s", buffer);

        if (get_post_field_value(buffer, "ssid", ssid, sizeof(ssid)) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get SSID from POST data");
            return send_error_response(req, "400 Bad Request", "Invalid SSID format");
        }
        if (get_post_field_value(buffer, "password", pass, sizeof(pass)) != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to get password from POST data");
            return send_error_response(req, "400 Bad Request", "Invalid password format");
        }

        ESP_LOGI(TAG, "SSID: %s, Password: %s", ssid, pass);
        // Save the SSID and password in the system state

        system_state_set(&system_state.sta_ssid, ssid, sizeof(ssid));
        system_state_set(&system_state.sta_pass, pass, sizeof(pass));

        ESP_LOGI(TAG, "SSID: %s, Password: %s", system_state.sta_ssid, system_state.sta_pass);

        vTaskDelay(pdMS_TO_TICKS(1)); 

        xTaskCreate(store_running_config, "store_running_config", 4096, NULL, 5, NULL);
        ESP_LOGI(TAG, "running config stored");

        return send_ok_response(req, "STA credentials saved successfully");
    }

    return ESP_FAIL; // No matching URI found
}

static esp_err_t http_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Prio handler: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());
    dump_request(req);

    if (system_state.wifi_state == WIFI_STATE_AP)
    {
        if (req->method == HTTP_GET)
        {
            ESP_LOGI(TAG, "Serving index.html for AP mode");
            return send_index_html(req);
        }
        if (req->method == HTTP_POST)
        {
            ESP_LOGI(TAG, "POST request received in AP mode");
            if (parse_ap_post_request(req) == ESP_OK)
            {
                return ESP_OK; // Response already sent in parse_ap_post_request
            }
        }
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