#pragma GCC diagnostic ignored "-Wwrite-strings"

//start=============================================================

//Automatische Steuerung zum Bierbrauen
//von fg100
// Anpassungen an 4x20 LCD von realholgi

// Version Vx.x vit mit geänderter Encoderdrehrichtung

// V2.2 Anpassungen und Verkleinerung der Sketchgröße
// V2.1 Timerfunktion hinzu
// V2.0 Funktion Kochen mit Hopfengaben hinzu
// V1.6 Fehler bei der Zeitzählung wenn größergleich 60 min behoben
// V1.5 Rufmodus mit/ohne Stopfunktion wählbar
// V1.4 Funktion Kühlung hinzu
// V1.3 Funktion Nachgussbereitung und Alarmtest hinzu
// V1.2 mit Anschluss für Funkalarm
//      und Temperaturvorgabe abhängig von der Rastanzahl
// V1.1 mit Sensorfehlererkennung


// Bedienung
//  - Alle Einstellungen sind mit dem Encoder vorzunehmen
//  - Encoder drehen = Werteverstellung
//  - kurz Drücken = Eingabebestätigung
//  - lang Drücken (>2Sekunden) = Abbruch und zurück zum Startmenue
//  - bei Abbruch weren alle eingegenbenen Werte beibehalten
//  - wenn man z.B. einen Fehler bei der Eingabe macht einfach
//    lange Drücken bis Abbruch. Dann nochmals die Eingaben
//    durchdrücken und ggf. verändern.
//  - während des automatischen Ablaufs ist durch Drehen
//    des Encoders Änderungen an der Temperatur
//    bzw. des Zeitablaufes möglich
//  - länger als 5 Sekunden den Encoder drücken, dann
//    startet ein Test der Alarmsummer
//  - Kühlen, Hysterese +1°C und -1°C
//    d.h. schaltet 1° unter soll aus und^1° über soll ein
//    Schaltverzögerung beim Kühlen 1 min


// Sensorfehler
// -127 => VCC fehlt
// 85.00 => interner Sensorfehler ggf. Leitung zu lang
// 0.00 => Datenleitung oder GND fehlt



/*
 * Schaltvorgänge des Ein- und Ausschalten sind immer nach einer
 * Wartezeit von 1 min möglich.
 * Dadurch soll stetiges Schalten verhindert werden.
 * Gilt nicht bei Überschreitung der Sollteperatur um 0,5 Grad,
 * dann wird sofort ausgeschaltet.

 * Die Funktionen die auf den Vergleich Sollwert mit Istwert
 * beruhen werden immer erst nach 10 Messungen ausgeführt.


 * Bedienung mit Encoder und
 * LCD-Display Nokia 5110
 * Anschlüsse:
 * PIN 2 : Encoder => Pullup geschaltet => Encoderzuleitung mit GND versorgen
 * PIN 3 : Encoder => Pullup geschaltet => Encoderzuleitung mit GND versorgen
 * PIN 4 : Taster von Encoder => Pullup geschaltet => Zuleitung mit GND versorgen
 * PIN 5 : DS1820 Sensor => 4,7k zw. 5V und PIN5
 * PIN 6 : Heizung Phase      => funktioniert umgekehrt (HIGH aus, LOW an)
 * PIN 7 : Heizung Nullleiter => funktioniert umgekehrt (HIGH aus, LOW an)
 * PIN A5 (=19): Ruf (Summer) => Analogpin als Digitaler Ausgang
 * PIN A4 (=18): Funkruf (Summer) => Analogpin als Digitaler Ausgang

 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3f, 20, 4);
#include <avr/wdt.h>
#include <avr/eeprom.h>

const byte CRASH = 0x13;

byte degC[8] = {
    B01000, B10100, B01000, B00111, B01000, B01000, B01000, B00111
};

//------------------------------------------------------------------

//Encoder Initialisierung ----------------------------------------------
volatile int number = 0;
volatile boolean halfleft = false;      // Used in both interrupt routines
volatile boolean halfright = false;     // Used in both interrupt routines

int oldnumber = 0;
boolean ButtonPressed = false;

enum PinAssignments {
    encoderPinA = 2,   // rigtht
    encoderPinB = 3,   // left
    tasterPin = 5,    // another  pins
    oneWirePin = 6,
    ruehrerPin = 7,  // Relais1 = Ruehrer
    heizungPin = 8,  // Relais2 = Heizung
    heizungExternPin = 9, // Relais3 = Heizung extern
    beeperPin = 14,  // Braumeisterruf A0
    schalterFPin = 10, // // Relais4 = Beeper extern Braumeisterruf
};

//int drehen;        //drehgeber Werte
//int fuenfmindrehen;    //drehgeber Werte 5 Minutensprünge

//------------------------------------------------------------------



//Temperatursensor DS1820 Initialisierung ---------------------------
#include <OneWire.h>
#include <DallasTemperature.h>


// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(oneWirePin);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;


//Zeitmessung----------------------------------------------------------
#include <Time.h>

//-----------------------------------------------------------------

enum MODUS {HAUPTSCHIRM = 0,
            MANUELL, NACHGUSS, MAISCHEN, KUEHLEN,
            EINGABE_RAST_ANZ, AUTOMATIK = EINGABE_RAST_ANZ, EINGABE_MAISCHTEMP, EINGABE_RAST_TEMP, EINGABE_RAST_ZEIT, EINGABE_BRAUMEISTERRUF, EINGABE_ENDTEMP,
            AUTO_START, AUTO_MAISCHTEMP, AUTO_RAST_TEMP, AUTO_RAST_ZEIT, AUTO_ENDTEMP,
            BRAUMEISTERRUFALARM, BRAUMEISTERRUF,
            KOCHEN, EINGABE_HOPFENGABEN_ANZAHL, EINGABE_HOPFENGABEN_ZEIT, KOCHEN_START_FRAGE, KOCHEN_AUFHEIZEN, KOCHEN_AUTO_LAUF,
            TIMER, TIMERLAUF,
            ABBRUCH, ALARMTEST
           };

enum REGEL_MODE {REGL_AUS = 0, REGL_MAISCHEN, REGL_KUEHLEN, REGL_KOCHEN};

enum BM_ALARM_MODE {BM_ALARM_AUS = 0, BM_ALARM_MIN = BM_ALARM_AUS, BM_ALARM_WAIT, BM_ALARM_SIGNAL, BM_ALARM_MAX = BM_ALARM_SIGNAL};

void funktion_startabfrage(MODUS naechsterModus, char *title);
boolean warte_und_weiter(MODUS naechsterModus);

//Allgemein Initialisierung------------------------------------------
//boolean anfang = true;
//unsigned long altsekunden;
//REGEL_MODE regelung = REGL_AUS;
//boolean heizung = false;
//boolean ruehrer = false;
//boolean sensorfehler = false;
//float hysterese = 0;
//unsigned long wartezeit = -60000;
//unsigned long serwartezeit = 0;
//float sensorwert;
//float isttemp = 20;                                   //Vorgabe 20 damit Sensorfehler am Anfang nicht anspricht
//int isttemp_ganzzahl;                                 //für Übergabe der isttemp als Ganzzahl
//MODUS modus = HAUPTSCHIRM;
//MODUS rufmodus = HAUPTSCHIRM;
//unsigned long rufsignalzeit = 0;
//boolean nachgussruf = false;                          //Signal wenn Nachgusstemp erreicht
//int x = 1;                                            //aktuelle Rast Nummer
//int y = 1;                                            //Übergabewert von x für Braumeisterruf
int n = 0; //                                           //Counter Messungserhöhung zur Fehlervermeidung
//int pause = 0;                                        //Counter für Ruftonanzahl
unsigned long abbruchtaste;
boolean einmaldruck = false;                          //Überprüfung loslassen der Taste Null
//boolean zeigeH = false;

//int sekunden = 0;                                       //Zeitzählung
//int minuten = 0;                                        //Zeitzählung
//int minutenwert = 0;                                    //Zeitzählung
//int stunden = 0;                                        //Zeitzählung


//Vorgabewerte zur ersten Einstellung-------------------------------------------
//int sollwert = 20;                                  //Sollwertvorgabe für Anzeige
//int maischtemp = 38;                               //Vorgabe Einmasichtemperatur
//int rasten = 4;                                       //Vorgabe Anzahl Rasten
//int rastTemp[] = {
//    0, 50, 64, 72, 72, 72, 72, 72
//};        //Rasttemperatur Werte
//int rastZeit[] = {
//    0, 40, 30, 20, 15, 20, 20, 20
//};              //Rastzeit Werte
//BM_ALARM_MODE braumeister[] = {
//    BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_SIGNAL, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS
//};               //Braumeisterruf standart AUS
//int endtemp = 78;                                   //Vorgabewert Endtemperatur

//int kochzeit = 90;
//int hopfenanzahl = 2;
//int hopfenZeit[] = {
//    0, 10, 80, 80, 80, 40, 40
//};
//int timer = 10;

#define LEFT 0
#define RIGHT 9999
#define CENTER 9998

struct config_t {
    boolean anfang;
    boolean heizung;
    boolean ruehrer;
    MODUS modus;
    REGEL_MODE regelung;
    MODUS rufmodus;
    int x;
    int y;
    boolean zeigeH;
    int sekunden;
    int minuten;
    int minutenwert;
    int stunden;
    unsigned long altsekunden;
    int kochzeit;
    int endtemp;
    int hopfenanzahl;
    int timer;
    int rasten;
    int pause;
    int sollwert;
    int maischtemp;
    boolean nachgussruf;
    unsigned long rufsignalzeit;
    unsigned long wartezeit;
    int drehen;
    int fuenfmindrehen;
    float sensorwert;
    float isttemp;
    int isttemp_ganzzahl;
    boolean sensorfehler;
    float hysterese;
    int rastTemp[];
    int rastZeit[];
    BM_ALARM_MODE braumeister[];
    int hopfenZeit[];
} cfg;

/*
template <class T> int EEPROM_readAnything(int ee, T& value)
{
    byte* p = (byte*)(void*)&value;
    unsigned int i;
    for (i = 0; i < sizeof(value); i++)
          *p++ = EEPROM.read(ee++);
    return i;
}

template <class T> int EEPROM_writeAnything(int ee, const T &value)
{
    const byte *p = (const byte *)(const void *)&value;
    unsigned int i;

    for (i = 0; i < sizeof(value); i++) {
        if ( EEPROM.read(ee) == *p ) {
            ee++;
            p++;
        } else {
            EEPROM.write(ee++, *p++);  // Only write the data if it is different to what's there
        }
    }
}
*/

