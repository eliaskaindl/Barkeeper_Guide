#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"

// Nur noch die Hardware, die wir wirklich brauchen!
#include "Leds.h"
#include "Scale.h"
#include "recipes.h"
#include "wlan.h"
#include "nvs_flash.h"
#include "webserver.h"

typedef enum
{
    STATE_START = 0,
    STATE_PLACE_GLASS = 1,
    STATE_MENU = 2,
    STATE_POURING = 4,
    STATE_FINISHED = 5,
    STATE_WAIT_REMOVE = 6
} AppState;

// --- Globale Variablen ---
AppState current_state = STATE_START;
int selected_drink_idx = 0;
int current_ingredient_idx = 0;

void setup_hardware(void)
{
    printf("Starte Barkeeper Guide (Smart Edition)...\n");
    init_scale();
    init_matrix();
    vTaskDelay(pdMS_TO_TICKS(100));
}

void handle_state_start(void)
{
    // Wir gehen sofort ins Menü, damit die App bedienbar ist
    current_state = STATE_MENU;
}

void handle_state_menu(void)
{
    // Wir warten auf den Briefkasten vom Webserver!
    if (bestellter_drink > 0)
    {
        printf("Webserver meldet Bestellung: Drink %d\n", bestellter_drink);

        // Drink-Index setzen (Array startet bei 0, Bestellung bei 1)
        selected_drink_idx = bestellter_drink - 1;

        // Briefkasten leeren
        bestellter_drink = 0;

        // Maschine schaltet auf "Glas aufstellen"
        current_state = STATE_PLACE_GLASS;
    }
}

void handle_state_place_glass(void)
{
    float weight = scale_get_weight_gram();

    // AUTO-ERKENNUNG: Sobald mehr als 20g auf der Waage stehen, ist es ein Glas (mit oder ohne Eis)
    if (weight > 20.0)
    {
        printf("Glas erkannt (%.1fg)! Warte auf Beruhigung...\n", weight);
        vTaskDelay(pdMS_TO_TICKS(2000)); // 2 Sekunden warten, bis das Glas ruhig steht

        scale_tara();
        printf("Tariert! Starte Mix-Vorgang...\n");

        current_ingredient_idx = 0;
        current_state = STATE_POURING;
    }
}

void handle_state_pouring(void)
{
    Drink *current_drink = &drinks[selected_drink_idx];
    Ingredient *current_ing = &current_drink->ingredients[current_ingredient_idx];

    float target = current_ing->target_weight;
    float weight = scale_get_weight_gram();

    led_set_progress(weight, target, current_ing->r, current_ing->g, current_ing->b);

    static int stable_measurements = 0;

    if (weight >= target)
    {
        stable_measurements++;
        if (stable_measurements >= 2)
        {
            printf("Zutat [%s] voll!\n", current_ing->name);
            clear_led();

            // HARDWARE-RESET FÜR DEN HX711-CHIP
            gpio_set_level(GPIO_NUM_1, 1);
            esp_rom_delay_us(60);
            gpio_set_level(GPIO_NUM_1, 0);
            esp_rom_delay_us(10);

            vTaskDelay(pdMS_TO_TICKS(2000)); // 2 Sekunden Pause zwischen Zutaten

            current_ingredient_idx++;
            if (current_ingredient_idx >= current_drink->num_ingredients)
            {
                current_state = STATE_FINISHED;
            }
            else
            {
                scale_tara(); // Für die nächste Zutat wieder auf 0 setzen
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            stable_measurements = 0;
        }
    }
    else
    {
        stable_measurements = 0;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
}

void handle_state_finished(void)
{
    printf("Drink fertig!\n");
    // Grünes Licht als optisches Signal (statt Display)
    led_set_progress(100, 100, 0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(3000));
    clear_led();

    current_state = STATE_WAIT_REMOVE;
}

void handle_state_wait_remove(void)
{
    float current_weight = scale_get_weight_gram();

    // Da wir mit Glas tariert haben, wird das Gewicht extrem negativ, wenn man das Glas wegnimmt
    if (current_weight < -20.0)
    {
        printf("Glas wurde entfernt! System setzt sich zurueck...\n");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Kurz warten

        scale_tara();               // Waage wieder auf echten Nullpunkt setzen
        current_state = STATE_MENU; // Zurück ins Hauptmenü
    }
    vTaskDelay(pdMS_TO_TICKS(500));
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    start_wlan_hotspot();
    start_webserver();
    setup_hardware();

    while (1)
    {
        // GANZ WICHTIG: Hier updaten wir den Status für das Handy!
        aktueller_status = current_state;

        switch (current_state)
        {
        case STATE_START:
            handle_state_start();
            break;
        case STATE_PLACE_GLASS:
            handle_state_place_glass();
            break;
        case STATE_MENU:
            handle_state_menu();
            break;
        case STATE_POURING:
            handle_state_pouring();
            break;
        case STATE_FINISHED:
            handle_state_finished();
            break;
        case STATE_WAIT_REMOVE:
            handle_state_wait_remove();
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}