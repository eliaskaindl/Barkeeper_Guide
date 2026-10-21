#ifndef WEBSERVER_H
#define WEBSERVER_H

extern volatile int bestellter_drink;
extern volatile int aktueller_status;
extern volatile int skip_requested;
extern volatile int eis_auswahl; // <-- NEU: 1 = Ohne Eis, 2 = Mit Eis

// <-- NEU: Texte und Zahlen für die Live-Anzeige
extern char web_drink_name[32];
extern char web_ing_name[32];
extern volatile float web_current_weight;
extern volatile float web_target_weight;

void start_webserver(void);

#endif