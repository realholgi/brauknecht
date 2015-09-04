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

int drehen;        //drehgeber Werte
int fuenfmindrehen;    //drehgeber Werte 5 Minutensprünge

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
//------------------------------------------------------------------

//EEPROM für Hystere-------------------------------------------------
#include <EEPROM.h>   //EEPROM Programmsierung für Hysteresewert
//------------------------------------------------------------------

int val = 0;           // Variable um Messergebnis zu speichern
//-----------------------------------------------------------------

enum MODUS {HAUPTSCHIRM = 0,
            MANUELL, NACHGUSS, MAISCHEN, SETUP,
            EINGABE_RAST_ANZ, AUTOMATIK = EINGABE_RAST_ANZ, EINGABE_MAISCHTEMP, EINGABE_RAST_TEMP, EINGABE_RAST_ZEIT, EINGABE_BRAUMEISTERRUF, EINGABE_ENDTEMP,
            AUTO_START, AUTO_MAISCHTEMP, AUTO_RAST_TEMP, AUTO_RAST_ZEIT, AUTO_ENDTEMP,
            BRAUMEISTERRUFALARM, BRAUMEISTERRUF,
            KOCHEN, EINGABE_HOPFENGABEN_ANZAHL, EINGABE_HOPFENGABEN_ZEIT, KOCHEN_START_FRAGE, KOCHEN_AUFHEIZEN, KOCHEN_AUTO_LAUF,
            TIMER, TIMERLAUF,
            ABBRUCH, ALARMTEST
           };

enum REGEL_MODE {REGL_AUS = 0, REGL_MAISCHEN, REGL_KOCHEN};

enum BM_ALARM_MODE {BM_ALARM_AUS = 0, BM_ALARM_MIN = BM_ALARM_AUS, BM_ALARM_WAIT, BM_ALARM_SIGNAL, BM_ALARM_MAX = BM_ALARM_SIGNAL};

void funktion_startabfrage(MODUS naechsterModus, char *title);
boolean warte_und_weiter(MODUS naechsterModus);

//Allgemein Initialisierung------------------------------------------
boolean anfang = true;
unsigned long altsekunden;
REGEL_MODE regelung = REGL_AUS;
boolean heizung = false;
boolean ruehrer = false;
boolean sensorfehler = false;
float hysterese = 0;
int hysteresespeicher = 5;
unsigned long wartezeit = -60000;
//unsigned long serwartezeit = 0;
float sensorwert;
float isttemp = 20;                                   //Vorgabe 20 damit Sensorfehler am Anfang nicht anspricht
int isttemp_ganzzahl;                                 //für Übergabe der isttemp als Ganzzahl
MODUS modus = HAUPTSCHIRM;
MODUS rufmodus = HAUPTSCHIRM;
unsigned long rufsignalzeit = 0;
boolean nachgussruf = false;                          //Signal wenn Nachgusstemp erreicht
int x = 1;                                            //aktuelle Rast Nummer
int y = 1;                                            //Übergabewert von x für Braumeisterruf
int n = 0;                                            //Counter Messungserhöhung zur Fehlervermeidung
int pause = 0;                                        //Counter für Ruftonanzahl
unsigned long abbruchtaste;
boolean einmaldruck = false;                          //Überprüfung loslassen der Taste Null
boolean zeigeH = false;

int sekunden = 0;                                       //Zeitzählung
int minuten = 0;                                        //Zeitzählung
int minutenwert = 0;                                    //Zeitzählung
int stunden = 0;                                        //Zeitzählung

boolean hendi_special = true;

//Vorgabewerte zur ersten Einstellung-------------------------------------------
int sollwert = 20;                                  //Sollwertvorgabe für Anzeige
int maischtemp = 38;                               //Vorgabe Einmasichtemperatur
int rasten = 1;                                       //Vorgabe Anzahl Rasten
int rastTemp[] = {
    0, 50, 64, 72, 72, 72, 72, 72
};        //Rasttemperatur Werte
int rastZeit[] = {
    0, 40, 30, 20, 15, 20, 20, 20
};              //Rastzeit Werte
BM_ALARM_MODE braumeister[] = {
    BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_SIGNAL, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS
};               //Braumeisterruf standart AUS
int endtemp = 78;                                   //Vorgabewert Endtemperatur

int kochzeit = 90;
int hopfenanzahl = 2;
int hopfenZeit[] = {
    0, 10, 80, 80, 80, 40, 40
};
int timer = 10;

#define LEFT 0
#define RIGHT 9999
#define CENTER 9998

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
    drehen = sollwert;

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

    print_lcd("BK V2.3 - LC2004", LEFT, 0);
    print_lcd("Arduino", LEFT, 1);
    print_lcd(":)", RIGHT, 2);
    print_lcd("realholgi & fg100", RIGHT, 3);

    //delay (2000);

    if (0) { // FIXME
        for (x = 1; x < 3; x++) {
            beeperOn(true);
            funkrufOn(true);   // Funkalarmtest
            delay(200);
            beeperOn(false);
            funkrufOn(false);   // Funkalarm ausschalten
            delay(200);
        }
    }
    x = 1;

    lcd.clear();
    // ---------------------------------------------------------------


    //Temperatursensor DS1820 ----------------------------------------
    sensors.getAddress(insideThermometer, 0);
    // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
    sensors.setResolution(insideThermometer, 9);
    //---------------------------------------------------------------

    //Hysteresewert Einlesen ----------------------------------------------
    hysteresespeicher = EEPROM.read(0);
    hysterese = hysteresespeicher;   //Zuweisung in einer Zeile gibt Probleme mit float
    hysterese = hysterese / 10;      //Zuweisung in einer Zeile gibt Probleme mit float
    //-------------------------------------------------------------

    watchdogSetup();
}


