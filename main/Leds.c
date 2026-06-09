#include "Leds.h"
#include "led_strip.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define MATRIX_PIN 8
#define MAX_LEDS 25

static led_strip_handle_t matrix_handle;

void init_matrix()
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = MATRIX_PIN,
        .max_leds = MAX_LEDS,
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &matrix_handle));
}

void clear_led()
{
    led_strip_clear(matrix_handle);
}

void test_led() // rotes Blinkmuster um zu veranschaulichen ob es gestartet ist
{
    for (uint8_t i = 0; i < 3; i++)
    {
        for (uint8_t j = 0; j < MAX_LEDS; j++) // alle LEDs auf rot
        {
            led_strip_set_pixel(matrix_handle, j, 50, 0, 0);
        }
        led_strip_refresh(matrix_handle);
        vTaskDelay(pdMS_TO_TICKS(250)); // 250ms warten
        clear_led();                    // alle LEDs löschen
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void led_set_progress(float current_weight, float target_weight, uint32_t r, uint32_t g, uint32_t b)
{
    if (target_weight <= 0.0)
        return;

    // 1. Berechnen, wie viel Prozent der Zutat bereits eingegossen sind (0.0 bis 1.0)
    float progress = current_weight / target_weight;

    // Sicherheits-Begrenzungen (falls das Gewicht negativ ist oder über das Ziel hinausschießt)
    if (progress < 0.0)
        progress = 0.0;
    if (progress > 1.0)
        progress = 1.0;

    // 2. Berechnen, wie viele der 25 LEDs leuchten sollen
    int leds_to_light = (int)(progress * MAX_LEDS);

    // 3. LEDs entsprechend setzen
    for (int i = 0; i < MAX_LEDS; i++)
    {
        if (i < leds_to_light)
        {
            // Diese LED leuchtet in der Zutatenfarbe
            led_strip_set_pixel(matrix_handle, i, r, g, b);
        }
        else
        {
            // Der Rest bleibt aus
            led_strip_set_pixel(matrix_handle, i, 0, 0, 0);
        }
    }

    // 4. Die Matrix aktualisieren, damit man die Änderung sieht
    led_strip_refresh(matrix_handle);
}