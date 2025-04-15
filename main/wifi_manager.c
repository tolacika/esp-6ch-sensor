#include "wifi_manager.h"
#include "lwip/sockets.h"
#include "esp_mac.h"
// #include "lwip/netdb.h"
// #include "lwip/dns.h"

// EventGroupHandle_t wifi_event_group;
static const char *TAG = "wifi_ap";

static esp_netif_t *ap_netif = NULL;
static esp_netif_t *sta_netif = NULL; 

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "Event received: %s, ID: %ld", event_base, event_id);
    if (event_base == WIFI_EVENT)
    {
        switch (event_id)
        {
        case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
        {
            wifi_event_sta_disconnected_t *disconnected = (wifi_event_sta_disconnected_t *)event_data;
            ESP_LOGW(TAG, "WiFi disconnected, reason: %d", disconnected->reason);

            switch (disconnected->reason)
            {
            case WIFI_REASON_NO_AP_FOUND:
                ESP_LOGE(TAG, "SSID not found");
                break;
            case WIFI_REASON_AUTH_FAIL:
                ESP_LOGE(TAG, "Authentication failed");
                break;
            default:
                ESP_LOGE(TAG, "Disconnected for unknown reason %d", disconnected->reason);
                break;
            }

            // Update system state
            system_state_set(&system_state.wifi_sta_connection_state, &disconnected->reason, sizeof(disconnected->reason));
            system_state_set(&system_state.wifi_current_ip, NULL, 0); // Clear current IP address
            // Trigger EVENT_WIFI_DISCONNECTED
            events_post(EVENT_WIFI_STATE_CHANGED, NULL, 0);

            vTaskDelay(pdMS_TO_TICKS(5000)); // Wait before retrying
            ESP_LOGI(TAG, "Retrying connection...");

            // esp_wifi_connect(); // Retry connection
            break;
        }
        break;
        case WIFI_EVENT_AP_STACONNECTED:
            wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
            ESP_LOGI(TAG, "Station connected, MAC: " MACSTR " AID=%d", MAC2STR(event->mac), event->aid);
            break;
        case WIFI_EVENT_AP_STADISCONNECTED:
            wifi_event_ap_stadisconnected_t *event_disconnected = (wifi_event_ap_stadisconnected_t *)event_data;
            ESP_LOGI(TAG, "Station disconnected, MAC: " MACSTR " AID=%d, reason: %d", MAC2STR(event_disconnected->mac), event_disconnected->aid, event_disconnected->reason);
            break;
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        // Update system state
        system_state_set(&system_state.wifi_current_ip, &event->ip_info.ip, sizeof(event->ip_info.ip)); // Update current IP address
        system_state_set(&system_state.wifi_sta_connection_state, 0, 0);                                 // Clear connection state
        // Trigger EVENT_WIFI_CONNECTED
        events_post(EVENT_WIFI_STATE_CHANGED, NULL, 0);
    }
}

void wifi_sta_init(void)
{
    // Create default WiFi STA
    esp_netif_create_default_wifi_sta();

    // Initialize WiFi with default configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    // Configure WiFi STA settings
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = "",
            .password = "",
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    strlcpy((char *)wifi_config.sta.ssid, system_state.sta_ssid, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, system_state.sta_pass, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA initialized and connecting...");
}

void wifi_ap_init(void)
{
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap(); // Set IP address for the access point
    esp_netif_ip_info_t ip_info;
    IP4_ADDR(&ip_info.ip, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 4, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif)); // Stop DHCP server before setting IP
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif)); // Restart DHCP server

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, NULL));

    // Configure WiFi AP settings
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .password = "",
            .max_connection = WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .channel = system_state.ap_channel,
        },
    };

    strlcpy((char *)wifi_config.ap.ssid, system_state.ap_ssid, sizeof(wifi_config.ap.ssid));
    strlcpy((char *)wifi_config.ap.password, system_state.ap_pass, sizeof(wifi_config.ap.password));
    wifi_config.ap.ssid_len = strnlen(system_state.ap_ssid, sizeof(wifi_config.ap.ssid));

    if (strlen((const char *)wifi_config.ap.password) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP enabled with SSID: %s", wifi_config.ap.ssid);
}

static void _wifi_button_long_press_event_handler(void *handler_arg, esp_event_base_t base, int32_t id, void *event_data)
{
    ESP_LOGI(TAG, "Long press detected, switching to AP mode...");
    ESP_LOGI(TAG, "Prio: %d, Core: %d", uxTaskPriorityGet(NULL), xPortGetCoreID());

    if (system_state.wifi_state == WIFI_STATE_TRANSITION || system_state.wifi_state == WIFI_STATE_NONE)
    {
        ESP_LOGI(TAG, "Already in transition state, ignoring long press event.");
        return;
    }

    if (system_state.wifi_state == WIFI_STATE_NONE)
    {
        system_state.wifi_state = WIFI_STATE_TRANSITION;
        ESP_LOGI(TAG, "Initializing WiFi in STA mode...");
        wifi_sta_init();                          // Initialize WiFi in STA mode
        system_state.wifi_state = WIFI_STATE_STA; // Update state to STA
        ESP_LOGI(TAG, "WiFi initialized in STA mode successfully.");
    }
    else if (system_state.wifi_state == WIFI_STATE_AP)
    {
        system_state.wifi_state = WIFI_STATE_TRANSITION;
        ESP_LOGI(TAG, "Switching to STA mode...");
        esp_wifi_stop();   // Stop WiFi
        esp_wifi_deinit(); // Deinitialize WiFi

        wifi_sta_init();                          // Initialize WiFi in STA mode
        system_state.wifi_state = WIFI_STATE_STA; // Update state to STA
        ESP_LOGI(TAG, "Switched to STA mode successfully.");
    }
    else
    {
        system_state.wifi_state = WIFI_STATE_TRANSITION;
        ESP_LOGI(TAG, "Switching to AP mode...");
        esp_wifi_stop();   // Stop WiFi
        esp_wifi_deinit(); // Deinitialize WiFi

        wifi_ap_init();                          // Initialize WiFi in AP mode
        system_state.wifi_state = WIFI_STATE_AP; // Update state to AP
        ESP_LOGI(TAG, "Switched to AP mode successfully.");
    }
}

void wifi_initialize()
{
    // Initialize TCP/IP stack
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    events_subscribe(EVENT_BUTTON_LONG_PRESS, _wifi_button_long_press_event_handler, NULL);
}

void wifi_connect_sta()
{
    if (system_state.wifi_state != WIFI_STATE_NONE)
    {
        ESP_LOGI(TAG, "WiFi already initialized, skipping initialization.");
        return;
    }
    system_state.wifi_state = WIFI_STATE_TRANSITION;
    ESP_LOGI(TAG, "Connecting to WiFi in STA mode...");
    wifi_sta_init();
    system_state.wifi_state = WIFI_STATE_STA; // Update state to STA
    ESP_LOGI(TAG, "WiFi connected in STA mode successfully.");
}

void wifi_connect_ap()
{
    if (system_state.wifi_state != WIFI_STATE_NONE)
    {
        ESP_LOGI(TAG, "WiFi already initialized, skipping initialization.");
        return;
    }
    system_state.wifi_state = WIFI_STATE_TRANSITION;
    ESP_LOGI(TAG, "Connecting to WiFi in AP mode...");
    wifi_ap_init();
    system_state.wifi_state = WIFI_STATE_AP; // Update state to AP
    ESP_LOGI(TAG, "WiFi connected in AP mode successfully.");
}
