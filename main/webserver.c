#include "webserver.h"
#include "esp_http_server.h"
#include "esp_log.h"

volatile int bestellter_drink = 0;
volatile int aktueller_status = 0; // Hier schreibt die app_main immer den aktuellen Status rein!

static const char *TAG = "WEBSERVER";

// 1. Die Hauptseite mit dem schicken Menü
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const char *html_antwort =
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<style>"
        "body { font-family: sans-serif; background-color: #121212; color: #ffffff; text-align: center; padding: 20px; }"
        "h1 { color: #f39c12; }"
        ".btn { background-color: #f39c12; color: #121212; padding: 15px 20px; font-size: 20px; font-weight: bold; text-decoration: none; border-radius: 10px; display: block; margin: 15px auto; width: 80%; }"
        ".btn:active { background-color: #e67e22; }"
        "</style></head><body>"
        "<h1>Barkeeper Guide</h1>"
        "<p>Waehle deinen Drink:</p>"
        "<a href='/make?drink=1' class='btn'>Gin Tonic</a>"
        "<a href='/make?drink=2' class='btn'>Rum Cola</a>"
        "<a href='/make?drink=3' class='btn'>Vodka Lemon</a>"
        "</body></html>";

    httpd_resp_send(req, html_antwort, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// 2. Diese Funktion fängt den Knopfdruck ab
static esp_err_t make_get_handler(httpd_req_t *req)
{
    // 1. Herausfinden, welcher Drink (1, 2 oder 3) bestellt wurde
    char buf[100];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
    {
        char param[10];
        if (httpd_query_key_value(buf, "drink", param, sizeof(param)) == ESP_OK)
        {
            // Den Text in eine echte Zahl umwandeln und in den Briefkasten legen!
            bestellter_drink = atoi(param);
            ESP_LOGI(TAG, "==== DRINK NUMMER %d BESTELLT! ====", bestellter_drink);
        }
    }

    // 2. Antwort an das Handy (wie vorher)
    const char *html_antwort =
        "<!DOCTYPE html><html><head>"
        "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
        "<style>"
        "body { font-family: sans-serif; background-color: #121212; color: #fff; text-align: center; padding: 20px; }"
        ".btn { background-color: #f39c12; color: #121212; padding: 15px 20px; font-size: 20px; font-weight: bold; text-decoration: none; border-radius: 10px; display: block; margin: 15px auto; width: 80%; cursor: pointer; border: none; }"
        "#menu, #status_screen { display: none; }" /* Versteckt anfangs alles */
        "</style>"
        "<script>"
        "setInterval(() => {"
        "  fetch('/status').then(response => response.text()).then(state => {"
        "    let stateNum = parseInt(state);"
        "    let menu = document.getElementById('menu');"
        "    let statusScr = document.getElementById('status_screen');"
        "    let statusText = document.getElementById('status_text');"
        "    "
        "    /* Hier übersetzen wir die Zahlen in das, was vorher auf dem LCD stand! */"
        "    /* ACHTUNG: Die Zahlen (2, 4, 5) müssen zu deinen STATE_... Konstanten passen! */"
        "    if(stateNum === 2) { " // Angenommen 2 ist STATE_MENU
        "       menu.style.display = 'block'; statusScr.style.display = 'none';"
        "    } else {"
        "       menu.style.display = 'none'; statusScr.style.display = 'block';"
        "       if(stateNum === 1) statusText.innerText = 'Bitte Glas aufstellen und tarieren...';"
        "       else if(stateNum === 4) statusText.innerText = 'Drink wird gemixt! Bitte warten...';"
        "       else if(stateNum === 5) statusText.innerText = 'Cheers! Dein Drink ist fertig!';"
        "       else if(stateNum === 6) statusText.innerText = 'Bitte Glas entnehmen!';"
        "       else statusText.innerText = 'Maschine bereitet sich vor...';"
        "    }"
        "  });"
        "}, 1000);" // Jede Sekunde (1000ms) abfragen

        "function order(drinkId) {"
        "  fetch('/make?drink=' + drinkId);"
        "}"
        "</script></head><body>"
        "<h1>Barkeeper Guide</h1>"

        "<div id='menu'>"
        "<p>Waehle deinen Drink:</p>"
        "<button class='btn' onclick='order(1)'>Gin Tonic</button>"
        "<button class='btn' onclick='order(2)'>Rum Cola</button>"
        "<button class='btn' onclick='order(3)'>Vodka Lemon</button>"
        "</div>"

        "<div id='status_screen'>"
        "<h2 id='status_text' style='color:#f39c12;'>Verbinde mit Maschine...</h2>"
        "</div>"

        "</body></html>";

    httpd_resp_send(req, html_antwort, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char status_string[10];
    // Wandelt den Status (z.B. 4 für Pouring) in einen Text um
    snprintf(status_string, sizeof(status_string), "%d", aktueller_status);
    httpd_resp_send(req, status_string, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        // URL für die Startseite verknüpfen
        httpd_uri_t uri_root = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = root_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &uri_root);

        // URL für die Bestellung verknüpfen
        httpd_uri_t uri_make = {
            .uri = "/make",
            .method = HTTP_GET,
            .handler = make_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &uri_make);

        httpd_uri_t uri_status = {
            .uri = "/status",
            .method = HTTP_GET,
            .handler = status_get_handler,
            .user_ctx = NULL};
        httpd_register_uri_handler(server, &uri_status);
    }
}