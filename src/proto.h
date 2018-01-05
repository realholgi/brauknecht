#ifndef PROTO_H
#define PROTO_H

#include "config.h"

void funktion_startabfrage(MODUS naechsterModus, char *title);
boolean warte_und_weiter(MODUS naechsterModus);
void heizungOn(boolean value);
void beeperOn(boolean value);
void watchdogSetup(void);
void encoderTicker();
boolean getButton();

void funktion_hauptschirm();
void funktion_maischmenue();
void funktion_setupmenu();
void funktion_temperatur();
void funktion_rastanzahl();
void funktion_maischtemperatur();
void funktion_rasteingabe();
void funktion_zeiteingabe();
void funktion_braumeister();
void funktion_endtempeingabe();
void funktion_startabfrage(MODUS naechsterModus, char *title);
void funktion_maischtemperaturautomatik();
void funktion_tempautomatik();
void funktion_zeitautomatik();
void funktion_endtempautomatik();
void funktion_braumeisterrufalarm();
void funktion_braumeisterruf();
void funktion_hysterese();
void funktion_kochschwelle();
void funktion_kochzeit();
void funktion_anzahlhopfengaben();
void funktion_hopfengaben();
void funktion_kochenaufheizen();
void funktion_hopfenzeitautomatik();
void _next_koch_step();
void funktion_timer();
void funktion_timerlauf();
void funktion_abbruch();

void print_lcd_minutes (int value, int x, int y);
void print_lcd (char *st, int x, int y);
void printNumI_lcd(int num, int x, int y);
void printNumF_lcd (double num, int x, int y, byte dec = 1, int length = 0);

void setupWebserver();
void handleNotFound();
void handleDataJson();
void handleRoot();
bool setupWIFI();

bool saveConfig();
bool readConfig();

#endif //PROTO_H
