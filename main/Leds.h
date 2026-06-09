#ifndef LEDS_H
#define LEDS_H

#include <stdint.h>

void init_matrix();
void clear_led();
void test_led();
void led_set_progress(float current_weight, float target_weight, uint32_t r, uint32_t g, uint32_t b);

#endif