void print_lcd (char *st, int x, int y)
{
    int stl = strlen(st);
    if (x == RIGHT) {
        x = 20 - stl;
    }
    if (x == CENTER) {
        x = (20 - stl) / 2;
    }

    x = constrain(x, 0, 19);
    y = constrain(y, 0, 3);

    lcd.setCursor(x, y);
    lcd.print(st);
}

void printNumI_lcd(int num, int x, int y)
{
    char st[27];
    sprintf(st, "%i", num);
    print_lcd(st, x, y);
}

void printNumF_lcd (double num, int x, int y, byte dec = 1, int length = 0)
{
    char st[27];

    dtostrf(num, length, dec, st );
    print_lcd(st, x, y);
}

void beeperOn(boolean value)
{
    if (value) {
        digitalWrite(beeperPin, HIGH); // einschalten
    } else {
        digitalWrite(beeperPin, LOW); // ausschalten
    }
}

void ruehrerOn(boolean value)
{
    if (value) {
        digitalWrite(ruehrerPin, LOW); // einschalten
    } else {
        digitalWrite(ruehrerPin, HIGH); // ausschalten
    }
}

void heizungOn(boolean value)
{
    if (value) {
        digitalWrite(heizungPin, LOW);   // einschalten FIXME
        digitalWrite(heizungExternPin, LOW);   // einschalten
    } else {
        digitalWrite(heizungPin, HIGH);   // ausschalten
        digitalWrite(heizungExternPin, HIGH);   // ausschalten
    }
}

void funkrufOn(boolean value)
{
    if (value) {
        //digitalWrite(schalterFPin, LOW); // einschalten FIXME
    } else {
        digitalWrite(schalterFPin, HIGH); // ausschalten
    }
}
//setup=============================================================
void setup()
{

    /*
        Serial.begin(9600);           //.........Test Serial
        Serial.println("Test");         //.........Test Serial
    */

    pinMode(ruehrerPin, OUTPUT);   // initialize the digital pin as an output.
    pinMode(heizungPin, OUTPUT);   // initialize the digital pin as an output.
    pinMode(heizungExternPin, OUTPUT);   // initialize the digital pin as an output.
    pinMode(beeperPin, OUTPUT);   // initialize the digital pin as an output.
    pinMode(schalterFPin, OUTPUT);   // initialize the digital pin as an output.

    ruehrerOn(false);
    heizungOn(false);
    beeperOn(false);
    funkrufOn(false);   // ausschalten

    pinMode(tasterPin, INPUT);                    // Pin für Taster
    digitalWrite(tasterPin, HIGH);                // Turn on internal pullup resistor

    pinMode(encoderPinA, INPUT);                    // Pin für Drehgeber
    digitalWrite(encoderPinA, HIGH);                // Turn on internal pullup resistor
    pinMode(encoderPinB, INPUT);                    // Pin für Drehgeber
    digitalWrite(encoderPinB, HIGH);                // Turn on internal pullup resistor

    attachInterrupt(0, isr_2, FALLING);   // Call isr_2 when digital pin 2 goes LOW
    attachInterrupt(1, isr_3, FALLING);   // Call isr_3 when digital pin 3 goes LOW


    //LCD Setup ------------------------------------------------------
    lcd.init();
    lcd.createChar(8, degC);         // Celsius
    lcd.backlight();
    lcd.clear();
    lcd.noCursor();

    print_lcd("BK V2.2 - LC2004", LEFT, 0);
    print_lcd("Arduino", LEFT, 1);
    print_lcd(":)", RIGHT, 2);
    print_lcd("realholgi & fg100", RIGHT, 3);

    //delay (2000);

    lcd.clear();
    // ---------------------------------------------------------------


    //Temperatursensor DS1820 ----------------------------------------
    sensors.getAddress(insideThermometer, 0);
    // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
    sensors.setResolution(insideThermometer, 9);
    //---------------------------------------------------------------
    watchdogSetup();

    cfg.drehen = cfg.sollwert;
    cfg.x = 1;

    if (eeprom_read_byte(0) == CRASH) {
        eeprom_read_block((void *)&cfg, (void *)1, sizeof(cfg));
        eeprom_write_byte(0, 0);
    } else {
        cfg.anfang = true;
        cfg.regelung = REGL_AUS;
        cfg.heizung = false;
        cfg.ruehrer = false;
        cfg.sensorfehler = false;
        cfg.hysterese = 0;
        cfg.wartezeit = -60000;
        cfg.isttemp = 20;                                   //Vorgabe 20 damit Sensorfehler am Anfang nicht anspricht
        cfg.modus = HAUPTSCHIRM;
        cfg.rufmodus = HAUPTSCHIRM;
        cfg.rufsignalzeit = 0;
        cfg.nachgussruf = false;                          //Signal wenn Nachgusstemp erreicht
        cfg.x = 1;                                            //aktuelle Rast Nummer
        cfg.y = 1;                                            //Übergabewert von x für Braumeisterruf
        cfg.pause = 0;
        cfg.zeigeH = false;
        cfg.sekunden = 0;                                       //Zeitzählung
        cfg.minuten = 0;                                        //Zeitzählung
        cfg.minutenwert = 0;                                    //Zeitzählung
        cfg.stunden = 0;                                        //Zeitzählung
        cfg.sollwert = 20;                                  //Sollwertvorgabe für Anzeige
        cfg.maischtemp = 38;                               //Vorgabe Einmasichtemperatur
        cfg.rasten = 4;                                    //Vorgabe Anzahl Rasten
        cfg.rastTemp[0] = 0;
        cfg.rastTemp[1] = 50;
        cfg.rastTemp[2] = 64;
        cfg.rastTemp[3] = 72;
        cfg.rastTemp[4] = 72;
        cfg.rastTemp[5] = 72;
        cfg.rastTemp[6] = 72;
        cfg.rastTemp[7] = 72;
        cfg.rastZeit[0] = 0;
        cfg.rastZeit[1] = 40;
        cfg.rastZeit[2] = 30;
        cfg.rastZeit[3] = 20;
        cfg.rastZeit[4] = 15;
        cfg.rastZeit[5] = 20;
        cfg.rastZeit[6] = 20;
        cfg.rastZeit[7] = 20;
        cfg.braumeister[0] = BM_ALARM_AUS;
        cfg.braumeister[1] = BM_ALARM_AUS;
        cfg.braumeister[2] = BM_ALARM_AUS;
        cfg.braumeister[3] = BM_ALARM_AUS;
        cfg.braumeister[4] = BM_ALARM_SIGNAL;
        cfg.braumeister[5] = BM_ALARM_AUS;
        cfg.braumeister[6] = BM_ALARM_AUS;
        cfg.braumeister[7] = BM_ALARM_AUS;
        cfg.endtemp = 78;                                   //Vorgabewert Endtemperatur
        cfg.kochzeit = 90;
        cfg.hopfenanzahl = 2;
        cfg.hopfenZeit[0] = 0;
        cfg.hopfenZeit[1] = 10;
        cfg.hopfenZeit[2] = 80;
        cfg.hopfenZeit[3] = 80;
        cfg.hopfenZeit[3] = 80;
        cfg.hopfenZeit[4] = 40;
        cfg.hopfenZeit[5] = 40;
        cfg.timer = 10;
    }
}


