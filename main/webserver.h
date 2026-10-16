#ifndef WEBSERVER_H
#define WEBSERVER_H

// Briefkasten für die Bestellung vom Handy ZUM ESP32
extern volatile int bestellter_drink;

// Briefkasten für den aktuellen Status vom ESP32 ZUM Handy
// (Wir nehmen einfach an, dein current_state ist ein Integer/Zahl)
extern volatile int aktueller_status;

void start_webserver(void);

#endif