//loop=============================================================
void loop()
{

    //Hystereseeinstellung -------------------------------------------------------
    if (hysteresespeicher > 40)     //Bei erster Anwendung des IC nötig
    { funktion_hysterese(); }

    // Zeitermittlung ------------------------------------------------------
    sekunden = second();  //aktuell Sekunde abspeichern für die Zeitrechnung
    minutenwert = minute(); //aktuell Minute abspeichern für die Zeitrechnung
    stunden = hour();     //aktuell Stunde abspeichern für die Zeitrechnung
    //---------------------------------------------------------------


    // Temperatursensor DS1820 Temperaturabfrage ---------------------
    // call sensors.requestTemperatures() to issue a global temperature
    sensors.requestTemperatures(); // Send the command to get temperatures
    sensorwert = sensors.getTempC(insideThermometer);
    if ((sensorwert != isttemp) && (n < 5)) { // Messfehlervermeidung
        // des Sensorwertes
        n++;                                   // nach mehreren
    }                                      // Messungen
    else {
        isttemp = sensorwert;
        n = 0;
    }
    //---------------------------------------------------------------


    // Sensorfehler -------------------------------------------------
    // Sensorfehler -127 => VCC fehlt
    // Sensorfehler 85.00 => interner Sensorfehler ggf. Leitung zu lang
    //                       nicht aktiviert
    // Sensorfehler 0.00 => Datenleitung oder GND fehlt


    if (regelung == REGL_MAISCHEN) { //nur bei Masichen
        if ((int)isttemp == -127 || (int)isttemp == 0 ) {
            //zur besseren Erkennung Umwandling in (int)-Wert
            //sonst Probleme mit der Erkennung gerade bei 0.00
            if (!sensorfehler) {
                rufmodus = modus;
                print_lcd("Sensorfehler", RIGHT, 2);
                regelung = REGL_AUS;
                heizung = false;
                ruehrer = false;
                sensorfehler = true;
                modus = BRAUMEISTERRUFALARM;
            }
        }
    }
    //-------------------------------------------------------------------

    //Encoder drehen ------------------------------------------------
    if (number != oldnumber) {
        {
            if (number > oldnumber) { // < > Zeichen ändern = Encoderdrehrichtung ändern
                ++drehen;
                //halbdrehen=halbdrehen+.5;
                fuenfmindrehen = fuenfmindrehen + 5;
            } else {
                --drehen;
                //halbdrehen=halbdrehen-.5;
                fuenfmindrehen = fuenfmindrehen - 5;
            }
            oldnumber = number;
        }
    }
    //---------------------------------------------------------------

    // Temperaturanzeige Istwert ---------------------------------------
    if ((!sensorfehler) && (int(sensorwert) != -127)) {
        print_lcd("ist ", 10, 3);
        printNumF_lcd(float(isttemp), 15, 3);
        lcd.setCursor(19, 3);
        lcd.write(8);
    } else {
        print_lcd("   ERR", RIGHT, 3);
    }

    //-------------------------------------------------------------------


    //Heizregelung----------------------------------------------------
    if (regelung == REGL_MAISCHEN) {
        // Temperaturanzeige Sollwert ---------------------------------------
        print_lcd("soll ", 9, 1);
        printNumF_lcd(int(sollwert), 15, 1);
        lcd.setCursor(19, 1);
        lcd.write(8);
        //-------------------------------------------------------------------
    }

    //unsigned long now = millis();

    if (regelung == REGL_MAISCHEN) {
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
        if ((isttemp <= (sollwert - 4)) && (heizung == 1)) {
            hysterese = hysteresespeicher;   //Zuweisung in einer Zeile gibt Probleme mit float
            hysterese = hysterese / 10;      //Zuweisung in einer Zeile gibt Probleme mit float
        }

        //Ausschalten wenn Sollwert-Hysterese erreicht und dann Wartezeit
        if (heizung && (isttemp >= (sollwert - hysterese)) && (millis() >= (wartezeit + 60000))) {
            // mit Wartezeit für eine Temperaturstabilität
            heizung = false;             // Heizung ausschalten
            hysterese = 0;           //Verschiebung des Schaltpunktes um die Hysterese
            wartezeit = millis();    //Start Wartezeitzählung
        }

        //Einschalten wenn kleiner Sollwert und dann Wartezeit
        if ((!heizung) && (isttemp <= (sollwert - 0.5)) && (millis() >= (wartezeit + 60000))) {
            // mit Wartezeit für eine Temperaturstabilität
            heizung = true;             // Heizung einschalten
            hysterese = 0;           //Verschiebung des Schaltpunktes um die Hysterese
            wartezeit = millis();    //Start Wartezeitzählung
        }

        //Ausschalten vor der Wartezeit, wenn Sollwert um 0,5 überschritten
        if (heizung && (isttemp >= (sollwert + 0.5))) {
            heizung = false;             // Heizung ausschalten
            hysterese = 0;           //Verschiebung des Schaltpunktes um die Hysterese
            wartezeit = millis();           //Start Wartezeitzählung
        }
    }

    //Ende Heizregelung---------------------------------------------------

    //Kochen => dauernd ein----------------------------------------------
    if (regelung == REGL_KOCHEN) {
        heizung = true;             // einschalten
        if (minuten == 85 && hendi_special) {    // HENDI-Spezial, welche nach 90 min abschaltet
            heizung = false;
            delay(1000);
            heizung = true;
            hendi_special = false;
        }
    }
    //Ende Kochen -----------------------------------------------------------

    // Zeigt den Buchstaben "H" bzw. "K", wenn Heizen oder Kühlen----------
    // und schalter die Pins--------------------------------------------

    if (heizung) {
        if (zeigeH) {
            switch (regelung) {
                case REGL_KOCHEN: //Kochen
                case REGL_MAISCHEN:  //Maischen
                    print_lcd("H", LEFT, 3);
                    break;

                default:
                    break;
            }
        }
    } else {
        if (zeigeH) {
            print_lcd(" ", LEFT, 3);
        }
    }

    heizungOn(heizung);
    ruehrerOn(ruehrer);

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
    zeigeH = true;
    switch (modus) {
        case HAUPTSCHIRM:  //Hauptschirm
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_hauptschirm();
            break;

        case MANUELL:  //Nur Temperaturregelung
            regelung = REGL_MAISCHEN;
            ruehrer = true;
            zeigeH = true;
            funktion_temperatur();
            break;

        case MAISCHEN:  //Maischmenue
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_maischmenue();
            break;

        case NACHGUSS:  //Nachgusswasserbereitung
            regelung = REGL_MAISCHEN;
            ruehrer = false;
            zeigeH = true;
            funktion_temperatur();
            break;

        case SETUP:  //Setup
            ruehrer = false;
            zeigeH = false;
            funktion_hysterese();
            break;

        case ALARMTEST: //Alarmtest
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            rufmodus = HAUPTSCHIRM;
            modus = BRAUMEISTERRUFALARM;
            print_lcd("Alarmtest", RIGHT, 0);
            break;

        case EINGABE_RAST_ANZ:   //Eingabe Anzahl der Rasten
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_rastanzahl();
            break;

        case EINGABE_MAISCHTEMP:  //Eingabe Einmaischtemperatur
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_maischtemperatur();
            break;

        case EINGABE_RAST_TEMP:  //Eingabe der Temperatur der Rasten
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_rasteingabe();
            break;

        case EINGABE_RAST_ZEIT:  //Eingabe der Rastzeitwerte
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_zeiteingabe();
            break;

        case EINGABE_BRAUMEISTERRUF:  //Eingabe Braumeisterruf an/aus ?
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_braumeister();
            break;

        case EINGABE_ENDTEMP:  //Eingabe der Endtemperaturwert
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_endtempeingabe();
            break;

        case AUTO_START:  //Startabfrage
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_startabfrage(AUTO_MAISCHTEMP, "Auto");
            break;

        case AUTO_MAISCHTEMP:  //Automatik Maischtemperatur
            regelung = REGL_MAISCHEN;
            ruehrer = true;
            zeigeH = true;
            funktion_maischtemperaturautomatik();
            break;

        case AUTO_RAST_TEMP:  //Automatik Temperatur
            regelung = REGL_MAISCHEN;
            ruehrer = true;
            zeigeH = true;
            funktion_tempautomatik();
            break;

        case AUTO_RAST_ZEIT:  //Automatik Zeit
            regelung = REGL_MAISCHEN;
            ruehrer = true;
            zeigeH = true;
            funktion_zeitautomatik();
            break;

        case AUTO_ENDTEMP:  //Automatik Endtemperatur
            regelung = REGL_MAISCHEN;
            ruehrer = true;
            zeigeH = true;
            funktion_endtempautomatik();
            break;

        case BRAUMEISTERRUFALARM:  //Braumeisterrufalarm
            ruehrer = false;
            zeigeH = false;
            funktion_braumeisterrufalarm();
            break;

        case BRAUMEISTERRUF:  //Braumeisterruf
            ruehrer = false;
            zeigeH = false;
            funktion_braumeisterruf();
            break;

        case KOCHEN:  //Kochen Kochzeit
            ruehrer = false;
            zeigeH = false;
            funktion_kochzeit();
            break;

        case EINGABE_HOPFENGABEN_ANZAHL:  //Kochen Anzahl der Hopfengaben
            ruehrer = false;
            zeigeH = false;
            funktion_anzahlhopfengaben();
            break;

        case EINGABE_HOPFENGABEN_ZEIT:  //Kochen Eingabe der Zeitwerte
            ruehrer = false;
            zeigeH = false;
            funktion_hopfengaben();
            break;

        case KOCHEN_START_FRAGE:  //Startabfrage
            ruehrer = false;
            zeigeH = false;
            funktion_startabfrage(KOCHEN_AUFHEIZEN, "Kochen");
            break;

        case KOCHEN_AUFHEIZEN:  //Aufheizen
            regelung = REGL_KOCHEN;        //Kochen => dauernd eingeschaltet
            ruehrer = false;
            zeigeH = true;
            funktion_kochenaufheizen();
            break;

        case KOCHEN_AUTO_LAUF:  //Kochen Automatik Zeit
            regelung = REGL_KOCHEN;        //Kochen => dauernd eingeschaltet
            ruehrer = false;
            zeigeH = true;
            funktion_hopfenzeitautomatik();
            break;

        case TIMER:  //Timer
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_timer();
            break;

        case TIMERLAUF:  //Timerlauf
            regelung = REGL_AUS;
            ruehrer = false;
            zeigeH = false;
            funktion_timerlauf();
            break;

        case ABBRUCH:  //Abbruch
            ruehrer = false;
            zeigeH = true;
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
            modus = ABBRUCH;                              //abbruchmodus=modus80
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
            modus = naechsterModus;
            anfang = true;
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
    if (anfang) {
        lcd.clear();
        print_lcd("Maischen", 2, 0);
        print_lcd("Kochen", 2, 1);
        print_lcd("Timer", 2, 2);
        print_lcd("Setup", 2, 3);
        drehen = 0;
        anfang = false;
    }

    drehen = constrain(drehen, 0, 3);

    menu_zeiger(drehen);
    switch (drehen) {
        case 0:
            rufmodus = MAISCHEN;
            break;
        case 1:
            rufmodus = KOCHEN;
            break;
        case 2:
            rufmodus = TIMER;
            break;
        case 3:
            rufmodus = SETUP;
            break;
    }

    if (warte_und_weiter(rufmodus)) {
        lcd.clear();
    }
}
//-----------------------------------------------------------------


// Funktion Hauptschirm---------------------------------
void funktion_maischmenue()      //Modus=01
{
    if (anfang) {
        lcd.clear();
        print_lcd("Manuell", 2, 0);
        print_lcd("Automatik", 2, 1);
        print_lcd("Nachguss", 2, 2);
        drehen = 0;
        anfang = false;
    }

    drehen = constrain(drehen, 0, 2);

    menu_zeiger(drehen);
    switch (drehen) {
        case 0:
            rufmodus = MANUELL;
            break;
        case 1:
            rufmodus = AUTOMATIK;
            break;
        case 2:
            rufmodus = NACHGUSS;
            break;
    }

    if (warte_und_weiter(rufmodus)) {
        if (modus == MANUELL) {
            //Übergabe an Modus1
            isttemp_ganzzahl = isttemp;     //isttemp als Ganzzahl
            drehen = (isttemp_ganzzahl + 10); //ganzzahliger Vorgabewert 10°C über Ist
        }                               //für Sollwert
        if (modus == NACHGUSS) {
            //Übergabe an Modus2
            drehen = endtemp;                    //Nachgusstemperatur
        }                               //für Sollwert
        //lcd.clear();
    }
}
//-----------------------------------------------------------------


// Funktion nur Temperaturregelung---------------------------------
void funktion_temperatur()      //Modus=1 bzw.2
{
    if (anfang) {
        lcd.clear();
        anfang = false;
    }

    sollwert = drehen;
    switch (modus) {
        case MANUELL:
            print_lcd("Manuell", LEFT, 0);
            break;

        case NACHGUSS:
            print_lcd("Nachguss", LEFT, 0);
            break;

        default:
            break;
    }

    if ((modus == MANUELL) && (isttemp >= sollwert)) { //Manuell -> Sollwert erreicht
        rufmodus = MANUELL;                //Abbruch nach Rufalarm
        modus = BRAUMEISTERRUFALARM;
        //regelung = REGL_AUS;              //Regelung aus
        //heizung = false;               //Heizung aus
        y = 0;
        braumeister[y] = BM_ALARM_SIGNAL;        // Ruf und Abbruch
    }

    if ((modus == NACHGUSS) && (isttemp >= sollwert) && (nachgussruf == false)) { //Nachguss -> Sollwert erreicht
        nachgussruf = true;
        rufmodus = NACHGUSS;          //Rufalarm
        modus = BRAUMEISTERRUFALARM;
        y = 0;
        braumeister[y] = BM_ALARM_SIGNAL;        //nur Ruf und weiter mit Regelung
    }
}

// Funktion Eingabe der Rastanzahl------------------------------------
void funktion_rastanzahl()          //Modus=19
{
    if (anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        print_lcd("Rasten", LEFT, 1);

        drehen = rasten;
        anfang = false;
    }

    drehen = constrain(drehen, 1, 5);
    rasten = drehen;

    //Vorgabewerte bei verschiedenen Rasten
    switch (drehen) {
        case 1:
            rastTemp[1] = 67;
            rastZeit[1] = 60;
            maischtemp = 65;
            break;

        case 2:
            rastTemp[1] = 62;
            rastZeit[1] = 30;
            rastTemp[2] = 72;
            rastZeit[2] = 35;
            maischtemp = 55;
            break;

        case 3:
            rastTemp[1] = 43;
            rastZeit[1] = 20;
            rastTemp[2] = 62;
            rastZeit[2] = 30;
            rastTemp[3] = 72;
            rastZeit[3] = 30;
            maischtemp = 45;
            break;

        case 4:
            rastTemp[1] = 45;
            rastZeit[1] = 10;
            rastTemp[2] = 52;
            rastZeit[2] = 10;
            rastTemp[3] = 65;
            rastZeit[3] = 30;
            rastTemp[4] = 72;
            rastZeit[4] = 30;
            maischtemp = 37;
            break;

        case 5:
            rastTemp[1] = 35;
            rastZeit[1] = 20;
            rastTemp[2] = 40;
            rastZeit[2] = 20;
            rastTemp[3] = 55;
            rastZeit[3] = 15;
            rastTemp[4] = 64;
            rastZeit[4] = 35;
            rastTemp[5] = 72;
            rastZeit[5] = 25;
            maischtemp = 30;
            break;

        default:
            ;
    }

    printNumI_lcd(rasten, 19, 1);

    warte_und_weiter(EINGABE_MAISCHTEMP);
}
//------------------------------------------------------------------


// Funktion Maischtemperatur-----------------------------------------
void funktion_maischtemperatur()      //Modus=20
{

    if (anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        drehen = maischtemp;
        anfang = false;
    }

    drehen = constrain( drehen, 10, 105);
    maischtemp = drehen;

    print_lcd("Maischtemp", LEFT, 1);
    printNumF_lcd(int(maischtemp), 15, 1);
    lcd.setCursor(19, 1);
    lcd.write(8);

    warte_und_weiter(EINGABE_RAST_TEMP);
}
//------------------------------------------------------------------


// Funktion Rasteingabe Temperatur----------------------------------
void funktion_rasteingabe()      //Modus=21
{

    if (anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        drehen = rastTemp[x];
        anfang = false;
    }

    drehen = constrain( drehen, 9, 105);
    rastTemp[x] = drehen;

    printNumI_lcd(x, LEFT, 1);
    print_lcd(". Rast", 1, 1);
    printNumF_lcd(int(rastTemp[x]), 15, 1);
    lcd.setCursor(19, 1);
    lcd.write(8);

    warte_und_weiter(EINGABE_RAST_ZEIT);
}
//------------------------------------------------------------------


// Funktion Rasteingabe Zeit----------------------------------------
void funktion_zeiteingabe()      //Modus=22
{

    if (anfang) {
        fuenfmindrehen = rastZeit[x];
        anfang = false;
    }

    fuenfmindrehen = constrain( fuenfmindrehen, 1, 99);
    rastZeit[x] = fuenfmindrehen;

    print_lcd_minutes(rastZeit[x], RIGHT, 2);

    warte_und_weiter(EINGABE_BRAUMEISTERRUF);
}
//------------------------------------------------------------------


// Funktion Braumeister---------------------------------------------
void funktion_braumeister() //Modus=24
{
    if (anfang) {
        drehen = (int)braumeister[x];
        anfang = false;
    }

    //delay(200);

    drehen = constrain( drehen, BM_ALARM_MIN, BM_ALARM_MAX);
    braumeister[x] = (BM_ALARM_MODE)drehen;

    print_lcd("Ruf", LEFT, 2);

    switch (braumeister[x]) {
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
        if (x < rasten) {
            x++;
            modus = EINGABE_RAST_TEMP;           //Sprung zur Rasttemperatureingabe
        } else {
            x = 1;
            modus = EINGABE_ENDTEMP;           //Sprung zur Rastzeiteingabe
        }
    }
}
//------------------------------------------------------------------


// Funktion Ende Temperatur-----------------------------------------
void funktion_endtempeingabe()      //Modus=25
{
    if (anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        drehen = endtemp;
        anfang = false;
    }

    drehen = constrain( drehen, 10, 80);
    endtemp = drehen;

    print_lcd("Endtemperatur", LEFT, 1);
    printNumF_lcd(int(endtemp), 15, 1);
    lcd.setCursor(19, 1);
    lcd.write(8);

    warte_und_weiter(AUTO_START);
}
//------------------------------------------------------------------

// Funktion Startabfrage--------------------------------------------
void funktion_startabfrage(MODUS naechsterModus, char *title)
{
    if (anfang) {
        lcd.clear();
        print_lcd(title, LEFT, 0);
        anfang = false;
        altsekunden = millis();
    }

    if (millis() >= (altsekunden + 1000)) {
        print_lcd("       ", CENTER, 2);
        if (millis() >= (altsekunden + 1500))
        { altsekunden = millis(); }
    } else {
        print_lcd("Start ?", CENTER, 2);
    }

    warte_und_weiter(naechsterModus);
}

//------------------------------------------------------------------


// Funktion Automatik Maischtemperatur---------------------------------
void funktion_maischtemperaturautomatik()      //Modus=27
{
    if (anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Maischen", RIGHT, 0);
        drehen = maischtemp;    //Zuordnung Encoder
        anfang = false;
    }

    drehen = constrain( drehen, 10, 105);
    maischtemp = drehen;
    sollwert = maischtemp;

    if (isttemp >= sollwert) { // Sollwert erreicht ?
        rufmodus = AUTO_RAST_TEMP;
        y = 0;
        braumeister[y] = BM_ALARM_WAIT;
        modus = BRAUMEISTERRUFALARM;
    }
}
//------------------------------------------------------------------


// Funktion Automatik Temperatur------------------------------------
void funktion_tempautomatik()      //Modus=28
{
    if (anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        printNumI_lcd(x, 13, 0);
        print_lcd(". Rast", RIGHT, 0);

        drehen = rastTemp[x];
        anfang = false;
        wartezeit = millis() + 60000;  // sofort aufheizen
    }

    drehen = constrain( drehen, 10, 105);

    rastTemp[x] = drehen;
    sollwert = rastTemp[x];

    if (isttemp >= sollwert) { // Sollwert erreicht ?
        modus = AUTO_RAST_ZEIT;              //zur Zeitautomatik
        anfang = true;
    }
}
//------------------------------------------------------------------


// Funktion Automatik Zeit------------------------------------------
void funktion_zeitautomatik()      //Modus=29
{
    if (anfang) {
        drehen = rastZeit[x];              //Zuordnung für Encoder
    }

    print_lcd_minutes(rastZeit[x], RIGHT, 2);

    // Zeitzählung---------------
    if (anfang) {
        print_lcd("Set Time", LEFT, 3);

        setTime(00, 00, 00, 00, 01, 01); //.........Sekunden auf 0 stellen

        delay(400); //test

        sekunden = second();  //aktuell Sekunde abspeichern für die Zeitrechnung
        minutenwert = minute(); //aktuell Minute abspeichern für die Zeitrechnung
        stunden = hour();     //aktuell Stunde abspeichern für die Zeitrechnung

        print_lcd("            ", 0, 3);
        anfang = false;
        print_lcd("00:00", LEFT, 2);
    }


    if (sekunden < 10) {
        printNumI_lcd(sekunden, 4, 2);
        if (sekunden == 0) {
            print_lcd("0", 3, 2);
        }
    } else {
        printNumI_lcd(sekunden, 3, 2);
    }

    if (stunden == 0) {
        minuten = minutenwert;
    } else {
        minuten = ((stunden * 60) + minutenwert);
    }

    if (minuten < 10) {
        printNumI_lcd(minuten, 1, 2);
    } else {
        printNumI_lcd(minuten, 0, 2);
    }
    // Ende Zeitzählung---------------------

    drehen = constrain( drehen, 10, 105);
    rastZeit[x] = drehen;   //Encoderzuordnung

    if (minuten >= rastZeit[x]) { // Sollwert erreicht ?
        anfang = true;
        y = x;
        if (x < rasten) {
            modus = AUTO_RAST_TEMP;              // zur Temperaturregelung
            x++;                    // nächste Stufe
        } else {
            x = 1;                              //Endtemperatur
            modus = AUTO_ENDTEMP;                          //Endtemperatur
        }

        if (braumeister[y] != BM_ALARM_AUS) {
            rufmodus = modus;
            modus = BRAUMEISTERRUFALARM;
        }
    }
}
//------------------------------------------------------------------


// Funktion Automatik Endtemperatur---------------------------------
void funktion_endtempautomatik()      //Modus=30
{
    if (anfang) {
        lcd.clear();
        print_lcd("Auto", LEFT, 0);
        print_lcd("Endtemp", RIGHT, 0);
        drehen = endtemp;    //Zuordnung Encoder
        anfang = false;
    }

    drehen = constrain( drehen, 10, 105);
    endtemp = drehen;
    sollwert = endtemp;

    if (isttemp >= sollwert) { // Sollwert erreicht ?
        rufmodus = ABBRUCH;         //Abbruch
        modus = BRAUMEISTERRUFALARM;
        regelung = REGL_AUS;              //Regelung aus
        heizung = false;               //Heizung aus
        y = 0;
        braumeister[y] = BM_ALARM_WAIT;
    }
}
//------------------------------------------------------------------


// Funktion braumeisterrufalarm---------------------------------------
void funktion_braumeisterrufalarm()      //Modus=31
{
    if (anfang) {
        rufsignalzeit = millis();
        anfang = false;
    }

    if (millis() >= (altsekunden + 1000)) { //Bliken der Anzeige und RUF
        print_lcd("          ", LEFT, 3);
        beeperOn(false);
        if (millis() >= (altsekunden + 1500)) {
            altsekunden = millis();
            pause++;
        }
    } else {
        print_lcd("RUF", LEFT, 3);
        if (pause <= 4) {
            beeperOn(true);
        }
        if (pause > 8) {
            pause = 0;
        }
    }                                   //Bliken der Anzeige und RUF


    if ((pause == 4) || (pause == 8)) {   //Funkalarm schalten
        funkrufOn(true);;    //Funkalarm ausschalten
    } else {
        funkrufOn(false);;   // Funkalarm ausschalten
    }

    //20 Sekunden Rufsignalisierung wenn "Ruf Signal"
    if (braumeister[y] == BM_ALARM_SIGNAL && millis() >= (rufsignalzeit + 20000)) {
        anfang = true;
        pause = 0;
        beeperOn(false);   // Alarm ausschalten
        funkrufOn(false);;   // Funkalarm ausschalten
        modus = rufmodus;
        einmaldruck = false;
    }
    //weiter mit Programmablauf

    if (warte_und_weiter(BRAUMEISTERRUF)) {
        pause = 0;
        beeperOn(false);   // Alarm ausschalten
        funkrufOn(false);   // Funkalarm ausschalten
        if (braumeister[y] == BM_ALARM_SIGNAL) {
            print_lcd("   ", LEFT, 3);
            modus = rufmodus;
        }
    }
}
//------------------------------------------------------------------


// Funktion braumeisterruf------------------------------------------
void funktion_braumeisterruf()      //Modus=32
{
    if (anfang) {
        anfang = false;
    }

    if (millis() >= (altsekunden + 1000)) {
        print_lcd("        ", LEFT, 3);
        if (millis() >= (altsekunden + 1500)) {
            altsekunden = millis();
        }
    } else {
        print_lcd("weiter ?", LEFT, 3);
    }

    if (warte_und_weiter(rufmodus)) {
        print_lcd("        ", LEFT, 3);     //Text "weiter ?" löschen
        print_lcd("             ", RIGHT, 3); //Löscht Text bei
        sensorfehler = false;                         //Sensorfehler oder Alarmtest
        delay(500);     //kurze Wartezeit, damit nicht
        //durch unbeabsichtigtes Drehen
        //der nächste Vorgabewert
        //verstellt wird
    }
}
//------------------------------------------------------------------

// Funktion Hysterese-------------------------------------------------
void funktion_hysterese() //Modus 10
{
    if (anfang) {
        lcd.clear();
        print_lcd("Setup", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);

        fuenfmindrehen = hysteresespeicher;
        anfang = false;
    }

    fuenfmindrehen = constrain( fuenfmindrehen, 0, 40); //max. 4,0 Sekunden Hysterese
    hysteresespeicher = fuenfmindrehen; //5min-Sprünge

    printNumF_lcd(float(hysteresespeicher) / 10, RIGHT, 1);

    if (warte_und_weiter(HAUPTSCHIRM)) {
        EEPROM.write(0, hysteresespeicher);
    }
}

// Funktion Kochzeit-------------------------------------------------
void funktion_kochzeit()      //Modus=40
{
    if (anfang) {
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        print_lcd("Zeit", LEFT, 1);

        fuenfmindrehen = kochzeit;
        anfang = false;
    }

    fuenfmindrehen = constrain( fuenfmindrehen, 20, 180);
    kochzeit = fuenfmindrehen; //5min-Sprünge

    print_lcd_minutes( kochzeit, RIGHT, 1);

    warte_und_weiter(EINGABE_HOPFENGABEN_ANZAHL);
}
//------------------------------------------------------------------

// Funktion Anzahl der Hopfengaben------------------------------------------
void funktion_anzahlhopfengaben()      //Modus=41
{
    if (anfang) {
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        print_lcd("Hopfengaben", LEFT, 1);

        drehen = hopfenanzahl;
        anfang = false;
    }

    drehen = constrain(drehen, 1, 5);
    hopfenanzahl = drehen;

    printNumI_lcd(hopfenanzahl, RIGHT, 1);

    warte_und_weiter(EINGABE_HOPFENGABEN_ZEIT);
}
//------------------------------------------------------------------

// Funktion Hopfengaben-------------------------------------------
void funktion_hopfengaben()      //Modus=42
{
    if (anfang) {
        x = 1;
        fuenfmindrehen = hopfenZeit[x];
        anfang = false;
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
    }

    printNumI_lcd(x, LEFT, 1);
    print_lcd(". Hopfengabe", 1, 1);
    print_lcd("nach", LEFT, 2);

    fuenfmindrehen = constrain(fuenfmindrehen, hopfenZeit[(x - 1)] + 5, kochzeit - 5);
    hopfenZeit[x] = fuenfmindrehen;

    print_lcd_minutes(hopfenZeit[x], RIGHT, 2);

    if (warte_und_weiter(modus)) {
        if (x < hopfenanzahl) {
            x++;
            fuenfmindrehen = hopfenZeit[x];
            print_lcd("  ", LEFT, 1);
            print_lcd("   ", 13, 2);
            delay(400);
            anfang = false; // nicht auf Anfang zurück
        } else {
            x = 1;
            modus = KOCHEN_START_FRAGE;
        }
    }
}
//------------------------------------------------------------------


// Funktion Kochenaufheizen-------------------------------------------
void funktion_kochenaufheizen()      //Modus=44
{
    if (anfang) {
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);
        anfang = false;
    }

    if (isttemp >= 98) {
        print_lcd("            ", RIGHT, 0);
        print_lcd("Kochbeginn", CENTER, 1);
        beeperOn(true);
        delay(500);
        beeperOn(false);
        anfang = true;
        modus = KOCHEN_AUTO_LAUF;
    } else {
        print_lcd("Aufheizen", RIGHT, 0);
    }
}
//------------------------------------------------------------------


