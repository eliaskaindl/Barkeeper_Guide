#ifndef RECIPES_H
#define RECIPES_H

#include <stdint.h>

#define MAX_INGREDIENTS 5

typedef struct
{
    char name[20];
    float target_weight;
    uint32_t r, g, b;
} Ingredient;

typedef struct
{
    char name[20];
    int num_ingredients;
    Ingredient ingredients[MAX_INGREDIENTS];
} Drink;

// Das "extern" sagt dem Compiler: "Dieses Array existiert, vertrau mir, aber reserviere hier noch keinen Speicher."
extern Drink drinks[];
extern const int NUM_DRINKS;

#endif