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

// Dynamische Generierung der Hauptseite inklusive aller Rezepte
static esp_err_t root_get_handler(httpd_req_t *req)
{
    // Großer Puffer für das dynamische HTML
    char *html_buf = malloc(4096);
    if (html_buf == NULL)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // 1. HTML Header & Styles
    int len = snprintf(html_buf, 4096,
                       "<!DOCTYPE html><html><head>"
                       "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
                       "<style>"
                       "body { font-family: sans-serif; background-color: #121212; color: #fff; text-align: center; padding: 20px; }"
                       ".btn { background-color: #f39c12; color: #121212; padding: 15px 20px; font-size: 18px; font-weight: bold; text-decoration: none; border-radius: 10px; display: block; margin: 10px auto; width: 85%%; cursor: pointer; border: none; }"
                       ".btn-outline { background-color: transparent; color: #f39c12; border: 2px solid #f39c12; }"
                       "#menu, #ice_screen, #status_screen { display: none; }"
                       "</style>"
                       "<script>"
                       "setInterval(() => {"
                       "  fetch('/status').then(r => r.json()).then(d => {"
                       "    let menu = document.getElementById('menu');"
                       "    let ice = document.getElementById('ice_screen');"
                       "    let statScr = document.getElementById('status_screen');"
                       "    let statTxt = document.getElementById('status_text');"
                       "    menu.style.display = 'none'; ice.style.display = 'none'; statScr.style.display = 'none';"
                       "    if(d.st === 2) { menu.style.display = 'block'; }"
                       "    else if(d.st === 3) { ice.style.display = 'block'; }"
                       "    else {"
                       "       statScr.style.display = 'block';"
                       "       if(d.st === 1) statTxt.innerHTML = 'Bitte Glas aufstellen...<br><br><span style=\"font-size:30px;color:#f39c12;\">' + d.cw.toFixed(1) + ' g</span>';"
                       "       else if(d.st === 4) {"
                       "           statTxt.innerHTML = 'Mixe: <b style=\"color:#f39c12;\">' + d.dn + '</b><br><br>' +"
                       "           '<span style=\"color:#aaa;\">Aktuelle Zutat:</span><br><b>' + d.in + '</b><br><br>' +"
                       "           '<span style=\"font-size:45px; color:#f39c12;\">' + d.cw.toFixed(1) + '</span><span style=\"font-size:25px;\"> / ' + d.tw.toFixed(1) + ' g</span>';"
                       "       }"
                       "       else if(d.st === 5) statTxt.innerHTML = 'Cheers!<br>Dein Drink ist fertig!';"
                       "       else if(d.st === 6) statTxt.innerHTML = 'Bitte Glas entnehmen!';"
                       "       else statTxt.innerHTML = 'Bereite vor...';"
                       "    }"
                       "  });"
                       "}, 500);"
                       "</script></head><body>"
                       "<h1>Barkeeper Guide</h1>"
                       "<div id='menu'><p>Waehle deinen Drink:</p>");

    // 2. Schleife: Erzeugt für JEDEN Drink in recipes.c automatisch einen Button!
    extern const int NUM_DRINKS;
    extern Drink drinks[];
    for (int i = 0; i < NUM_DRINKS; i++)
    {
        len += snprintf(html_buf + len, 4096 - len,
                        "<button class='btn' onclick='fetch(\"/make?drink=%d\")'>%s</button>",
                        i + 1, drinks[i].name);
    }

    // 3. HTML Abschluss & Eis-Screen
    snprintf(html_buf + len, 4096 - len,
             "</div>"
             "<div id='ice_screen'>"
             "<h2>Eis ins Glas?</h2>"
             "<button class='btn btn-outline' onclick='fetch(\"/ice?opt=1\")'>Ohne Eis</button>"
             "<button class='btn' onclick='fetch(\"/ice?opt=2\")'>Mit Eis</button>"
             "</div>"
             "<div id='status_screen'><h2 id='status_text'>Verbinde...</h2></div>"
             "<button onclick='fetch(\"/skip\")' style='position:fixed; bottom:10px; right:10px; opacity:0.3; background:red; color:white; border:none; padding:10px; border-radius:5px; z-index:9999;'>Skip</button>"
             "</body></html>");

    httpd_resp_send(req, html_buf, HTTPD_RESP_USE_STRLEN);
    free(html_buf); // Speicher wieder freigeben
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
    }
}