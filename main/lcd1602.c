#include "lcd1602.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static lcd1602_config_t lcd_cfg;

// Interne Funktion: Schickt ein Byte an das Display (inkl. Backlight-Bit)
static void lcd_i2c_write(uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (lcd_cfg.i2c_address << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, data | 0x08, true);
    i2c_master_stop(cmd);

    // Wir fangen den Fehlerstatus ab!
    esp_err_t err = i2c_master_cmd_begin(lcd_cfg.i2c_port, cmd, pdMS_TO_TICKS(200));
    if (err != ESP_OK)
    {
        printf("I2C Fehler beim Senden: %s\n", esp_err_to_name(err));
    }

    i2c_cmd_link_delete(cmd);
}

// Erzeugt den nötigen Takt-Puls (Enable)
static void lcd_toggle_enable(uint8_t val)
{
    lcd_i2c_write(val | 0x04); // Enable HIGH
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_i2c_write(val & ~0x04); // Enable LOW
    vTaskDelay(pdMS_TO_TICKS(1));
}

// Sendet Daten/Befehle im 4-Bit-Modus (Zorxx-Logik)
static void lcd_send(uint8_t value, uint8_t mode)
{
    uint8_t highnib = value & 0xF0;
    uint8_t lownib = (value << 4) & 0xF0;
    lcd_toggle_enable(highnib | mode);
    lcd_toggle_enable(lownib | mode);
}

esp_err_t lcd1602_init(lcd1602_config_t *config)
{
    lcd_cfg = *config;

    // I2C-Hardware des ESP32-C3 konfigurieren
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = lcd_cfg.sda_io_num,
        .scl_io_num = lcd_cfg.scl_io_num,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000,
        .clk_flags = 0};
    i2c_param_config(lcd_cfg.i2c_port, &conf);
    i2c_driver_install(lcd_cfg.i2c_port, conf.mode, 0, 0, 0);

    // Zorxx-Initialisierungssequenz für den 4-Bit-Modus
    vTaskDelay(pdMS_TO_TICKS(50));
    lcd_toggle_enable(0x30);
    vTaskDelay(pdMS_TO_TICKS(5));
    lcd_toggle_enable(0x30);
    vTaskDelay(pdMS_TO_TICKS(1));
    lcd_toggle_enable(0x30);
    lcd_toggle_enable(0x20); // 4-Bit Modus aktiviert

    // Display-Grundeinstellungen
    lcd_send(0x28, 0); // Function Set: 4-Bit, 2/4 Zeilen, 5x8 Font
    lcd_send(0x0C, 0); // Display Control: Display AN, Cursor AUS, Blinken AUS
    lcd1602_clear();   // Display löschen
    lcd_send(0x06, 0); // Entry Mode Set: Von links nach rechts schreiben

    return ESP_OK;
}

void lcd1602_clear(void)
{
    lcd_send(0x01, 0); // Clear Display Command
    vTaskDelay(pdMS_TO_TICKS(2));
}

void lcd1602_move_cursor(int col, int row)
{
    int row_offsets[] = {0x00, 0x40, 0x14, 0x54}; // Adressen für 2004 Displays
    if (row >= 0 && row < 4)
    {
        lcd_send(0x80 | (col + row_offsets[row]), 0);
    }
}

void lcd1602_write_char(char c)
{
    lcd_send((uint8_t)c, 0x01); // 0x01 = RS-Bit gesetzt (Daten-Modus)
}

void lcd1602_write_string(const char *str)
{
    while (*str)
    {
        lcd1602_write_char(*str++);
    }
}