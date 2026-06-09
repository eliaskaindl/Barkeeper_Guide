#ifndef BUTTON_H
#define BUTTON_H

#include <stdbool.h>

void button_init();
int read_left_button();
int read_right_button();
bool is_left_clicked();
bool is_right_clicked();

#endif