//loop=============================================================
void loop()
{

    // Zeitermittlung ------------------------------------------------------
    cfg.sekunden = second();  //aktuell Sekunde abspeichern für die Zeitrechnung
    cfg.minutenwert = minute(); //aktuell Minute abspeichern für die Zeitrechnung
    cfg.stunden = hour();     //aktuell Stunde abspeichern für die Zeitrechnung
    //---------------------------------------------------------------


    // Temperatursensor DS1820 Temperaturabfrage ---------------------
    // call sensors.requestTemperatures() to issue a global temperature
    sensors.requestTemperatures(); // Send the command to get temperatures
    cfg.sensorwert = sensors.getTempC(insideThermometer);
    if ((cfg.sensorwert != cfg.isttemp) && (n < 5)) { // Messfehlervermeidung
        // des Sensorwertes
        n++;                                   // nach mehreren
    }                                      // Messungen
    else {
        cfg.isttemp = cfg.sensorwert;
        n = 0;
    }
    //---------------------------------------------------------------


    // Sensorfehler -------------------------------------------------
    // Sensorfehler -127 => VCC fehlt
    // Sensorfehler 85.00 => interner Sensorfehler ggf. Leitung zu lang
    //                       nicht aktiviert
    // Sensorfehler 0.00 => Datenleitung oder GND fehlt


    if (cfg.regelung == REGL_MAISCHEN || cfg.regelung == REGL_KUEHLEN) { //nur bei Masichen bzw. Kühlen
        if ((int)cfg.isttemp == -127 || (int)cfg.isttemp == 0 ) {
            //zur besseren Erkennung Umwandling in (int)-Wert
            //sonst Probleme mit der Erkennung gerade bei 0.00
            if (!cfg.sensorfehler) {
                cfg.rufmodus = cfg.modus;
                print_lcd("Sensorfehlr", RIGHT, 3);
                cfg.regelung = REGL_AUS;
                cfg.heizung = false;
                cfg.ruehrer = false;
                cfg.sensorfehler = true;
                cfg.modus = BRAUMEISTERRUFALARM;
            }
        }
    }
    //-------------------------------------------------------------------

    //Encoder drehen ------------------------------------------------
    if (number != oldnumber) {
        {
            if (number > oldnumber) { // < > Zeichen ändern = Encoderdrehrichtung ändern
                ++cfg.drehen;
                //halbdrehen=halbdrehen+.5;
                cfg.fuenfmindrehen = cfg.fuenfmindrehen + 5;
            } else {
                --cfg.drehen;
                //halbdrehen=halbdrehen-.5;
                cfg.fuenfmindrehen = cfg.fuenfmindrehen - 5;
            }
            oldnumber = number;
        }
    }
    //---------------------------------------------------------------

    // Temperaturanzeige Istwert ---------------------------------------
    if ((!cfg.sensorfehler) && (int(cfg.sensorwert) != -127)) {
        print_lcd("ist ", 10, 3);
        printNumF_lcd(float(cfg.isttemp), 15, 3);
        lcd.setCursor(19, 3);
        lcd.write(8);
    } else {
        print_lcd("   ERR", RIGHT, 3);
    }

    //-------------------------------------------------------------------


    //Heizregelung----------------------------------------------------
    if (cfg.regelung == REGL_MAISCHEN || cfg.regelung == REGL_KUEHLEN) {
        // Temperaturanzeige Sollwert ---------------------------------------
        print_lcd("soll ", 9, 1);
        printNumF_lcd(int(cfg.sollwert), 15, 1);
        lcd.setCursor(19, 1);
        lcd.write(8);
        //-------------------------------------------------------------------
    }

    //unsigned long now = millis();

    if (cfg.regelung == REGL_MAISCHEN) {
        /*
        Regelung beim Hochfahren: Heizung schaltet 0,5°C vor Sollwert aus
         nach einer Wartezeit schaltet es dann um auf hysteresefreie Regelung
         beim Umschalten zwischen ein und aus,
         D.h. nach dem Umschalten ist ein weiteres Schalten für 1 min gesperrt.
         Ausnahme ist die Überschreitung des Sollwertes um 0,5°C.
         Dann wird sofort ausgschaltet.
         Es soll dadurch das Relaisrattern durch Springen
         der Temperatur am Schaltpunkt verhindern.
         */

        //setzt Hysteres beim Hochfahren auf 0.5°C unter sollwert
        if ((cfg.isttemp <= (cfg.sollwert - 4)) && (cfg.heizung == 1)) {
            cfg.hysterese = 0.5;
        }

        //Ausschalten wenn Sollwert-Hysterese erreicht und dann Wartezeit
        if (cfg.heizung && (cfg.isttemp >= (cfg.sollwert - cfg.hysterese)) && (millis() >= (cfg.wartezeit + 60000))) {
            // mit Wartezeit für eine Temperaturstabilität
            cfg.heizung = false;             // Heizung ausschalten
            cfg.hysterese = 0;           //Verschiebung des Schaltpunktes um die Hysterese
            cfg.wartezeit = millis();    //Start Wartezeitzählung
        }

        //Einschalten wenn kleiner Sollwert und dann Wartezeit
        if ((!cfg.heizung) && (cfg.isttemp <= (cfg.sollwert - 0.5)) && (millis() >= (cfg.wartezeit + 60000))) {
            // mit Wartezeit für eine Temperaturstabilität
            cfg.heizung = true;             // Heizung einschalten
            cfg.hysterese = 0;           //Verschiebung des Schaltpunktes um die Hysterese
            cfg.wartezeit = millis();    //Start Wartezeitzählung
        }

        //Ausschalten vor der Wartezeit, wenn Sollwert um 0,5 überschritten
        if (cfg.heizung && (cfg.isttemp >= (cfg.sollwert + 0.5))) {
            cfg.heizung = false;             // Heizung ausschalten
            cfg.hysterese = 0;           //Verschiebung des Schaltpunktes um die Hysterese
            cfg.wartezeit = millis();           //Start Wartezeitzählung
        }
    }

    //Ende Heizregelung---------------------------------------------------


    //Kühlregelung -----------------------------------------------------
    if (cfg.regelung == REGL_KUEHLEN) {
        if ((!cfg.heizung) && (cfg.isttemp >= (cfg.sollwert + 1)) && (millis() >= (cfg.wartezeit + 60000))) {
            // mit Wartezeit für eine Temperaturstabilität
            cfg.heizung = true;             // einschalten
            cfg.wartezeit = millis();    //Start Wartezeitzählung
        }

        if (cfg.heizung && (cfg.isttemp <= cfg.sollwert - 1) && (millis() >= (cfg.wartezeit + 60000))) {
            // mit Wartezeit für eine Temperaturstabilität
            cfg.heizung = false;             // ausschalten
            cfg.wartezeit = millis();    //Start Wartezeitzählung
        }

        //Ausschalten vor der Wartezeit, wenn Sollwert um 2 unterschritten
        if (cfg.heizung && (cfg.isttemp < (cfg.sollwert - 2))) {
            cfg.heizung = false;             // ausschalten
            cfg.wartezeit = millis();    //Start Wartezeitzählung
        }
    }
    //Ende Kühlregelung ---------------------------------------------

    //Kochen => dauernd ein----------------------------------------------
    if (cfg.regelung == REGL_KOCHEN) {
        cfg.heizung = true;             // einschalten
    }
    //Ende Kochen -----------------------------------------------------------

    // Zeigt den Buchstaben "H" bzw. "K", wenn Heizen oder Kühlen----------
    // und schalter die Pins--------------------------------------------

    if (cfg.heizung) {
        if (cfg.zeigeH) {
            switch (cfg.regelung) {
                case REGL_KOCHEN: //Kochen
                case REGL_MAISCHEN:  //Maischen
                    print_lcd("H", LEFT, 3);
                    break;

                case REGL_KUEHLEN: //Kühlen
                    print_lcd("K", LEFT, 3);
                    break;

                default:
                    break;
            }
        }
    } else {
        if (cfg.zeigeH) {
            print_lcd(" ", LEFT, 3);
        }
    }

    heizungOn(cfg.heizung);
    ruehrerOn(cfg.ruehrer);

    /*
    if (millis() >= (serwartezeit + 1000)) {
        Serial.print(millis());
        Serial.print("\t");
        Serial.print(isttemp);
        Serial.println("");
        serwartezeit = millis();
    }
    */
    // Drehgeber und Tastenabfrage -------------------------------------------------
    getButton();  //Taster abfragen
    //---------------------------------------------------------------


    // Abfrage Modus
    cfg.zeigeH = true;
    switch (cfg.modus) {
        case HAUPTSCHIRM:  //Hauptschirm
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_hauptschirm();
            break;

        case MANUELL:  //Nur Temperaturregelung
            cfg.regelung = REGL_MAISCHEN;
            cfg.ruehrer = true;
            cfg.zeigeH = true;
            funktion_temperatur();
            break;

        case MAISCHEN:  //Maischmenue
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_maischmenue();
            break;

        case NACHGUSS:  //Nachgusswasserbereitung
            cfg.regelung = REGL_MAISCHEN;
            cfg.ruehrer = false;
            cfg.zeigeH = true;
            funktion_temperatur();
            break;

        case KUEHLEN:  //Kühlen
            cfg.regelung = REGL_KUEHLEN;
            cfg.ruehrer = false;
            cfg.zeigeH = true;
            funktion_temperatur();
            break;

        case ALARMTEST: //Alarmtest
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            cfg.rufmodus = HAUPTSCHIRM;
            cfg.modus = BRAUMEISTERRUFALARM;
            print_lcd("Alarmtest", RIGHT, 0);
            break;

        case EINGABE_RAST_ANZ:   //Eingabe Anzahl der Rasten
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_rastanzahl();
            break;

        case EINGABE_MAISCHTEMP:  //Eingabe Einmaischtemperatur
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_maischtemperatur();
            break;

        case EINGABE_RAST_TEMP:  //Eingabe der Temperatur der Rasten
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_rasteingabe();
            break;

        case EINGABE_RAST_ZEIT:  //Eingabe der Rastzeitwerte
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_zeiteingabe();
            break;

        case EINGABE_BRAUMEISTERRUF:  //Eingabe Braumeisterruf an/aus ?
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_braumeister();
            break;

        case EINGABE_ENDTEMP:  //Eingabe der Endtemperaturwert
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_endtempeingabe();
            break;

        case AUTO_START:  //Startabfrage
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_startabfrage(AUTO_MAISCHTEMP, "Auto");
            break;

        case AUTO_MAISCHTEMP:  //Automatik Maischtemperatur
            cfg.regelung = REGL_MAISCHEN;
            cfg.ruehrer = true;
            cfg.zeigeH = true;
            funktion_maischtemperaturautomatik();
            break;

        case AUTO_RAST_TEMP:  //Automatik Temperatur
            cfg.regelung = REGL_MAISCHEN;
            cfg.ruehrer = true;
            cfg.zeigeH = true;
            funktion_tempautomatik();
            break;

        case AUTO_RAST_ZEIT:  //Automatik Zeit
            cfg.regelung = REGL_MAISCHEN;
            cfg.ruehrer = true;
            cfg.zeigeH = true;
            funktion_zeitautomatik();
            break;

        case AUTO_ENDTEMP:  //Automatik Endtemperatur
            cfg.regelung = REGL_MAISCHEN;
            cfg.ruehrer = true;
            cfg.zeigeH = true;
            funktion_endtempautomatik();
            break;

        case BRAUMEISTERRUFALARM:  //Braumeisterrufalarm
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_braumeisterrufalarm();
            break;

        case BRAUMEISTERRUF:  //Braumeisterruf
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_braumeisterruf();
            break;

        case KOCHEN:  //Kochen Kochzeit
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_kochzeit();
            break;

        case EINGABE_HOPFENGABEN_ANZAHL:  //Kochen Anzahl der Hopfengaben
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_anzahlhopfengaben();
            break;

        case EINGABE_HOPFENGABEN_ZEIT:  //Kochen Eingabe der Zeitwerte
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_hopfengaben();
            break;

        case KOCHEN_START_FRAGE:  //Startabfrage
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_startabfrage(KOCHEN_AUFHEIZEN, "Kochen");
            break;

        case KOCHEN_AUFHEIZEN:  //Aufheizen
            cfg.regelung = REGL_KOCHEN;        //Kochen => dauernd eingeschaltet
            cfg.ruehrer = false;
            cfg.zeigeH = true;
            funktion_kochenaufheizen();
            break;

        case KOCHEN_AUTO_LAUF:  //Kochen Automatik Zeit
            cfg.regelung = REGL_KOCHEN;        //Kochen => dauernd eingeschaltet
            cfg.ruehrer = false;
            cfg.zeigeH = true;
            funktion_hopfenzeitautomatik();
            break;

        case TIMER:  //Timer
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_timer();
            break;

        case TIMERLAUF:  //Timerlauf
            cfg.regelung = REGL_AUS;
            cfg.ruehrer = false;
            cfg.zeigeH = false;
            funktion_timerlauf();
            break;

        case ABBRUCH:  //Abbruch
            cfg.ruehrer = false;
            cfg.zeigeH = true;
            funktion_abbruch();
            break;
    }

    // -----------------------------------------------------------------
    wdt_reset();
}
// Ende Loop
// ------------------------------------------------------------------

