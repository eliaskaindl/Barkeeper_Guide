#include "Scale.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h" // Für ets_delay_us (Mikrosekunden-Pausen)

#define SERIAL_CLOCK_PIN 1
#define DATA_PIN 0

// gemessener Nullpunkt (Tara)
// const long WAAGE_NULLPUNKT = 495300;

// Kalibrierungsfaktor
const float SCALE_FACTOR = 420.0;

long dynamic_point_zero = 0;

void init_scale()
{
    // Clock-Pin als Ausgang (Senden)
    gpio_config_t clock_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << SERIAL_CLOCK_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&clock_config);

    // Data-Pin als Eingang (Empfangen)
    gpio_config_t data_config = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << DATA_PIN),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
    };
    gpio_config(&data_config);

    // HX711 in Grundzustand versetzen
    gpio_set_level(SERIAL_CLOCK_PIN, 0);
}

int32_t scale_read_raw()
{
    int32_t counter = 0;

    // 1. Warten, bis der HX711 bereit ist (Data-Pin geht auf LOW)
    // OPTIMIERUNG: Extrem schneller Timeout in Mikrosekunden statt Millisekunden!
    uint32_t timeout = 50000; // 50.000 Durchläufe
    while (gpio_get_level(DATA_PIN) && timeout > 0)
    {
        ets_delay_us(1); // Nur 1 Mikrosekunde warten, nicht 1 Millisekunde!
        timeout--;
    }

    if (timeout == 0)
        return 0; // Fehler: Waage nicht angeschlossen

    // 2. Die 24 Bit Daten auslesen
    for (uint8_t i = 0; i < 24; i++)
    {
        gpio_set_level(SERIAL_CLOCK_PIN, 1);
        ets_delay_us(1); // 1 Mikrosekunde warten
        counter = counter << 1;
        gpio_set_level(SERIAL_CLOCK_PIN, 0);
        ets_delay_us(1);

        if (gpio_get_level(DATA_PIN)) // schaut nach ob auf der Datenleitung eine 1 oder eine 0 ist
        {
            counter++;
        }
    }

    // 3. Den 25. Taktimpuls senden (Standard für Kanal A, Verstärkung 128)
    gpio_set_level(SERIAL_CLOCK_PIN, 1);
    ets_delay_us(1);
    gpio_set_level(SERIAL_CLOCK_PIN, 0);
    ets_delay_us(1);

    // 4. Den 24-Bit Wert in einen echten 32-Bit Integer (mit Vorzeichen) umwandeln
    if (counter & 0x800000) // wenn vorzeichen negativ ist (24-Bit = 1)
    {
        counter |= 0xFF000000; // Zweierkomplement
    }

    // SICHERHEITS-ANCHOR: Pin vor dem Verlassen der Funktion IMMER auf 0!
    gpio_set_level(SERIAL_CLOCK_PIN, 0);
    return counter;
}

void scale_auto_tara()
{
    printf("Kalibrierung läuft... Bitte Waage nicht berühren!\n");
    long sum = 0;
    int valid_measurment = 0;

    // Wir versuchen 20 Mal einen Wert zu holen
    for (int i = 0; i < 20; i++)
    {
        long raw_value = scale_read_raw();

        // NUR echte Werte zulassen, keine Systemfehler (0) beim Booten! (Wägezelle 2)
        if (raw_value != 0)
        {
            sum += raw_value;
            valid_measurment++;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (valid_measurment > 0)
    {
        dynamic_point_zero = sum / valid_measurment;
    }
    else
    {
        // Falls gar nichts ging (z.B. Kabel ab), nehmen wir einen Standard-Nullpunkt
        dynamic_point_zero = 0;
    }

    printf("Waage erfolgreich genullt auf aktuellen Wert: %ld\n", dynamic_point_zero);
}

// Berechnungsfunktion
// Liest 10 Werte, bildet den Mittelwert, zieht Tara ab und rechnet in Gramm um
float scale_get_weight_gram()
{
    long sum = 0;
    int valid_measurment = 0;

    for (int i = 0; i < 5; i++)
    {
        long raw_value = scale_read_raw();
        if (raw_value != 0)
        {
            sum += raw_value;
            valid_measurment++;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    if (valid_measurment == 0)
        return 0.0;

    long averaged_raw_value = sum / valid_measurment;

    long difference = averaged_raw_value - dynamic_point_zero;

    // HINWEIS: Falls der Wert beim Drücken negativ wird,
    // multiplizieren wir die Differenz einfach mit -1
    // long difference = (averaged_raw_value - dynamic_point_zero) * -1;

    float weight_g = (float)difference / SCALE_FACTOR;

    // Kleiner Stabilisierungs-Filter (Totzone für die perfekte Optik)
    if (weight_g > -1.0 && weight_g < 1.0)
    {
        weight_g = 0.0;
    }

    return weight_g;
}

void scale_tara()
{
    printf("Waage wird tariert... Bitte nicht berühren!\n");
    long sum = 0;
    int valid_measurment = 0;

    // Wir messen 10-mal schnell hintereinander für einen stabilen neuen Nullpunkt
    for (int i = 0; i < 10; i++)
    {
        long raw_value = scale_read_raw();
        if (raw_value != 0)
        {
            sum += raw_value;
            valid_measurment++;
        }
        vTaskDelay(pdMS_TO_TICKS(40)); // Etwas schnelleres Delay für die manuelle Tara
    }

    if (valid_measurment > 0)
    {
        dynamic_point_zero = sum / valid_measurment;
        printf("Neu tariert auf Wert: %ld\n", dynamic_point_zero);
    }
    else
    {
        printf("Fehler: Tarieren fehlgeschlagen (keine stabilen Werte).\n");
    }
}