#ifndef SCALE_H
#define SCALE_H

#include <stdint.h>

void init_scale();
int32_t scale_read_raw();
void scale_auto_tara();
float scale_get_weight_gram();
void scale_tara();

#endif