// Funktion Hopfengaben Benachrichtigung------------------------------------------
void funktion_hopfenzeitautomatik()      //Modus=45
{
    if (anfang) {
        x = 1;
        lcd.clear();
        print_lcd("Kochen", LEFT, 0);

        setTime(00, 00, 00, 00, 01, 01); //.........Sekunden auf 0 stellen

        print_lcd_minutes(kochzeit, RIGHT, 0);

        sekunden = second();  //aktuell Sekunde abspeichern für die Zeitrechnung
        minutenwert = minute(); //aktuell Minute abspeichern für die Zeitrechnung
        stunden = hour();     //aktuell Stunde abspeichern für die Zeitrechnung

        anfang = false;
        print_lcd("00:00", 11, 1);
    }


    if (x <= hopfenanzahl) {
        printNumI_lcd(x, LEFT, 2);
        print_lcd(". Gabe bei ", 1, 2);
        print_lcd_minutes(hopfenZeit[x], RIGHT, 2);
    } else {
        print_lcd("                    ", 0, 2);
    }

    print_lcd("min", RIGHT, 1);

    if (sekunden < 10) {
        printNumI_lcd(sekunden, 15, 1);
        if (sekunden) {
            print_lcd("0", 14, 1);
        }
    } else {
        printNumI_lcd(sekunden, 14, 1);
    }

    minuten = ((stunden * 60) + minutenwert);
    if (minuten < 10) {
        printNumI_lcd(minuten, 12, 1);
    }

    if ((minuten >= 10) && (minuten < 100)) {
        printNumI_lcd(minuten, 11, 1);
    }

    if (minuten >= 100) {
        printNumI_lcd(minuten, 10, 1);
    }

    if ((minuten == hopfenZeit[x]) && (x <= hopfenanzahl)) {  // Hopfengabe
        //Alarm -----
        zeigeH = false;
        if (millis() >= (altsekunden + 1000)) { //Bliken der Anzeige und RUF
            print_lcd("   ", LEFT, 3);
            beeperOn(false);
            if (millis() >= (altsekunden + 1500)) {
                altsekunden = millis();
                pause++;
            }
        } else {
            print_lcd("RUF", LEFT, 3);
            if (pause <= 4) {
                beeperOn(true);
            }
            if (pause > 8) {
                pause = 0;
            }
        }                                   //Bliken der Anzeige und RUF


        if ((pause == 4) || (pause == 8)) {   //Funkalarm schalten
            funkrufOn(true);    //Funkalarm einschalten
        } else {
            funkrufOn(false);   // Funkalarm ausschalten
        }
        //-----------


        if (warte_und_weiter(modus)) {
            pause = 0;
            zeigeH = true;
            print_lcd("   ", LEFT, 3);
            beeperOn(false);   // Alarm ausschalten
            funkrufOn(false);   // Funkalarm ausschalten
            x++;
            anfang = false; // nicht zurücksetzen!!!
        }
    }

    if ((minuten > hopfenZeit[x]) && (x <= hopfenanzahl)) {  // Alarmende nach 1 Minute
        pause = 0;
        beeperOn(false);   // Alarm ausschalten
        funkrufOn(false);   // Funkalarm ausschalten
        x++;
    }

    if (minuten >= kochzeit) {   //Kochzeitende
        rufmodus = ABBRUCH;                //Abbruch nach Rufalarm
        modus = BRAUMEISTERRUFALARM;
        regelung = REGL_AUS;              //Regelung aus
        heizung = false;               //Heizung aus
        y = 0;
        braumeister[y] = BM_ALARM_WAIT;
    }
}
//------------------------------------------------------------------


