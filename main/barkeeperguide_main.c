#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "Leds.h"
#include "Scale.h"
#include "recipes.h"
#include "wlan.h"
#include "nvs_flash.h"
#include "webserver.h"

#define USE_SCALE 0

typedef enum
{
    STATE_START = 0,
    STATE_PLACE_GLASS = 1,
    STATE_MENU = 2,
    STATE_ICE_OPTION = 3,
    STATE_POURING = 4,
    STATE_FINISHED = 5,
    STATE_WAIT_REMOVE = 6
} AppState;

AppState current_state = STATE_START;
int selected_drink_idx = 0;
int current_ingredient_idx = 0;
float fake_weight = 0.0;
float recipe_scale_factor = 1.0;
extern volatile int cancel_requested;

void setup_hardware(void)
{
    init_matrix();
#if USE_SCALE == 1
    init_scale();

    // Sicherstellen, dass GPIO 1 als Ausgang für den HX711 Reset konfiguriert ist
    gpio_set_direction(GPIO_NUM_1, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NUM_1, 0);

    // KRITISCHER FIX: Initiale Nullung ohne Glas, damit das Leergewicht nicht als Glas erkannt wird!
    printf("Fuehre System-Tarieren beim Booten durch...\n");
    scale_auto_tara();
#else
    printf("SIMULATION MODE: Hardware-Initialisierung wird uebersprungen.\n");
#endif
}

void handle_state_start(void)
{
    current_state = STATE_MENU;
}

void handle_state_menu(void)
{
    if (bestellter_drink > 0)
    {
        selected_drink_idx = bestellter_drink - 1;
        bestellter_drink = 0;
        eis_auswahl = 0;
        current_state = STATE_ICE_OPTION;
    }
}

void handle_state_ice_option(void)
{
    if (eis_auswahl > 0)
    {
        if (eis_auswahl == 1)
        {
            recipe_scale_factor = 1.0;
            printf("Option 'Ohne Eis' gewaehlt (Faktor 1.0).\n");
        }
        else if (eis_auswahl == 2)
        {
            // Da das aktuelle Web-Interface keinen Zwischenschritt für das händische
            // Einfüllen und Wiegen des Eises hat, nutzen wir einen sicheren Schätzwert.
            // Sobald du das Web-UI um einen "Eis einfüllen" Screen erweiterst,
            // kann hier wieder die dynamische Wiegung rein.
            recipe_scale_factor = 0.8;
            printf("Option 'Mit Eis' gewaehlt (Faktor 0.8).\n");
        }

        eis_auswahl = 0;
        current_state = STATE_PLACE_GLASS; // Direkt weiter zum Glas aufstellen
    }
}

void handle_state_place_glass(void)
{
#if USE_HARDWARE == 1
    float weight = scale_get_weight_gram();
    web_current_weight = weight; // Live-Gewicht ans Handy senden

    if (weight > 20.0)
    {
        printf("Glas erkannt (%.1fg)! Warte auf Beruhigung...\n", weight);
        vTaskDelay(pdMS_TO_TICKS(2000)); // Beruhigungszeit

        scale_tara(); // Genau EINMAL tarieren, wenn das Glas steht
        printf("Tariert! Starte Mix-Vorgang...\n");

        current_ingredient_idx = 0;
        current_state = STATE_POURING;
    }
#else
    // SIMULATION: Automatischer Timer anstelle von echtem Gewicht
    static int sim_wait_counter = 0;
    sim_wait_counter++;

    // Bei 10ms Delay in der main-Schleife sind 200 Durchläufe ca. 2 Sekunden.
    // Alternativ kann man weiterhin sofort mit dem Skip-Button überspringen.
    if (sim_wait_counter > 200 || skip_requested == 1)
    {
        printf("SIMULATION: Glas fiktiv erkannt! Starte Mix-Vorgang...\n");
        fake_weight = 0.0;
        current_ingredient_idx = 0;
        current_state = STATE_POURING;

        skip_requested = 0;
        sim_wait_counter = 0; // Zähler für den nächsten Drink zurücksetzen
    }
#endif
}

