#include "Button.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PIN_LEFT 9
#define PIN_RIGHT 2
#define GPIO_PIN_MASK ((1 << PIN_LEFT) | (1 << PIN_RIGHT))

void button_init()
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = GPIO_PIN_MASK, // Nutzt jetzt sauber Pin 9 und 2
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&io_conf);
}

int read_left_button()
{
    return gpio_get_level(PIN_LEFT) == 0;
}

int read_right_button()
{
    return gpio_get_level(PIN_RIGHT) == 0;
}

bool is_left_clicked()
{
    if (read_left_button())
    {
        vTaskDelay(pdMS_TO_TICKS(30)); // 30ms Warten (Entprellen)
        if (read_left_button())
        {
            // Warten bis der Finger wieder vom Knopf weg ist
            while (read_left_button())
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            return true;
        }
    }
    return false;
}

bool is_right_clicked()
{
    if (read_right_button())
    {
        vTaskDelay(pdMS_TO_TICKS(30)); // 30ms Warten (Entprellen)
        if (read_right_button())
        {
            // Warten bis der Finger wieder vom Knopf weg ist
            while (read_right_button())
            {
                vTaskDelay(pdMS_TO_TICKS(10));
            }
            return true;
        }
    }
    return false;
}