//Funktionen=============================================================
// ab hier Funktionen

// Funtionen für Drehgeber
void isr_2()
{
    // Pin2 went LOW
    delay(1);                                                // Debounce time
    if (digitalRead(encoderPinA) == LOW) {                             // Pin2 still LOW ?
        if (digitalRead(encoderPinB) == HIGH && halfright == false) {    // -->
            halfright = true;                                    // One half click clockwise
        }
        if (digitalRead(encoderPinB) == LOW && halfleft == true) {       // <--
            halfleft = false;                                    // One whole click counter-
            {
                number++;
            }                                                    // clockwise
        }
    }
}

void isr_3()
{
    // Pin3 went LOW
    delay(1);                                               // Debounce time
    if (digitalRead(encoderPinB) == LOW) {                            // Pin3 still LOW ?
        if (digitalRead(encoderPinA) == HIGH && halfleft == false) {    // <--
            halfleft = true;                                    // One half  click counter-
        }                                                     // clockwise
        if (digitalRead(encoderPinA) == LOW && halfright == true) {     // -->
            halfright = false;                                  // One whole click clockwise
            {
                number--;
            }
        }
    }
}
// ----------------------------------------------------------------------


// Funktion Tastendruck getButton-----------------------------------------------
boolean getButton()
{
    //delay(100);
    int ButtonVoltage = digitalRead(tasterPin);
    if (ButtonVoltage  == HIGH) {
        ButtonPressed = false;
        abbruchtaste = millis();
    } else if (ButtonVoltage == LOW) {
        ButtonPressed = true;
        if (millis() >= (abbruchtaste + 2000)) {     //Taste 2 Sekunden drücken
            cfg.modus = ABBRUCH;                              //abbruchmodus=modus80
        }
    }

    return ButtonPressed;
}
//--------------------------------------------------------------------