void handle_state_pouring(void)
{
    Drink *current_drink = &drinks[selected_drink_idx];
    Ingredient *current_ing = &current_drink->ingredients[current_ingredient_idx];
    float target = current_ing->target_weight * recipe_scale_factor;

#if USE_SCALE == 1
    float weight = scale_get_weight_gram();
#else
    // SIMULATION: Fließender Anstieg durch winzige Schritte.
    // Bei ca. 100 Schleifendurchläufen pro Sekunde bedeuten +0.15g etwa 15g pro Sekunde.
    fake_weight += 0.15;
    float weight = fake_weight;
#endif

    web_current_weight = weight; // Live-Update für das Web-Interface
    led_set_progress(weight, target, current_ing->r, current_ing->g, current_ing->b);

    static int stable_measurements = 0;

    if (weight >= target)
    {
        stable_measurements++;
        if (stable_measurements >= 2)
        {
            clear_led();

#if USE_SCALE == 1
            // HX711 Hardware-Reset
            gpio_set_level(GPIO_NUM_1, 1);
            esp_rom_delay_us(60);
            gpio_set_level(GPIO_NUM_1, 0);
            esp_rom_delay_us(10);
#endif

            printf("Zutat %s fertig!\n", current_ing->name);
            vTaskDelay(pdMS_TO_TICKS(1500));
            current_ingredient_idx++;

            if (current_ingredient_idx >= current_drink->num_ingredients)
            {
                current_state = STATE_FINISHED;
            }
            else
            {
#if USE_SCALE == 1
                scale_tara(); // Nur tarieren für die NÄCHSTE Zutat
#else
                fake_weight = 0.0;
#endif
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
            stable_measurements = 0;
        }
    }
    else
    {
        stable_measurements = 0;
    }
}

void handle_state_finished(void)
{
    printf("Drink fertig! Bitte entnehmen.\n");

#if USE_SCALE == 1
    led_set_progress(100, 100, 0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(3000));
    clear_led();
#else
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif

    current_state = STATE_WAIT_REMOVE;
}

void handle_state_wait_remove(void)
{
#if USE_HARDWARE == 1
    float current_weight = scale_get_weight_gram();
    web_current_weight = current_weight;

    // Da die Waage mit Glas auf 0.0g tariert wurde, fällt das Gewicht ins Minus
    if (current_weight < -20.0)
    {
        printf("Glas entfernt. System setzt sich zurueck...\n");
        vTaskDelay(pdMS_TO_TICKS(1500));

        scale_tara();
        current_state = STATE_MENU;
    }
#else
    // SIMULATION: Automatischer Timer für die Glas-Entnahme
    static int sim_remove_counter = 0;
    sim_remove_counter++;

    // Bei 10ms Delay in der main-Schleife sind 400 Durchläufe exakt 4 Sekunden.
    if (sim_remove_counter > 400 || skip_requested == 1)
    {
        printf("SIMULATION: Glas fiktiv entnommen. Zurueck zum Menue...\n");

        // Systemvariablen zwingend nullen
        fake_weight = 0.0;
        skip_requested = 0;
        sim_remove_counter = 0;

        // Ausstehende Web-Bestellungen sicherheitshalber leeren
        eis_auswahl = 0;
        bestellter_drink = 0;

        current_state = STATE_MENU;
    }
#endif
}

void app_main(void)
{
    nvs_flash_init();
    start_wlan_hotspot();
    start_webserver();
    setup_hardware();

    while (1)
    {
        aktueller_status = current_state;

        // Webserver mit Texten und Zielgewichten füttern
        snprintf(web_drink_name, sizeof(web_drink_name), "%s", drinks[selected_drink_idx].name);
        snprintf(web_ing_name, sizeof(web_ing_name), "%s", drinks[selected_drink_idx].ingredients[current_ingredient_idx].name);
        web_target_weight = drinks[selected_drink_idx].ingredients[current_ingredient_idx].target_weight * recipe_scale_factor;

        // ACHTUNG: web_current_weight wird jetzt dynamisch in den State-Handlern gefüllt,
        // damit das Handy bei USE_SCALE == 1 echte Daten sieht!

        // Cheat-Logik (ausschließlich für die Web-Buttons / Simulation)
        if (skip_requested == 1)
        {
            skip_requested = 0;
            if (current_state == STATE_PLACE_GLASS)
            {
                current_state = STATE_POURING;
                current_ingredient_idx = 0;
                fake_weight = 0.0;
            }
            else if (current_state == STATE_POURING)
            {
                fake_weight = web_target_weight;
            }
            else if (current_state == STATE_FINISHED)
            {
                current_state = STATE_WAIT_REMOVE;
            }
            else if (current_state == STATE_WAIT_REMOVE)
            {
                current_state = STATE_MENU;
                fake_weight = 0.0;
            }
        }

        // Die harte Abbruch-Logik
        if (cancel_requested == 1)
        {
            cancel_requested = 0;
            printf("NOT-AUS: Vorgang durch Web-Interface abgebrochen!\n");

            clear_led();
            recipe_scale_factor = 1.0;
            fake_weight = 0.0;

            // NEU: Zwingender Reset der ausstehenden Web-Eingaben!
            eis_auswahl = 0;
            bestellter_drink = 0;

            current_state = STATE_MENU;
        }

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
        case STATE_ICE_OPTION:
            handle_state_ice_option();
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

        // Gibt FreeRTOS die nötige Luft, um den Webserver-Task nicht abzuwürgen
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}