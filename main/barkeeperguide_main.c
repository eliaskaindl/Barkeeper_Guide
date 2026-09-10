#include <stdio.h>
#include <inttypes.h>
#include <math.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "Button.h"
#include "Leds.h"
#include "Scale.h"
#include "lcd1602.h"

#define MAX_GLASS_VOLUME 300.0   // Maximales Fassungsvermögen deines Glase
float recipe_scale_factor = 1.0; // Multiplikator für die Zutaten

typedef enum
{
    STATE_START,
    STATE_PLACE_GLASS,
    STATE_MENU,
    STATE_ICE_OPTION,
    STATE_POURING,
    STATE_FINISHED,
    STATE_WAIT_REMOVE
} AppState;

// --- Datenstrukturen für Drinks ---
typedef struct
{
    char name[20];
    float target_weight;
    uint32_t r;
    uint32_t g;
    uint32_t b;
} Ingredient;

typedef struct
{
    char name[20];
    int num_ingredients;
    Ingredient ingredients[3];
} Drink;

// --- Rezept-Datenbank ---
Drink drinks[] = {
    {"Aperol Spritz", 3, {
                             {"Aperol", 60.0, 150, 40, 0},      // Orange
                             {"Prosecco", 90.0, 120, 120, 40},  // Hellgelb
                             {"Mineralwasser", 30.0, 0, 0, 150} // Blau
                         }},
    {"Gin Tonic", 2, {
                         {"Gin", 40.0, 100, 100, 100},        // Weiß/Klar
                         {"Tonic Water", 120.0, 0, 100, 120}, // Cyan/Türkis
                         {"", 0.0, 0, 0, 0}                   // Platzhalter
                     }},
    {"Wodka O", 2, {
                       {"Wodka", 40.0, 100, 100, 100},     // Weiß/Klar
                       {"Orangensaft", 120.0, 150, 80, 0}, // Orange
                       {"", 0.0, 0, 0, 0}                  // Platzhalter
                   }},
    {"Tequila Sunrise", 3, {
                               {"Tequila", 40.0, 100, 100, 100},   // Weiß/Klar
                               {"Orangensaft", 100.0, 150, 80, 0}, // Orange
                               {"Grenadine", 15.0, 150, 0, 0}      // Rot
                           }},
    {"Cuba Libre", 3, {
                          {"Rum", 40.0, 120, 60, 20},         // Braun/Amber
                          {"Cola", 120.0, 50, 20, 0},         // Dunkelbraun
                          {"Limettensaft", 15.0, 50, 150, 20} // Hellgrün
                      }},
    {"Margarita", 3, {
                         {"Tequila", 50.0, 100, 100, 100},   // Weiß/Klar
                         {"Triple Sec", 20.0, 120, 100, 50}, // Leichtes Gelb
                         {"Limettensaft", 20.0, 50, 150, 20} // Hellgrün
                     }}};

// WICHTIG: Die Anzahl der Drinks muss jetzt auf 6 erhöht werden!
const int NUM_DRINKS = 6;

// --- Globale Variablen ---
AppState current_state = STATE_START;
bool redraw_display = true;
int selected_drink_idx = 0;
int current_ingredient_idx = 0;
float last_displayed_weight = -999.0; // Hilfsvariable gegen Display-Flackern beim Gießen