// Funktion warte_und_weiter---------------------------------

boolean warte_und_weiter(MODUS naechsterModus)
{
    if (!ButtonPressed) {
        einmaldruck = true;
    }
    if (einmaldruck == true) {
        if (ButtonPressed) {
            einmaldruck = false;
            cfg.modus = naechsterModus;
            cfg.anfang = true;
            return true;
        }
    }
    return false;
}

//-----------------------------------------------------------------

void menu_zeiger(int pos)
{
    int p;
    for (p = 0; p <= 3; p++) {
        if (p == pos) {
            print_lcd("=>", LEFT, p);
        } else {
            print_lcd("  ", LEFT, p);
        }
    }
}

// Funktion Hauptschirm---------------------------------
void funktion_hauptschirm()      //Modus=0
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Maischen", 2, 0);
        print_lcd("Kochen", 2, 1);
        print_lcd("Timer", 2, 2);
        print_lcd("Kuehlen", 2, 3);
        cfg.drehen = 0;
        cfg.anfang = false;
    }

    cfg.drehen = constrain(cfg.drehen, 0, 3);

    menu_zeiger(cfg.drehen);
    switch (cfg.drehen) {
        case 0:
            cfg.rufmodus = MAISCHEN;
            break;
        case 1:
            cfg.rufmodus = KOCHEN;
            break;
        case 2:
            cfg.rufmodus = TIMER;
            break;
        case 3:
            cfg.rufmodus = KUEHLEN;
            break;
    }

    if (warte_und_weiter(cfg.rufmodus)) {
        if (cfg.modus == KUEHLEN) {
            //Übergabe an Modus1
            cfg.isttemp_ganzzahl = cfg.isttemp;     //isttemp als Ganzzahl
            cfg.drehen = cfg.isttemp_ganzzahl;  //ganzzahliger Vorgabewert
        }                               //für Sollwert
        lcd.clear();
    }
}
//-----------------------------------------------------------------


// Funktion Hauptschirm---------------------------------
void funktion_maischmenue()      //Modus=01
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Automatik", 2, 0);
        print_lcd("Manuell", 2, 1);
        print_lcd("Nachguss", 2, 2);
        cfg.drehen = 0;
        cfg.anfang = false;
    }

    cfg.drehen = constrain(cfg.drehen, 0, 2);

    menu_zeiger(cfg.drehen);
    switch (cfg.drehen) {
        case 0:
            cfg.rufmodus = AUTOMATIK;
            break;
        case 1:
            cfg.rufmodus = MANUELL;
            break;
        case 2:
            cfg.rufmodus = NACHGUSS;
            break;
    }

    if (warte_und_weiter(cfg.rufmodus)) {
        if (cfg.modus == MANUELL) {
            //Übergabe an Modus1
            cfg.isttemp_ganzzahl = cfg.isttemp;     //isttemp als Ganzzahl
            cfg.drehen = (cfg.isttemp_ganzzahl + 10); //ganzzahliger Vorgabewert 10°C über Ist
        }                               //für Sollwert
        if (cfg.modus == NACHGUSS) {
            //Übergabe an Modus2
            cfg.drehen = cfg.endtemp;                    //Nachgusstemperatur
        }                               //für Sollwert
        //lcd.clear();
    }
}
//-----------------------------------------------------------------


// Funktion nur Temperaturregelung---------------------------------
void funktion_temperatur()      //Modus=1 bzw.2
{
    if (cfg.anfang) {
        lcd.clear();
        cfg.anfang = false;
    }

    cfg.sollwert = cfg.drehen;
    switch (cfg.modus) {
        case MANUELL:
            print_lcd("Manuell", LEFT, 0);
            break;

        case NACHGUSS:
            print_lcd("Nachguss", LEFT, 0);
            break;

        case KUEHLEN:
            print_lcd("Kuehlen", LEFT, 0);
            break;

        default:
            break;
    }

    if ((cfg.modus == MANUELL) && (cfg.isttemp >= cfg.sollwert)) { //Manuell -> Sollwert erreicht
        cfg.rufmodus = ABBRUCH;                //Abbruch nach Rufalarm
        cfg.modus = BRAUMEISTERRUFALARM;
        cfg.regelung = REGL_AUS;              //Regelung aus
        cfg.heizung = false;               //Heizung aus
        cfg.y = 0;
        cfg.braumeister[cfg.y] = BM_ALARM_WAIT;        // Ruf und Abbruch
    }

    if ((cfg.modus == NACHGUSS) && (cfg.isttemp >= cfg.sollwert) && (cfg.nachgussruf == false)) { //Nachguss -> Sollwert erreicht
        cfg.nachgussruf = true;
        cfg.rufmodus = NACHGUSS;          //Rufalarm
        cfg.modus = BRAUMEISTERRUFALARM;
        cfg.y = 0;
        cfg.braumeister[cfg.y] = BM_ALARM_SIGNAL;        //nur Ruf und weiter mit Regelung
    }
}

// Funktion Eingabe der Rastanzahl------------------------------------
void funktion_rastanzahl()          //Modus=19
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        print_lcd("Rasten", LEFT, 1);

        cfg.drehen = cfg.rasten;
        cfg.anfang = false;
    }

    cfg.drehen = constrain(cfg.drehen, 1, 5);
    cfg.rasten = cfg.drehen;

    //Vorgabewerte bei verschiedenen Rasten
    switch (cfg.drehen) {
        case 1:
            cfg.rastTemp[1] = 67;
            cfg.rastZeit[1] = 30;
            cfg.maischtemp = 65;
            break;

        case 2:
            cfg.rastTemp[1] = 62;
            cfg.rastZeit[1] = 30;
            cfg.rastTemp[2] = 72;
            cfg.rastZeit[2] = 35;
            cfg.maischtemp = 55;
            break;

        case 3:
            cfg.rastTemp[1] = 50;
            cfg.rastZeit[1] = 40;
            cfg.rastTemp[2] = 64;
            cfg.rastZeit[2] = 30;
            cfg.rastTemp[3] = 72;
            cfg.rastZeit[3] = 30;
            cfg.maischtemp = 38;
            break;

        case 4:
            cfg.rastTemp[1] = 50;
            cfg.rastZeit[1] = 40;
            cfg.rastTemp[2] = 64;
            cfg.rastZeit[2] = 30;
            cfg.rastTemp[3] = 72;
            cfg.rastZeit[3] = 20;
            cfg.rastTemp[4] = 72;
            cfg.rastZeit[4] = 15;
            cfg.maischtemp = 38;
            break;

        case 5:
            cfg.rastTemp[1] = 35;
            cfg.rastZeit[1] = 20;
            cfg.rastTemp[2] = 40;
            cfg.rastZeit[2] = 20;
            cfg.rastTemp[3] = 55;
            cfg.rastZeit[3] = 15;
            cfg.rastTemp[4] = 64;
            cfg.rastZeit[4] = 35;
            cfg.rastTemp[5] = 72;
            cfg.rastZeit[5] = 25;
            cfg.maischtemp = 30;
            break;

        default:
            ;
    }

    printNumI_lcd(cfg.rasten, 19, 1);

    warte_und_weiter(EINGABE_MAISCHTEMP);
}
//------------------------------------------------------------------


// Funktion Maischtemperatur-----------------------------------------
void funktion_maischtemperatur()      //Modus=20
{

    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        cfg.drehen = cfg.maischtemp;
        cfg.anfang = false;
    }

    cfg.drehen = constrain( cfg.drehen, 10, 105);
    cfg.maischtemp = cfg.drehen;

    print_lcd("Maischtemp", LEFT, 1);
    printNumF_lcd(int(cfg.maischtemp), 15, 1);
    lcd.setCursor(19, 1);
    lcd.write(8);

    warte_und_weiter(EINGABE_RAST_TEMP);
}
//------------------------------------------------------------------


