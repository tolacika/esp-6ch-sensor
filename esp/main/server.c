#include "server.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static const char *TAG = "http_server";

extern const char index_html_start[] asm("_binary_index_html_start");
extern const char index_html_end[] asm("_binary_index_html_end");

static esp_err_t send_default_response(httpd_req_t *req)
{
    // Send a simple response
    const char *resp_str = "Hello from ESP32!";
    httpd_resp_set_status(req, "404 Not Found");
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp_str, strlen(resp_str));
    return ESP_OK;
}

static esp_err_t dump_request(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Request method: %d", req->method);
    ESP_LOGI(TAG, "Request URI: %s", req->uri);
    ESP_LOGI(TAG, "Request content length: %d", req->content_len);
    // dump content
    char buf[100];
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
    }
    
    send_default_response(req);

    return ESP_OK;
}

static esp_err_t send_index_html(httpd_req_t *req)
{
    const uint32_t root_len = index_html_end - index_html_start;

    ESP_LOGI(TAG, "Serve esp STA static HTML page");

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, index_html_start, root_len);

    return ESP_OK;
}

static esp_err_t http_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "HTTP request received");

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
            return dump_request(req);
        }
    }

    return send_default_response(req);
}

void start_http_server(void)
{
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