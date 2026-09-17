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

void test_led() // rotes Blinkmuster
{
    for (uint8_t i = 0; i < 3; i++)
    {
        for (uint8_t j = 0; j < MAX_LEDS; j++)
        {
            led_strip_set_pixel(matrix_handle, j, 50, 0, 0);
        }
        led_strip_refresh(matrix_handle);
        vTaskDelay(pdMS_TO_TICKS(250));
        clear_led();
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void led_set_progress(float current_weight, float target_weight, uint32_t r, uint32_t g, uint32_t b)
{
    if (target_weight <= 0.0)
    {
        return;
    }
    float progress = current_weight / target_weight;

    // Sicherheits-Begrenzungen
    if (progress < 0.0)
    {
        progress = 0.0;
    }
    if (progress > 1.0)
    {
        progress = 1.0;
    }

    float exact_leds = progress * MAX_LEDS; // Kommazahlen für Dimmen
    int full_leds = (int)exact_leds;
    float fractional = exact_leds - full_leds; // Restwert

    for (int i = 0; i < MAX_LEDS; i++)
    {
        if (i < full_leds)
        {
            led_strip_set_pixel(matrix_handle, i, r, g, b); // 100% leuchtende LEDs
        }
        else if (i == full_leds)
        {
            led_strip_set_pixel(matrix_handle, i, r * fractional, g * fractional, b * fractional); // führende Led wird gedimmt
        }
        else
        {
            led_strip_set_pixel(matrix_handle, i, 0, 0, 0);
        }
    }
    led_strip_refresh(matrix_handle);
}