// Funktion Rasteingabe Temperatur----------------------------------
void funktion_rasteingabe()      //Modus=21
{

    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        cfg.drehen = cfg.rastTemp[cfg.x];
        cfg.anfang = false;
    }

    cfg.drehen = constrain( cfg.drehen, 9, 105);
    cfg.rastTemp[cfg.x] = cfg.drehen;

    printNumI_lcd(cfg.x, LEFT, 1);
    print_lcd(". Rast", 1, 1);
    printNumF_lcd(int(cfg.rastTemp[cfg.x]), 15, 1);
    lcd.setCursor(19, 1);
    lcd.write(8);

    warte_und_weiter(EINGABE_RAST_ZEIT);
}
//------------------------------------------------------------------


// Funktion Rasteingabe Zeit----------------------------------------
void funktion_zeiteingabe()      //Modus=22
{

    if (cfg.anfang) {
        cfg.fuenfmindrehen = cfg.rastZeit[cfg.x];
        cfg.anfang = false;
    }

    cfg.fuenfmindrehen = constrain( cfg.fuenfmindrehen, 1, 99);
    cfg.rastZeit[cfg.x] = cfg.fuenfmindrehen;

    print_lcd_minutes(cfg.rastZeit[cfg.x], RIGHT, 2);

    warte_und_weiter(EINGABE_BRAUMEISTERRUF);
}
//------------------------------------------------------------------


// Funktion Braumeister---------------------------------------------
void funktion_braumeister() //Modus=24
{
    if (cfg.anfang) {
        cfg.drehen = (int)cfg.braumeister[cfg.x];
        cfg.anfang = false;
    }

    //delay(200);

    cfg.drehen = constrain( cfg.drehen, BM_ALARM_MIN, BM_ALARM_MAX);
    cfg.braumeister[cfg.x] = (BM_ALARM_MODE)cfg.drehen;

    print_lcd("Ruf", LEFT, 2);

    switch (cfg.braumeister[cfg.x]) {
        case BM_ALARM_AUS:
            print_lcd("    Nein", RIGHT, 2);
            break;

        case BM_ALARM_WAIT:
            print_lcd("Anhalten", RIGHT, 2);
            break;

        case BM_ALARM_SIGNAL:
            print_lcd("  Signal", RIGHT, 2);
            break;
    }

    if (warte_und_weiter(EINGABE_ENDTEMP)) {
        if (cfg.x < cfg.rasten) {
            cfg.x++;
            cfg.modus = EINGABE_RAST_TEMP;           //Sprung zur Rasttemperatureingabe
        } else {
            cfg.x = 1;
            cfg.modus = EINGABE_ENDTEMP;           //Sprung zur Rastzeiteingabe
        }
    }
}
//------------------------------------------------------------------


// Funktion Ende Temperatur-----------------------------------------
void funktion_endtempeingabe()      //Modus=25
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        cfg.drehen = cfg.endtemp;
        cfg.anfang = false;
    }

    cfg.drehen = constrain( cfg.drehen, 10, 80);
    cfg.endtemp = cfg.drehen;

    print_lcd("Endtemperatur", LEFT, 1);
    printNumF_lcd(int(cfg.endtemp), 15, 1);
    lcd.setCursor(19, 1);
    lcd.write(8);

    warte_und_weiter(AUTO_START);
}
//------------------------------------------------------------------

// Funktion Startabfrage--------------------------------------------
void funktion_startabfrage(MODUS naechsterModus, char *title)
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd(title, LEFT, 0);
        cfg.anfang = false;
        cfg.altsekunden = millis();
    }

    if (millis() >= (cfg.altsekunden + 1000)) {
        print_lcd("       ", CENTER, 2);
        if (millis() >= (cfg.altsekunden + 1500))
        { cfg.altsekunden = millis(); }
    } else {
        print_lcd("Start ?", CENTER, 2);
    }

    warte_und_weiter(naechsterModus);
}

//------------------------------------------------------------------


// Funktion Automatik Maischtemperatur---------------------------------
void funktion_maischtemperaturautomatik()      //Modus=27
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Maischen", RIGHT, 0);
        cfg.drehen = cfg.maischtemp;    //Zuordnung Encoder
        cfg.anfang = false;
    }

    cfg.drehen = constrain( cfg.drehen, 10, 105);
    cfg.maischtemp = cfg.drehen;
    cfg.sollwert = cfg.maischtemp;

    if (cfg.isttemp >= cfg.sollwert) { // Sollwert erreicht ?
        cfg.rufmodus = AUTO_RAST_TEMP;
        cfg.y = 0;
        cfg.braumeister[cfg.y] = BM_ALARM_WAIT;
        cfg.modus = BRAUMEISTERRUFALARM;
    }
}
//------------------------------------------------------------------


// Funktion Automatik Temperatur------------------------------------
void funktion_tempautomatik()      //Modus=28
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        printNumI_lcd(cfg.x, 13, 0);
        print_lcd(". Rast", RIGHT, 0);

        cfg.drehen = cfg.rastTemp[cfg.x];
        cfg.anfang = false;
        cfg.wartezeit = millis() + 60000;  // sofort aufheizen
    }

    cfg.drehen = constrain( cfg.drehen, 10, 105);

    cfg.rastTemp[cfg.x] = cfg.drehen;
    cfg.sollwert = cfg.rastTemp[cfg.x];

    if (cfg.isttemp >= cfg.sollwert) { // Sollwert erreicht ?
        cfg.modus = AUTO_RAST_ZEIT;              //zur Zeitautomatik
        cfg.anfang = true;
    }
}
//------------------------------------------------------------------


// Funktion Automatik Zeit------------------------------------------
void funktion_zeitautomatik()      //Modus=29
{
    if (cfg.anfang) {
        cfg.drehen = cfg.rastZeit[cfg.x];              //Zuordnung für Encoder
    }

    print_lcd_minutes(cfg.rastZeit[cfg.x], RIGHT, 2);

    // Zeitzählung---------------
    if (cfg.anfang) {
        print_lcd("Set Time", LEFT, 3);

        setTime(00, 00, 00, 00, 01, 01); //.........Sekunden auf 0 stellen

        delay(400); //test

        cfg.sekunden = second();  //aktuell Sekunde abspeichern für die Zeitrechnung
        cfg.minutenwert = minute(); //aktuell Minute abspeichern für die Zeitrechnung
        cfg.stunden = hour();     //aktuell Stunde abspeichern für die Zeitrechnung

        print_lcd("            ", 0, 3);
        cfg.anfang = false;
        print_lcd("00:00", LEFT, 2);
    }


    if (cfg.sekunden < 10) {
        printNumI_lcd(cfg.sekunden, 4, 2);
        if (cfg.sekunden == 0) {
            print_lcd("0", 3, 2);
        }
    } else {
        printNumI_lcd(cfg.sekunden, 3, 2);
    }

    if (cfg.stunden == 0) {
        cfg.minuten = cfg.minutenwert;
    } else {
        cfg.minuten = ((cfg.stunden * 60) + cfg.minutenwert);
    }

    if (cfg.minuten < 10) {
        printNumI_lcd(cfg.minuten, 1, 2);
    } else {
        printNumI_lcd(cfg.minuten, 0, 2);
    }
    // Ende Zeitzählung---------------------

    cfg.drehen = constrain( cfg.drehen, 10, 105);
    cfg.rastZeit[cfg.x] = cfg.drehen;   //Encoderzuordnung

    if (cfg.minuten >= cfg.rastZeit[cfg.x]) { // Sollwert erreicht ?
        cfg.anfang = true;
        cfg.y = cfg.x;
        if (cfg.x < cfg.rasten) {
            cfg.modus = AUTO_RAST_TEMP;              // zur Temperaturregelung
            cfg.x++;                    // nächste Stufe
        } else {
            cfg.x = 1;                              //Endtemperatur
            cfg.modus = AUTO_ENDTEMP;                          //Endtemperatur
        }

        if (cfg.braumeister[cfg.y] != BM_ALARM_AUS) {
            cfg.rufmodus = cfg.modus;
            cfg.modus = BRAUMEISTERRUFALARM;
        }
    }
}
//------------------------------------------------------------------


