#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include <string.h>

#define BARKEEPER_WLAN_NAME "Barkeeper_Guide"
#define BARKEEPER_WLAN_PASS "cocktail123"

void start_wlan_hotspot(void)
{
    // Netzwerk-Schnittstelle initialisieren
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t loop_err = esp_event_loop_create_default();
    if (loop_err != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(loop_err);
    };
    esp_netif_create_default_wifi_ap();

    // Standard-WLAN-Treiber laden
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Hotspot (Access Point) konfigurieren
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = BARKEEPER_WLAN_NAME,
            .ssid_len = strlen(BARKEEPER_WLAN_NAME),
            .password = BARKEEPER_WLAN_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK},
    };

    // WLAN-Modus setzen und Hardware starten
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}