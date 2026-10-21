#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "Leds.h"
#include "Scale.h"
#include "recipes.h"
#include "wlan.h"
#include "nvs_flash.h"
#include "webserver.h"

#define USE_HARDWARE 0

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

void setup_hardware(void)
{
#if USE_HARDWARE == 1
    init_scale();
    init_matrix();
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
        eis_auswahl = 0;                  // Zurücksetzen
        current_state = STATE_ICE_OPTION; // Zuerst zum Eis-Menü!
    }
}

void handle_state_ice_option(void)
{
    if (eis_auswahl > 0)
    {
        if (eis_auswahl == 1)
        {
            // OHNE EIS: Volles Rezept (100% Menge)
            recipe_scale_factor = 1.0;
            printf("Option 'Ohne Eis' gewaehlt.\n");
        }
        else if (eis_auswahl == 2)
        {
            // MIT EIS:
#if USE_HARDWARE == 1
            // --- ECHTE HARDWARE ---
            // 1. Gewicht des Eises auf der Waage ablesen
            float ice_weight = scale_get_weight_gram();
            if (ice_weight < 0)
            {
                ice_weight = 0;
            }

            // 2. Gesamtgewicht des unskalierten Original-Rezepts berechnen
            Drink *current_drink = &drinks[selected_drink_idx];
            float total_recipe_weight = 0.0;
            for (int i = 0; i < current_drink->num_ingredients; i++)
            {
                total_recipe_weight += current_drink->ingredients[i].target_weight;
            }

            // 3. Verfügbaren Platz berechnen (ausgehend von z.B. 300g Max-Glasvolumen)
            float max_glass_volume = 300.0;
            if (ice_weight < max_glass_volume)
            {
                float available_space = max_glass_volume - (ice_weight * 1.1); // Factor 1.1 für Eisvolumen
                recipe_scale_factor = available_space / total_recipe_weight;

                if (recipe_scale_factor > 1.0)
                {
                    recipe_scale_factor = 1.0;
                }
            }
            else
            {
                recipe_scale_factor = 0.1; // Notfall-Minimalmenge bei extrem viel Eis
            }

            printf("Eis gewogen: %.1fg -> Skalierungsfaktor: %.2f\n", ice_weight, recipe_scale_factor);

            // 4. Waage für die folgenden Zutaten wieder auf 0 setzen
            scale_tara();

#else
            // --- SIMULATION ---
            // Fester Testwert für das Rezept mit Eis
            recipe_scale_factor = 0.8;
            printf("SIMULATION: Option 'Mit Eis' gewaehlt (Faktor 0.8).\n");
#endif
        }

        // Briefkasten leeren und in den nächsten Zustand wechseln
        eis_auswahl = 0;
        current_state = STATE_PLACE_GLASS;
    }
}

void handle_state_place_glass(void)
{
#if USE_HARDWARE == 1
    // --- ECHTER HARDWARE CODE ---
    float weight = scale_get_weight_gram();

    if (weight > 20.0)
    {
        printf("Glas erkannt (%.1fg)! Warte auf Beruhigung...\n", weight);
        vTaskDelay(pdMS_TO_TICKS(2000));

        scale_tara();
        printf("Tariert! Starte Mix-Vorgang...\n");

        current_ingredient_idx = 0;
        current_state = STATE_POURING;
    }

#else
    // --- SIMULATIONS CODE ---
    if (skip_requested == 1)
    { // In der Simulation überspringen wir das per Button
        printf("SIMULATION: Glas erkannt und tariert!\n");
        fake_weight = 0.0;
        current_ingredient_idx = 0;
        current_state = STATE_POURING;
        skip_requested = 0;
    }
#endif
}
void handle_state_pouring(void)
{
    Drink *current_drink = &drinks[selected_drink_idx];
    Ingredient *current_ing = &current_drink->ingredients[current_ingredient_idx];
    float target = current_ing->target_weight * recipe_scale_factor;

#if USE_HARDWARE == 1
    // --- ECHTER HARDWARE CODE ---
    float weight = scale_get_weight_gram();
    led_set_progress(weight, target, current_ing->r, current_ing->g, current_ing->b);
#else
    // --- SIMULATIONS CODE ---
    fake_weight += 2.0;
    float weight = fake_weight;
#endif

    // --- GEMEINSAME LOGIK (Filter & Abschluss) ---
    // Diese Logik gilt für beide Fälle!
    static int stable_measurements = 0;

    if (weight >= target)
    {
        stable_measurements++;
        if (stable_measurements >= 2)
        {
            clear_led();

#if USE_HARDWARE == 1
            // HX711 Reset brauchen wir nur bei echter Hardware
            gpio_set_level(GPIO_NUM_1, 1);
            esp_rom_delay_us(60);
            gpio_set_level(GPIO_NUM_1, 0);
            esp_rom_delay_us(10);
#endif

            vTaskDelay(pdMS_TO_TICKS(1500));
            current_ingredient_idx++;

            if (current_ingredient_idx >= current_drink->num_ingredients)
            {
                current_state = STATE_FINISHED;
            }
            else
            {
#if USE_HARDWARE == 1
                scale_tara();
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

    vTaskDelay(pdMS_TO_TICKS(200));
}

void handle_state_finished(void)
{
    printf("Drink fertig!\n");

#if USE_HARDWARE == 1
    // --- ECHTE HARDWARE ---
    // Gruenes Licht als optisches Signal für den Nutzer
    led_set_progress(100, 100, 0, 255, 0);
    vTaskDelay(pdMS_TO_TICKS(3000));
    clear_led();
#else
    // --- SIMULATION ---
    // Kurze Verzögerung für die optische Rückmeldung am Handy
    vTaskDelay(pdMS_TO_TICKS(1000));
#endif

    // Automatisch in den Warteraum für die Glas-Entnahme wechseln
    current_state = STATE_WAIT_REMOVE;
}

void handle_state_wait_remove(void)
{
#if USE_HARDWARE == 1
    // --- ECHTE HARDWARE ---
    float current_weight = scale_get_weight_gram();

    // Da die Waage vor dem Mixen mit Glas auf 0.0g tariert wurde,
    // fällt das Gewicht stark ins Minus, sobald das Glas abgehoben wird.
    if (current_weight < -20.0)
    {
        printf("Glas wurde entfernt (Gewicht: %.1fg). System setzt sich zurueck...\n", current_weight);
        vTaskDelay(pdMS_TO_TICKS(1500));

        scale_tara();               // Waage wieder auf den echten Nullpunkt setzen
        current_state = STATE_MENU; // Zurueck ins Hauptmenue
    }
    vTaskDelay(pdMS_TO_TICKS(500));

#else
    // --- SIMULATION ---
    // In der Simulation warten wir entweder auf den Skip-Button oder setzen zurueck
    if (skip_requested == 1)
    {
        printf("SIMULATION: Glas entnommen.\n");
        fake_weight = 0.0;
        skip_requested = 0;
        current_state = STATE_MENU;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
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

        // Webserver mit Texten füttern
        snprintf(web_drink_name, sizeof(web_drink_name), "%s", drinks[selected_drink_idx].name);
        snprintf(web_ing_name, sizeof(web_ing_name), "%s", drinks[selected_drink_idx].ingredients[current_ingredient_idx].name);
        web_target_weight = drinks[selected_drink_idx].ingredients[current_ingredient_idx].target_weight * recipe_scale_factor;
        web_current_weight = fake_weight;

        // Cheat-Logik
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
                fake_weight = web_target_weight; /* Zutat voll machen */
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
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}