// Funktion Automatik Endtemperatur---------------------------------
void funktion_endtempautomatik()      //Modus=30
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Endtemp", RIGHT, 0);
        cfg.drehen = cfg.endtemp;    //Zuordnung Encoder
        cfg.anfang = false;
    }

    cfg.drehen = constrain( cfg.drehen, 10, 105);
    cfg.endtemp = cfg.drehen;
    cfg.sollwert = cfg.endtemp;

    if (cfg.isttemp >= cfg.sollwert) { // Sollwert erreicht ?
        cfg.rufmodus = ABBRUCH;         //Abbruch
        cfg.modus = BRAUMEISTERRUFALARM;
        cfg.regelung = REGL_AUS;              //Regelung aus
        cfg.heizung = false;               //Heizung aus
        cfg.y = 0;
        cfg.braumeister[cfg.y] = BM_ALARM_WAIT;
    }
}
//------------------------------------------------------------------


// Funktion braumeisterrufalarm---------------------------------------
void funktion_braumeisterrufalarm()      //Modus=31
{
    if (cfg.anfang) {
        cfg.rufsignalzeit = millis();
        cfg.anfang = false;
    }

    if (millis() >= (cfg.altsekunden + 1000)) { //Bliken der Anzeige und RUF
        print_lcd("          ", LEFT, 3);
        beeperOn(false);
        if (millis() >= (cfg.altsekunden + 1500)) {
            cfg.altsekunden = millis();
            cfg.pause++;
        }
    } else {
        print_lcd("RUF", LEFT, 3);
        if (cfg.pause <= 4) {
            beeperOn(true);
        }
        if (cfg.pause > 8) {
            cfg.pause = 0;
        }
    }                                   //Bliken der Anzeige und RUF


    if ((cfg.pause == 4) || (cfg.pause == 8)) {   //Funkalarm schalten
        funkrufOn(true);;    //Funkalarm ausschalten
    } else {
        funkrufOn(false);;   // Funkalarm ausschalten
    }

    //20 Sekunden Rufsignalisierung wenn "Ruf Signal"
    if (cfg.braumeister[cfg.y] == BM_ALARM_SIGNAL && millis() >= (cfg.rufsignalzeit + 20000)) {
        cfg.anfang = true;
        cfg.pause = 0;
        beeperOn(false);   // Alarm ausschalten
        funkrufOn(false);;   // Funkalarm ausschalten
        cfg.modus = cfg.rufmodus;
        einmaldruck = false;
    }
    //weiter mit Programmablauf

    if (warte_und_weiter(BRAUMEISTERRUF)) {
        cfg.pause = 0;
        beeperOn(false);   // Alarm ausschalten
        funkrufOn(false);   // Funkalarm ausschalten
        if (cfg.braumeister[cfg.y] == BM_ALARM_SIGNAL) {
            print_lcd("   ", LEFT, 3);
            cfg.modus = cfg.rufmodus;
        }
    }
}
//------------------------------------------------------------------


// Funktion braumeisterruf------------------------------------------
void funktion_braumeisterruf()      //Modus=32
{
    if (cfg.anfang) {
        cfg.anfang = false;
    }

    if (millis() >= (cfg.altsekunden + 1000)) {
        print_lcd("        ", LEFT, 3);
        if (millis() >= (cfg.altsekunden + 1500)) {
            cfg.altsekunden = millis();
        }
    } else {
        print_lcd("weiter ?", LEFT, 3);
    }

    if (warte_und_weiter(cfg.rufmodus)) {
        print_lcd("        ", LEFT, 3);     //Text "weiter ?" löschen
        print_lcd("             ", RIGHT, 3); //Löscht Text bei
        cfg.sensorfehler = false;                         //Sensorfehler oder Alarmtest
        delay(500);     //kurze Wartezeit, damit nicht
        //durch unbeabsichtigtes Drehen
        //der nächste Vorgabewert
        //verstellt wird
    }
}
//------------------------------------------------------------------


// Funktion Kochzeit-------------------------------------------------
void funktion_kochzeit()      //Modus=40
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        print_lcd("Zeit", LEFT, 1);

        cfg.fuenfmindrehen = cfg.kochzeit;
        cfg.anfang = false;
    }

    cfg.fuenfmindrehen = constrain( cfg.fuenfmindrehen, 20, 180);
    cfg.kochzeit = cfg.fuenfmindrehen; //5min-Sprünge

    print_lcd_minutes( cfg.kochzeit, RIGHT, 1);

    warte_und_weiter(EINGABE_HOPFENGABEN_ANZAHL);
}
//------------------------------------------------------------------

// Funktion Anzahl der Hopfengaben------------------------------------------
void funktion_anzahlhopfengaben()      //Modus=41
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        print_lcd("Hopfengaben", LEFT, 1);

        cfg.drehen = cfg.hopfenanzahl;
        cfg.anfang = false;
    }

    cfg.drehen = constrain(cfg.drehen, 1, 5);
    cfg.hopfenanzahl = cfg.drehen;

    printNumI_lcd(cfg.hopfenanzahl, RIGHT, 1);

    warte_und_weiter(EINGABE_HOPFENGABEN_ZEIT);
}
//------------------------------------------------------------------

// Funktion Hopfengaben-------------------------------------------
void funktion_hopfengaben()      //Modus=42
{
    if (cfg.anfang) {
        cfg.x = 1;
        cfg.fuenfmindrehen = cfg.hopfenZeit[cfg.x];
        cfg.anfang = false;
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
    }

    printNumI_lcd(cfg.x, LEFT, 1);
    print_lcd(". Hopfengabe", 1, 1);
    print_lcd("nach", LEFT, 2);

    cfg.fuenfmindrehen = constrain(cfg.fuenfmindrehen, cfg.hopfenZeit[(cfg.x - 1)] + 5, cfg.kochzeit - 5);
    cfg.hopfenZeit[cfg.x] = cfg.fuenfmindrehen;

    print_lcd_minutes(cfg.hopfenZeit[cfg.x], RIGHT, 2);

    if (warte_und_weiter(cfg.modus)) {
        if (cfg.x < cfg.hopfenanzahl) {
            cfg.x++;
            cfg.fuenfmindrehen = cfg.hopfenZeit[cfg.x];
            print_lcd("  ", LEFT, 1);
            print_lcd("   ", 13, 2);
            delay(400);
            cfg.anfang = false; // nicht auf Anfang zurück
        } else {
            cfg.x = 1;
            cfg.modus = KOCHEN_START_FRAGE;
        }
    }
}
//------------------------------------------------------------------


// Funktion Kochenaufheizen-------------------------------------------
void funktion_kochenaufheizen()      //Modus=44
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);
        cfg.anfang = false;
    }

    if (cfg.isttemp >= 98) {
        print_lcd("            ", RIGHT, 0);
        print_lcd("Kochbeginn", CENTER, 1);
        beeperOn(true);
        delay(500);
        beeperOn(false);
        cfg.anfang = true;
        cfg.modus = KOCHEN_AUTO_LAUF;
    } else {
        print_lcd("Aufheizen", RIGHT, 0);
    }
}
//------------------------------------------------------------------


