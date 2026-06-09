#ifndef LCD1602_H
#define LCD1602_H

#include <stdint.h>
#include <stdbool.h>
#include "driver/i2c.h"

typedef struct
{
    int sda_io_num;
    int scl_io_num;
    uint8_t i2c_address;
    i2c_port_t i2c_port;
    int columns;
    int rows;
} lcd1602_config_t;

esp_err_t lcd1602_init(lcd1602_config_t *config);
void lcd1602_clear(void);
void lcd1602_move_cursor(int col, int row);
void lcd1602_write_string(const char *str);
void lcd1602_write_char(char c);

#endif