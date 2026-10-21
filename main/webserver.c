#include "webserver.h"
#include "recipes.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdlib.h>
volatile int bestellter_drink = 0;
volatile int aktueller_status = 0;
volatile int skip_requested = 0;
volatile int eis_auswahl = 0;

char web_drink_name[32] = "";
char web_ing_name[32] = "";
volatile float web_current_weight = 0.0;
volatile float web_target_weight = 0.0;

static const char *HTML_TOP =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no'>"
    "<title>Barkeeper</title>"
    "<style>"
    ":root { --bg: #0d1117; --primary: #f39c12; --surface: #161b22; --text: #c9d1d9; --border: #30363d; }"
    "body { font-family: -apple-system, sans-serif; background: var(--bg); color: var(--text); text-align: center; margin: 0; padding: 20px; }"
    "h1 { color: var(--primary); font-weight: 300; letter-spacing: 3px; margin-bottom: 30px; font-size: 24px; }"
    ".screen { display: none; max-width: 400px; margin: 0 auto; animation: fadeIn 0.3s; }"
    "@keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }"
    ".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; }"
    ".btn { background: var(--surface); color: var(--text); border: 1px solid var(--border); padding: 20px 10px; border-radius: 12px; font-size: 15px; font-weight: 600; cursor: pointer; transition: all 0.1s; box-shadow: 0 4px 6px rgba(0,0,0,0.2); }"
    ".btn:active { transform: scale(0.95); background: var(--primary); color: #000; border-color: var(--primary); }"
    ".btn-large { grid-column: span 2; padding: 15px; font-size: 18px; }"
    ".btn-outline { background: transparent; border: 2px solid var(--primary); color: var(--primary); }"
    ".progress-bg { width: 100%; background: var(--surface); border-radius: 15px; height: 25px; margin: 25px 0; border: 1px solid var(--border); overflow: hidden; position: relative; }"
    ".progress-fill { height: 100%; background: var(--primary); width: 0%; transition: width 0.4s ease-out; }"
    ".val { font-size: 45px; color: var(--primary); font-weight: bold; margin: 10px 0; }"
    ".sub { font-size: 18px; color: #8b949e; }"
    ".cheat-btn { position: fixed; bottom: 15px; right: 15px; background: #e74c3c; color: white; border: none; padding: 12px; border-radius: 50%; opacity: 0.3; font-size: 12px; font-weight: bold; z-index: 99; }"
    "</style>"
    "<script>"
    "setInterval(() => {"
    "  fetch('/status').then(r => r.json()).then(d => {"
    "    document.querySelectorAll('.screen').forEach(s => s.style.display = 'none');"
    "    if(d.st === 2) document.getElementById('menu').style.display = 'block';"
    "    else if(d.st === 3) document.getElementById('ice_screen').style.display = 'block';"
    "    else {"
    "       let scr = document.getElementById('status_screen');"
    "       scr.style.display = 'block';"
    "       let txt = document.getElementById('status_text');"
    "       if(d.st === 1) txt.innerHTML = '<div class=\"sub\">Bitte Glas aufstellen</div><div class=\"val\">' + d.cw.toFixed(1) + ' g</div>';"
    "       else if(d.st === 4) {"
    "           let pct = Math.min(100, Math.max(0, (d.cw / d.tw) * 100));"
    "           txt.innerHTML = '<div class=\"sub\">Mixe ' + d.dn + '</div>'"
    "           + '<div style=\"font-size:22px; margin-top:10px; color:#fff;\">' + d.in + '</div>'"
    "           + '<div class=\"progress-bg\"><div class=\"progress-fill\" style=\"width:' + pct + '%\"></div></div>'"
    "           + '<div class=\"val\">' + d.cw.toFixed(1) + '<span style=\"font-size:20px;color:#8b949e\"> / ' + d.tw.toFixed(1) + 'g</span></div>';"
    "       }"
    "       else if(d.st === 5) txt.innerHTML = '<div class=\"val\" style=\"font-size:35px;\">Cheers!</div><div class=\"sub\">Dein Drink ist fertig</div>';"
    "       else if(d.st === 6) txt.innerHTML = '<div class=\"val\" style=\"font-size:30px; color:#e74c3c\">Glas entnehmen</div>';"
    "       else txt.innerHTML = '<div class=\"sub\">System bereitet vor...</div>';"
    "    }"
    "  });"
    "}, 500);"
    "</script></head><body>"
    "<h1>BARKEEPER GUIDE</h1>"
    "<div id='menu' class='screen'><div class='grid'>";

static const char *HTML_BOTTOM =
    "</div></div>"
    "<div id='ice_screen' class='screen'>"
    "<h2 style='margin-bottom:30px; font-weight:400;'>Eis ins Glas?</h2>"
    "<div class='grid'>"
    "<button class='btn btn-outline btn-large' onclick='fetch(\"/ice?opt=1\")'>Ohne Eis</button>"
    "<button class='btn btn-large' onclick='fetch(\"/ice?opt=2\")'>Mit Eis</button>"
    "</div></div>"
    "<div id='status_screen' class='screen'><div id='status_text'></div></div>"
    "<button class='cheat-btn' onclick='fetch(\"/skip\")'>SKIP</button>"
    "</body></html>";