// Funktion Timer-------------------------------------------------
void funktion_timer()      //Modus=60
{
    if (anfang) {
        lcd.clear();
        print_lcd("Timer", LEFT, 0);
        print_lcd("Eingabe", RIGHT, 0);
        print_lcd("Zeit", LEFT, 2);

        drehen = timer;
        anfang = false;
    }

    drehen = constrain( drehen, 1, 99);
    timer = drehen;

    print_lcd_minutes(timer, RIGHT, 2);

    warte_und_weiter(TIMERLAUF);
}
//------------------------------------------------------------------


// Funktion Timerlauf-------------------------------------------------
void funktion_timerlauf()      //Modus=61
{
    if (anfang) {
        drehen = timer;

        anfang = false;
        lcd.clear();
        print_lcd("Timer", LEFT, 0);
        print_lcd("Set Time", RIGHT, 0);

        setTime(00, 00, 00, 00, 01, 01); //.........Sekunden auf 0 stellen

        delay(400); //test
        print_lcd("         ", RIGHT, 0);

        sekunden = second();  //aktuell Sekunde abspeichern für die Zeitrechnung
        minutenwert = minute(); //aktuell Minute abspeichern für die Zeitrechnung
        stunden = hour();     //aktuell Stunde abspeichern für die Zeitrechnung

        print_lcd("00:00", LEFT, 2);
    }

    drehen = constrain(drehen, 1, 99);
    timer = drehen;

    print_lcd_minutes(timer, RIGHT, 2);

    if (sekunden < 10) {
        printNumI_lcd(sekunden, 4, 2);
        if (sekunden == 0) {
            print_lcd("0", 3, 2);
        }
    } else {
        printNumI_lcd(sekunden, 3, 2);
    }

    minuten = ((stunden * 60) + minutenwert);
    if (minuten < 10) {
        printNumI_lcd(minuten, 1, 2);
    } else {
        printNumI_lcd(minuten, 0, 2);
    }

    if (minuten >= timer) {   //Timerende
        rufmodus = ABBRUCH;                //Abbruch nach Rufalarm
        modus = BRAUMEISTERRUFALARM;
        regelung = REGL_AUS;              //Regelung aus
        heizung = false;               //Heizung aus
        y = 0;
        braumeister[y] = BM_ALARM_WAIT;
    }
}
//------------------------------------------------------------------

// Funktion Abbruch-------------------------------------------------
void funktion_abbruch()       // Modus 80
{
    regelung = REGL_AUS;
    heizung = false;
    wartezeit = -60000;
    heizungOn(false);
    ruehrerOn(false);
    beeperOn(false);  // ausschalten
    funkrufOn(false);   // ausschalten
    anfang = true;                     //Daten zurücksetzen
    lcd.clear();                //Rastwerteeingaben
    rufmodus = HAUPTSCHIRM;                   //bleiben erhalten
    x = 1;                          //bei
    y = 1;                          //asm volatile ("  jmp 0");
    n = 0;                          //wird alles
    einmaldruck = false;            //zurückgesetetzt
    nachgussruf = false;
    pause = 0;
    drehen = sollwert;          //Zuweisung für Funktion Temperaturregelung

    if (millis() >= (abbruchtaste + 5000)) { //länger als 5 Sekunden drücken
        modus = SETUP;                    //Setup
    } else {
        modus = HAUPTSCHIRM;                    //Hauptmenue
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
    while (true);
}


