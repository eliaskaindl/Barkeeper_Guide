#include "recipes.h"

const int NUM_DRINKS = 6;

// --- Rezept-Datenbank ---
Drink drinks[] = {
    {"Aperol Spritz", 3, {
                             {"Aperol", 60.0, 150, 40, 0},      // Orange
                             {"Prosecco", 90.0, 120, 120, 40},  // Hellgelb
                             {"Mineralwasser", 30.0, 0, 0, 150} // Blau
                         }},
    {"Gin Tonic", 2, {
                         {"Gin", 40.0, 100, 100, 100},        // Weiß/Klar
                         {"Tonic Water", 120.0, 0, 100, 120}, // Cyan/Türkis
                         {"", 0.0, 0, 0, 0}                   // Platzhalter
                     }},
    {"Wodka O", 2, {
                       {"Wodka", 40.0, 100, 100, 100},     // Weiß/Klar
                       {"Orangensaft", 120.0, 150, 80, 0}, // Orange
                       {"", 0.0, 0, 0, 0}                  // Platzhalter
                   }},
    {"Tequila Sunrise", 3, {
                               {"Tequila", 40.0, 100, 100, 100},   // Weiß/Klar
                               {"Orangensaft", 100.0, 150, 80, 0}, // Orange
                               {"Grenadine", 15.0, 150, 0, 0}      // Rot
                           }},
    {"Cuba Libre", 3, {
                          {"Rum", 40.0, 120, 60, 20},         // Braun/Amber
                          {"Cola", 120.0, 50, 20, 0},         // Dunkelbraun
                          {"Limettensaft", 15.0, 50, 150, 20} // Hellgrün
                      }},
    {"Margarita", 3, {
                         {"Tequila", 50.0, 100, 100, 100},   // Weiß/Klar
                         {"Triple Sec", 20.0, 120, 100, 50}, // Leichtes Gelb
                         {"Limettensaft", 20.0, 50, 150, 20} // Hellgrün
                     }}};