// --- Das Hauptprogramm ---
void app_main(void)
{
    printf("Starte Barkeeper Guide...\n");

    // 1. Hardware initialisieren
    button_init();
    init_scale();
    init_matrix();

    vTaskDelay(pdMS_TO_TICKS(100)); // Warten für stabile Spannungen

    // --- DISPLAY KONFIGURIEREN & STARTEN ---
    if (lcd1602_init_default() == ESP_OK)
    {
        printf("Display erfolgreich gestartet!\n");
    }
    else
    {
        printf("Fehler beim Display-Start!\n");
    }

    while (1)
    {
        switch (current_state)
        {

        // 1. STARTBILDSCHIRM
        case STATE_START:
            if (redraw_display)
            {
                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));

                // Zeile 1: Deko-Rand oben
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("====================");

                // Zeile 2: Titel zentriert (bei 20 Zeichen Display)
                lcd1602_move_cursor(0, 1);
                lcd1602_write_string("  BARKEEPER  GUIDE  ");

                // Zeile 3: Deko-Rand unten
                lcd1602_move_cursor(0, 2);
                lcd1602_write_string("====================");

                // Zeile 4: Handlungsaufforderung zentriert
                lcd1602_move_cursor(0, 3);
                lcd1602_write_string(" > R-Knopf: Start < ");

                redraw_display = false;
            }

            if (is_right_clicked())
            {
                current_state = STATE_PLACE_GLASS;
                redraw_display = true;
            }
            break;

        // 2. GLAS AUFSTELLEN UND TARIEREN
        case STATE_PLACE_GLASS:
            if (redraw_display)
            {
                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));

                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("=== VORBEREITUNG ===");

                lcd1602_move_cursor(0, 1);
                lcd1602_write_string("1. Glas aufstellen");

                lcd1602_move_cursor(0, 2);
                lcd1602_write_string("2. Tara druecken");

                lcd1602_move_cursor(0, 3);
                lcd1602_write_string("L:Tara    | R:Weiter");

                redraw_display = false;
            }

            if (is_left_clicked())
            {
                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("Waage tariert...");
                lcd1602_move_cursor(0, 1);
                lcd1602_write_string("R-Knopf: Weiter ");

                scale_tara();
                printf("Waage tariert!\n");
                vTaskDelay(pdMS_TO_TICKS(1500));
                redraw_display = true;
            }

            if (is_right_clicked())
            {
                current_state = STATE_MENU;
                selected_drink_idx = 0;
                redraw_display = true;
            }
            break;

        // 3a. DRINK AUSWAHL (MENÜ)
        case STATE_MENU:
            if (redraw_display)
            {
                char drink_text[30];
                snprintf(drink_text, sizeof(drink_text), "> %s", drinks[selected_drink_idx].name);

                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));

                // Zeile 1: Titel
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("=== DRINK MENUE ===");

                // Zeile 2: Info
                lcd1602_move_cursor(0, 1);
                lcd1602_write_string("Aktuelle Wahl:");

                // Zeile 3: Der Drink
                lcd1602_move_cursor(0, 2);
                lcd1602_write_string(drink_text);

                // Zeile 4: Steuerung
                lcd1602_move_cursor(0, 3);
                lcd1602_write_string("L:Start  |  R:Next");

                redraw_display = false;
            }

            if (is_right_clicked())
            {
                selected_drink_idx++;
                if (selected_drink_idx >= NUM_DRINKS)
                    selected_drink_idx = 0;
                redraw_display = true;
            }

            if (is_left_clicked())
            {
                current_state = STATE_ICE_OPTION;
                current_ingredient_idx = 0;
                redraw_display = true;
            }
            break;

        // 3b. EISWÜRFEL ABFRAGE
        case STATE_ICE_OPTION:
            if (redraw_display)
            {
                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("Eis ins Glas?");
                lcd1602_move_cursor(0, 1);
                lcd1602_write_string("L:Nein | R:Ja");

                printf("DISPLAY: Eis abfragen | L: Nein | R: Ja\n");
                redraw_display = false;
            }

            // Option 1: Kein Eis gewollt (L-Knopf)
            if (is_left_clicked())
            {
                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("Bereite vor...");

                scale_tara();
                last_displayed_weight = -999.0;
                recipe_scale_factor = 1.0;

                current_state = STATE_POURING;
                redraw_display = true;
            }

            // Option 2: Mit Eis (R-Knopf)
            if (is_right_clicked())
            {
                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("Eis reingeben!");
                lcd1602_move_cursor(0, 1);
                lcd1602_write_string("R-Knopf: Fertig");

                vTaskDelay(pdMS_TO_TICKS(400));
                while (!is_right_clicked())
                {
                    vTaskDelay(pdMS_TO_TICKS(50));
                }

                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("Berechne Menge...");

                // 1. Eisgewicht messen
                float ice_weight = scale_get_weight_gram();
                if (ice_weight < 0)
                    ice_weight = 0;

                // 2. Gesamtgewicht des aktuellen Rezepts berechnen
                float total_recipe = 0;
                for (int i = 0; i < drinks[selected_drink_idx].num_ingredients; i++)
                {
                    total_recipe += drinks[selected_drink_idx].ingredients[i].target_weight;
                }

                // 3. Skalierungsfaktor berechnen (nur runterskalieren, nie hochskalieren)
                if ((ice_weight + total_recipe) > MAX_GLASS_VOLUME)
                {
                    recipe_scale_factor = (MAX_GLASS_VOLUME - ice_weight) / total_recipe;
                    if (recipe_scale_factor < 0.1)
                        recipe_scale_factor = 0.1; // Absicherung
                }
                else
                {
                    recipe_scale_factor = 1.0;
                }

                scale_tara();
                last_displayed_weight = -999.0;
                current_state = STATE_POURING;
                redraw_display = true;
            }
            break;

        // 4. EINGIESSEN DER ZUTATEN
        case STATE_POURING:
        {
            Drink *current_drink = &drinks[selected_drink_idx];
            Ingredient *current_ing = &current_drink->ingredients[current_ingredient_idx];

            // Zielgewicht dynamisch anpassen
            float scaled_target = current_ing->target_weight * recipe_scale_factor;

            float weight = scale_get_weight_gram();

            led_set_progress(weight, scaled_target, current_ing->r, current_ing->g, current_ing->b);

            // Aktualisiere das Display nur, wenn sich der Zustand geändert hat ODER das Gewicht sich um mehr als 0.5g verändert hat
            if (redraw_display || (fabs(weight - last_displayed_weight) >= 0.5))
            {
                char line2[40];
                char line3[40];
                char line4[40];

                snprintf(line2, sizeof(line2), "-> %s", current_ing->name);
                snprintf(line3, sizeof(line3), "Ziel: %5.1f g", scaled_target);
                snprintf(line4, sizeof(line4), "Ist : %5.1f g", weight);

                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));

                // Zeile 1: Welcher Drink wird gemischt?
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string(current_drink->name);

                // Zeile 2: Aktuelle Zutat
                lcd1602_move_cursor(0, 1);
                lcd1602_write_string(line2);

                // Zeile 3: Berechnetes Zielgewicht
                lcd1602_move_cursor(0, 2);
                lcd1602_write_string(line3);

                // Zeile 4: Live-Gewicht der Waage
                lcd1602_move_cursor(0, 3);
                lcd1602_write_string(line4);

                last_displayed_weight = weight;
                redraw_display = false;
            }

            // Ziel erreicht?
            if (weight >= scaled_target)
            {
                printf("Zutat [%s] voll: STOPP! Bitte nicht mehr gießen.\n", current_ing->name);
                clear_led();

                // 1. HARDWARE-RESET FÜR DEN HX711-CHIP
                gpio_set_level(GPIO_NUM_1, 1);
                esp_rom_delay_us(60);
                gpio_set_level(GPIO_NUM_1, 0);
                esp_rom_delay_us(10);

                // 2. Countdown auf dem Display anzeigen für die Beruhigungszeit
                for (int c = 3; c > 0; c--)
                {
                    char countdown_text[40];
                    snprintf(countdown_text, sizeof(countdown_text), "Naechste in %ds...", c);

                    lcd1602_clear();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    lcd1602_move_cursor(0, 0);
                    lcd1602_write_string("STOPP! Voll.");
                    lcd1602_move_cursor(0, 1);
                    lcd1602_write_string(countdown_text);

                    led_set_progress(scaled_target, scaled_target, current_ing->r, current_ing->g, current_ing->b);
                    vTaskDelay(pdMS_TO_TICKS(500)); // 500 Millisekunden leuchten

                    // 3. BLINK-EFFEKT: LEDs aus
                    clear_led();
                    vTaskDelay(pdMS_TO_TICKS(500));
                }
                current_ingredient_idx++;

                if (current_ingredient_idx >= current_drink->num_ingredients)
                {
                    current_state = STATE_FINISHED;
                }
                else
                {
                    lcd1602_clear();
                    vTaskDelay(pdMS_TO_TICKS(2));
                    lcd1602_move_cursor(0, 0);
                    lcd1602_write_string("Tariere neu...");

                    scale_tara();

                    lcd1602_move_cursor(0, 1);
                    lcd1602_write_string("Stabilisiere...");
                    printf("Gebe der Waage Zeit zum Stabilisieren...\n");
                    vTaskDelay(pdMS_TO_TICKS(1000));
                }
                last_displayed_weight = -999.0; // Reset für die nächste Zutat
                redraw_display = true;
            }

            vTaskDelay(pdMS_TO_TICKS(200));
            break;
        }

        // 5. DRINK FERTIG
        case STATE_FINISHED:
            if (redraw_display)
            {
                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));

                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("********************");

                lcd1602_move_cursor(0, 1);
                lcd1602_write_string("   DRINK FERTIG!    ");

                lcd1602_move_cursor(0, 2);
                lcd1602_write_string("   Glas entnehmen   ");

                lcd1602_move_cursor(0, 3);
                lcd1602_write_string("********************");

                redraw_display = false;
            }
            current_state = STATE_WAIT_REMOVE;
            break;

        // 6. WARTEN BIS GLAS WEG IST
        case STATE_WAIT_REMOVE:
        {
            float current_weight = scale_get_weight_gram();

            if (current_weight < -30.0)
            {
                lcd1602_clear();
                vTaskDelay(pdMS_TO_TICKS(2));
                lcd1602_move_cursor(0, 0);
                lcd1602_write_string("Glas entfernt!");
                lcd1602_move_cursor(0, 1);
                lcd1602_write_string("Neustart...");

                printf("Glas wurde entfernt! Starte Neustart-Timer...\n");
                vTaskDelay(pdMS_TO_TICKS(5000));
                current_state = STATE_START;
                redraw_display = true;
            }
            vTaskDelay(pdMS_TO_TICKS(500));
            break;
        }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}