// Liefert die gesamte Rezeptdatenbank als maschinenlesbares JSON
static esp_err_t api_recipes_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    // Array öffnen
    httpd_resp_send_chunk(req, "[", 1);

    extern const int NUM_DRINKS;
    extern Drink drinks[];
    char buf[256];

    for (int i = 0; i < NUM_DRINKS; i++)
    {
        int len = snprintf(buf, sizeof(buf), "{\"id\":%d,\"name\":\"%s\",\"ingredients\":[", i + 1, drinks[i].name);
        httpd_resp_send_chunk(req, buf, len);

        for (int j = 0; j < drinks[i].num_ingredients; j++)
        {
            // Leere Platzhalter überspringen
            if (strlen(drinks[i].ingredients[j].name) == 0)
                continue;

            len = snprintf(buf, sizeof(buf), "{\"name\":\"%s\",\"weight\":%.1f}%s",
                           drinks[i].ingredients[j].name,
                           drinks[i].ingredients[j].target_weight,
                           // Komma setzen, wenn es nicht die letzte gültige Zutat ist
                           (j < drinks[i].num_ingredients - 1 && strlen(drinks[i].ingredients[j + 1].name) > 0) ? "," : "");
            httpd_resp_send_chunk(req, buf, len);
        }

        len = snprintf(buf, sizeof(buf), "]}%s", (i < NUM_DRINKS - 1) ? "," : "");
        httpd_resp_send_chunk(req, buf, len);
    }

    // Array schließen und Übertragung beenden
    httpd_resp_send_chunk(req, "]", 1);
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    // 1. Statischen Kopfbereich senden
    httpd_resp_send_chunk(req, HTML_TOP, HTTPD_RESP_USE_STRLEN);

    // 2. Tausche malloc gegen ein winziges lokales Array für EINEN Button
    char btn_buf[128];
    extern const int NUM_DRINKS;
    extern Drink drinks[];

    // Schleife sendet jeden Button einzeln als Chunk an das Handy
    for (int i = 0; i < NUM_DRINKS; i++)
    {
        int len = snprintf(btn_buf, sizeof(btn_buf),
                           "<button class='btn' onclick='fetch(\"/make?drink=%d\")'>%s</button>",
                           i + 1, drinks[i].name);
        httpd_resp_send_chunk(req, btn_buf, len);
    }

    // 3. Statischen Fußbereich senden
    httpd_resp_send_chunk(req, HTML_BOTTOM, HTTPD_RESP_USE_STRLEN);

    // 4. Chunked Transfer sauber abschließen
    httpd_resp_send_chunk(req, NULL, 0);

    return ESP_OK;
}

static esp_err_t make_get_handler(httpd_req_t *req)
{
    char buf[100];
    char param[10];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK && httpd_query_key_value(buf, "drink", param, sizeof(param)) == ESP_OK)
    {
        bestellter_drink = atoi(param);
    }
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t ice_get_handler(httpd_req_t *req)
{
    char buf[100];
    char param[10];
    if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK && httpd_query_key_value(buf, "opt", param, sizeof(param)) == ESP_OK)
    {
        eis_auswahl = atoi(param);
    }
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t skip_get_handler(httpd_req_t *req)
{
    skip_requested = 1;
    httpd_resp_send(req, "OK", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// GANZ WICHTIG: Hier schnüren wir das JSON Paket mit den Rezeptdaten!
static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json[250];
    snprintf(json, sizeof(json), "{\"st\":%d, \"dn\":\"%s\", \"in\":\"%s\", \"cw\":%.1f, \"tw\":%.1f}",
             aktueller_status, web_drink_name, web_ing_name, web_current_weight, web_target_weight);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

void start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t u1 = {"/", HTTP_GET, root_get_handler, NULL};
        httpd_register_uri_handler(server, &u1);
        httpd_uri_t u2 = {"/make", HTTP_GET, make_get_handler, NULL};
        httpd_register_uri_handler(server, &u2);
        httpd_uri_t u3 = {"/status", HTTP_GET, status_get_handler, NULL};
        httpd_register_uri_handler(server, &u3);
        httpd_uri_t u4 = {"/skip", HTTP_GET, skip_get_handler, NULL};
        httpd_register_uri_handler(server, &u4);
        httpd_uri_t u5 = {"/ice", HTTP_GET, ice_get_handler, NULL};
        httpd_register_uri_handler(server, &u5);
        httpd_uri_t api_recipes_uri = {"/api/recipes", HTTP_GET, api_recipes_get_handler, NULL};
        httpd_register_uri_handler(server, &api_recipes_uri);
    }
}