// Funktion Hopfengaben Benachrichtigung------------------------------------------
void funktion_hopfenzeitautomatik()      //Modus=45
{
    if (cfg.anfang) {
        cfg.x = 1;
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);

        setTime(00, 00, 00, 00, 01, 01); //.........Sekunden auf 0 stellen

        print_lcd_minutes(cfg.kochzeit, RIGHT, 0);

        cfg.sekunden = second();  //aktuell Sekunde abspeichern für die Zeitrechnung
        cfg.minutenwert = minute(); //aktuell Minute abspeichern für die Zeitrechnung
        cfg.stunden = hour();     //aktuell Stunde abspeichern für die Zeitrechnung

        cfg.anfang = false;
        print_lcd("00:00", 11, 1);
    }


    if (cfg.x <= cfg.hopfenanzahl) {
        printNumI_lcd(cfg.x, LEFT, 2);
        print_lcd(". Gabe bei ", 1, 2);
        print_lcd_minutes(cfg.hopfenZeit[cfg.x], RIGHT, 2);
    } else {
        print_lcd("                    ", 0, 2);
    }

    print_lcd("min", RIGHT, 1);

    if (cfg.sekunden < 10) {
        printNumI_lcd(cfg.sekunden, 15, 1);
        if (cfg.sekunden) {
            print_lcd("0", 14, 1);
        }
    } else {
        printNumI_lcd(cfg.sekunden, 14, 1);
    }

    cfg.minuten = ((cfg.stunden * 60) + cfg.minutenwert);
    if (cfg.minuten < 10) {
        printNumI_lcd(cfg.minuten, 12, 1);
    }

    if ((cfg.minuten >= 10) && (cfg.minuten < 100)) {
        printNumI_lcd(cfg.minuten, 11, 1);
    }

    if (cfg.minuten >= 100) {
        printNumI_lcd(cfg.minuten, 10, 1);
    }

    if ((cfg.minuten == cfg.hopfenZeit[cfg.x]) && (cfg.x <= cfg.hopfenanzahl)) {  // Hopfengabe
        //Alarm -----
        cfg.zeigeH = false;
        if (millis() >= (cfg.altsekunden + 1000)) { //Bliken der Anzeige und RUF
            print_lcd("   ", LEFT, 3);
            beeperOn(false);
            if (millis() >= (cfg.altsekunden + 1500)) {
                cfg.altsekunden = millis();
                cfg.pause++;
            }
        } else {
            print_lcd("RUF", LEFT, 3);
            if (cfg.pause <= 4) {
                beeperOn(true);
            }
            if (cfg.pause > 8) {
                cfg.pause = 0;
            }
        }                                   //Bliken der Anzeige und RUF


        if ((cfg.pause == 4) || (cfg.pause == 8)) {   //Funkalarm schalten
            funkrufOn(true);    //Funkalarm einschalten
        } else {
            funkrufOn(false);   // Funkalarm ausschalten
        }
        //-----------


        if (warte_und_weiter(cfg.modus)) {
            cfg.pause = 0;
            cfg.zeigeH = true;
            print_lcd("   ", LEFT, 3);
            beeperOn(false);   // Alarm ausschalten
            funkrufOn(false);   // Funkalarm ausschalten
            cfg.x++;
            cfg.anfang = false; // nicht zurücksetzen!!!
        }
    }

    if ((cfg.minuten > cfg.hopfenZeit[cfg.x]) && (cfg.x <= cfg.hopfenanzahl)) {  // Alarmende nach 1 Minute
        cfg.pause = 0;
        beeperOn(false);   // Alarm ausschalten
        funkrufOn(false);   // Funkalarm ausschalten
        cfg.x++;
    }

    if (cfg.minuten >= cfg.kochzeit) {   //Kochzeitende
        cfg.rufmodus = ABBRUCH;                //Abbruch nach Rufalarm
        cfg.modus = BRAUMEISTERRUFALARM;
        cfg.regelung = REGL_AUS;              //Regelung aus
        cfg.heizung = false;               //Heizung aus
        cfg.y = 0;
        cfg.braumeister[cfg.y] = BM_ALARM_WAIT;
    }
}
//------------------------------------------------------------------


// Funktion Timer-------------------------------------------------
void funktion_timer()      //Modus=60
{
    if (cfg.anfang) {
        lcd.clear();
        print_lcd("Timer", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        print_lcd("Zeit", LEFT, 2);

        cfg.drehen = cfg.timer;
        cfg.anfang = false;
    }

    cfg.drehen = constrain( cfg.drehen, 1, 99);
    cfg.timer = cfg.drehen;

    print_lcd_minutes(cfg.timer, RIGHT, 2);

    warte_und_weiter(TIMERLAUF);
}
//------------------------------------------------------------------


// Funktion Timerlauf-------------------------------------------------
void funktion_timerlauf()      //Modus=61
{
    if (cfg.anfang) {
        cfg.drehen = cfg.timer;

        cfg.anfang = false;
        lcd.clear();
        print_lcd("Timer", LEFT, 0);
        print_lcd("Set Time", RIGHT, 0);

        setTime(00, 00, 00, 00, 01, 01); //.........Sekunden auf 0 stellen

        delay(400); //test
        print_lcd("         ", RIGHT, 0);

        cfg.sekunden = second();  //aktuell Sekunde abspeichern für die Zeitrechnung
        cfg.minutenwert = minute(); //aktuell Minute abspeichern für die Zeitrechnung
        cfg.stunden = hour();     //aktuell Stunde abspeichern für die Zeitrechnung

        print_lcd("00:00", LEFT, 2);
    }

    cfg.drehen = constrain(cfg.drehen, 1, 99);
    cfg.timer = cfg.drehen;

    print_lcd_minutes(cfg.timer, RIGHT, 2);

    if (cfg.sekunden < 10) {
        printNumI_lcd(cfg.sekunden, 4, 2);
        if (cfg.sekunden == 0) {
            print_lcd("0", 3, 2);
        }
    } else {
        printNumI_lcd(cfg.sekunden, 3, 2);
    }

    cfg.minuten = ((cfg.stunden * 60) + cfg.minutenwert);
    if (cfg.minuten < 10) {
        printNumI_lcd(cfg.minuten, 1, 2);
    } else {
        printNumI_lcd(cfg.minuten, 0, 2);
    }

    if (cfg.minuten >= cfg.timer) {   //Timerende
        cfg.rufmodus = ABBRUCH;                //Abbruch nach Rufalarm
        cfg.modus = BRAUMEISTERRUFALARM;
        cfg.regelung = REGL_AUS;              //Regelung aus
        cfg.heizung = false;               //Heizung aus
        cfg.y = 0;
        cfg.braumeister[cfg.y] = BM_ALARM_WAIT;
    }
}
//------------------------------------------------------------------

// Funktion Abbruch-------------------------------------------------
void funktion_abbruch()       // Modus 80
{
    cfg.regelung = REGL_AUS;
    cfg.heizung = false;
    cfg.wartezeit = -60000;
    heizungOn(false);
    ruehrerOn(false);
    beeperOn(false);  // ausschalten
    funkrufOn(false);   // ausschalten
    cfg.anfang = true;                     //Daten zurücksetzen
    lcd.clear();                //Rastwerteeingaben
    cfg.rufmodus = HAUPTSCHIRM;                   //bleiben erhalten
    cfg.x = 1;                          //bei
    cfg.y = 1;                          //asm volatile ("  jmp 0");
    n = 0;                          //wird alles
    einmaldruck = false;            //zurückgesetetzt
    cfg.nachgussruf = false;
    cfg.pause = 0;
    cfg.drehen = cfg.sollwert;          //Zuweisung für Funktion Temperaturregelung

    if (millis() >= (abbruchtaste + 5000)) { //länger als 5 Sekunden drücken
        cfg.modus = ALARMTEST;                    //Alarmtest
    } else {
        cfg.modus = HAUPTSCHIRM;                    //Hauptmenue
    }
    // asm volatile ("  jmp 0");       //reset Arduino
}
//------------------------------------------------------------------

void print_lcd_minutes (int value, int x, int y)
{
    if (x == RIGHT) {
        x = 19 - (3 + 4) + 1;
    }

    if (value < 10) {
        print_lcd("  ", x, y);
        printNumI_lcd(value, x + 2, y);
    }
    if ((value < 100) && (value >= 10)) {
        print_lcd(" ", x, y);
        printNumI_lcd(value, x + 1, y);
    }
    if (value >= 100) {
        printNumI_lcd(value, x, y);
    }
    print_lcd(" min", x + 3, y);
}

void watchdogSetup(void)
{
    cli(); // disable all interrupts
    wdt_reset(); // reset the WDT timer
    /*
    WDTCSR configuration:
    WDIE = 1: Interrupt Enable
    WDE = 1: Reset Enable
    WDP3 = 0 :For 2000ms Time-out
    WDP2 = 1 :For 2000ms Time-out
    WDP1 = 1 :For 2000ms Time-out
    WDP0 = 1 :For 2000ms Time-out
    */
    // Enter Watchdog Configuration mode:
    WDTCSR |= (1 << WDCE) | (1 << WDE);
    // Set Watchdog settings:
    WDTCSR = (1 << WDIE) | (1 << WDE) | (0 << WDP3) | (1 << WDP2) | (1 << WDP1) | (1 << WDP0);
    sei();
}

ISR(WDT_vect) // Watchdog timer interrupt.
{
    heizungOn(false);
    ruehrerOn(false);
    beeperOn(true); // beeeeeeeeeeep

    eeprom_write_byte(0, CRASH);
    eeprom_write_block((const void *)&cfg, (void *)1, sizeof(cfg));
    while (true);
}
