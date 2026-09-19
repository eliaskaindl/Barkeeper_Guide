#include "recipes.h"

const int NUM_DRINKS = 7;

// --- Rezept-Datenbank ---
Drink drinks[] = {
    {"Aperol Spritz", 3, {
                             {"Aperol", 60.0, 150, 40, 0},      // Orange
                             {"Prosecco", 90.0, 120, 120, 40},  // Hellgelb
                             {"Mineralwasser", 30.0, 0, 0, 150} // Blau
                         }},
    {"Sarti Spritz", 3, {
                            {"Sarti", 60.0, 255, 20, 147},     // Pink
                            {"Prosecco", 90.0, 120, 120, 40},  // Hellgelb
                            {"Mineralwasser", 30.0, 0, 0, 150} // Blau
                        }},
    {"Screwdriver", 2, {
                           {"Wodka", 50.0, 100, 100, 100},     // Weiß/Klar
                           {"Orangensaft", 100.0, 150, 80, 0}, // Orange
                           {"", 0.0, 0, 0, 0}                  // Platzhalter
                       }},
    {"Moscow Mule", 3, {
                           {"Wodka", 45.0, 100, 100, 100},      // Weiß/Klar
                           {"Ginger Beer", 120.0, 255, 140, 0}, // dunkles Orange/Amber
                           {"Limettensaft", 5.0, 50, 150, 20}   // Hellgrün
                       }},
    {"Gin Tonic", 2, {
                         {"Gin", 40.0, 100, 100, 100},        // Weiß/Klar
                         {"Tonic Water", 120.0, 0, 100, 120}, // Cyan/Türkis
                         {"", 0.0, 0, 0, 0}                   // Platzhalter
                     }},
    {"Gin Fizz", 3, {
                        {"Gin", 45.0, 100, 100, 100},       // Weiß/Klar
                        {"Mineralwasser", 80.0, 0, 0, 150}, // Blau
                        {"Limettensaft", 30.0, 50, 150, 20} // Hellgrün
                    }},
    {"Cuba Libre", 3, {
                          {"Rum", 50.0, 120, 60, 20},         // Braun/Amber
                          {"Cola", 120.0, 50, 20, 0},         // Dunkelbraun
                          {"Limettensaft", 10.0, 50, 150, 20} // Hellgrün
                      }},
};