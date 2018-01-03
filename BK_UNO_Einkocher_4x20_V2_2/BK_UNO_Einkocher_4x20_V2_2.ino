#pragma GCC diagnostic ignored "-Wwrite-strings"

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ClickEncoder.h> // https://github.com/soligen2010/encoder
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TimeLib.h>  // http://playground.arduino.cc/code/time

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <FS.h>
#include <Ticker.h>

#include "config.h"
#include "global.h"
#include "html.h"

LiquidCrystal_I2C lcd(0x27, 20, 4); //# 0x27=proto / 0x3f=box
OneWire oneWire(oneWirePin);
DallasTemperature DS18B20(&oneWire);
DeviceAddress insideThermometer;
Ticker ticker;
ClickEncoder encoder = ClickEncoder(encoderPinA, encoderPinB, tasterPin, ENCODER_STEPS_PER_NOTCH);

ESP8266WebServer HTTP(80);

String my_ssid;
String my_psk;

byte degC[8] = {
  B01000, B10100, B01000, B00111, B01000, B01000, B01000, B00111
};

enum MODUS {HAUPTSCHIRM = 0,
            MANUELL, NACHGUSS, MAISCHEN,
            SETUP_MENU, SETUP_HYSTERESE, SETUP_KOCHSCHWELLE,
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
void print_lcd (char *st, int x, int y);
void printNumI_lcd(int num, int x, int y);
void printNumF_lcd (double num, int x, int y, byte dec = 1, int length = 0);

#ifdef DEBUG
unsigned long serwartezeit = 0;
#endif

volatile int number = 0;
volatile int oldnumber = 0;
boolean ButtonPressed = false;

int val = 0;
volatile int drehen;

boolean anfang = true;
unsigned long altsekunden;
REGEL_MODE regelung = REGL_AUS;
boolean heizung = false;
boolean sensorfehler = false;
float hysterese = 0;
byte hysteresespeicher = 5;
unsigned long wartezeit = -60000;
float sensorwert;
float isttemp = 20;                                   //Vorgabe 20 damit Sensorfehler am Anfang nicht anspricht
MODUS modus = HAUPTSCHIRM;
MODUS rufmodus = HAUPTSCHIRM;
unsigned long rufsignalzeit = 0;
boolean nachgussruf = false;                          //Signal wenn Nachgusstemp oder manuelle Maischtemp  erreicht
int x = 1;                                            //aktuelle Rast Nummer
int y = 1;                                            //Übergabewert von x für Braumeisterruf
int n = 0;                                            //Counter Messungserhöhung zur Fehlervermeidung
int pause = 0;
boolean zeigeH = false;

uint32_t lastService = 0;

int sekunden = 0;
int minuten = 0;
int minutenwert = 0;
int stunden = 0;

boolean hendi_special = true;
bool schwelle_erreicht = false;
byte kschwelle = KOCHSCHWELLE;

//Vorgabewerte zur ersten Einstellung-------------------------------------------
int sollwert = 20;
int maischtemp = 38;

int rasten = 1;

int rastTemp[] = {
  0, 50, 64, 72, 72, 72, 72, 72
};
int rastZeit[] = {
  0, 40, 30, 20, 15, 20, 20, 20
};

BM_ALARM_MODE braumeister[] = {
  BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_SIGNAL, BM_ALARM_AUS, BM_ALARM_AUS, BM_ALARM_AUS
};

int endtemp = 78;

int kochzeit = 90;

int hopfenanzahl = 2;

int hopfenZeit[] = {
  0, 10, 80, 80, 80, 40, 40
};

int timer = 10;

void setup()
{
#ifdef DEBUG
  Serial.begin(115200);
#endif

  SerialOut(F("\nBK Start"));
  SerialOut("\nFW " FIRMWAREVERSION);
  SerialOut(ESP.getSdkVersion());

  lcd.init();
  lcd.createChar(8, degC);
  lcd.backlight();
  lcd.clear();
  lcd.noCursor();

  print_lcd("BK V3.0 - LC2004", LEFT, 0);
  print_lcd("Arduino", LEFT, 1);
  print_lcd(":)", RIGHT, 2);
  print_lcd("realholgi & fg100", RIGHT, 3);
  delay (500);

  drehen = sollwert;

  pinMode(heizungPin, OUTPUT);
  pinMode(beeperPin, OUTPUT);

  heizungOn(false);
  beeperOn(false);

  encoder.setButtonHeldEnabled(true);
  encoder.setDoubleClickEnabled(true);
  //encoder.setButtonOnPinZeroEnabled(true);


#ifndef DEBUG
  for (x = 1; x <= 3; x++) {
    beeperOn(true);
    delay(200);
    beeperOn(false);
    delay(200);
  }
#endif

  x = 1;

  lcd.clear();

  DS18B20.begin();
  DS18B20.getAddress(insideThermometer, 0);
  DS18B20.setResolution(insideThermometer, RESOLUTION);   // set the resolution to 9 bit

  bool _validConf = readConfig();
  if (!_validConf) {
    SerialOut(F("ERROR config corrupted"));
  }

  bool _wifiCred = (WiFi.SSID() != "");
  uint8_t c = 0;
  if (!_wifiCred) {
    WiFi.begin();
  }
  while (!_wifiCred)
  {
    if (c > 10)
      break;
    SerialOut(F("."), false);
    delay(100);
    c++;
    _wifiCred = (WiFi.SSID() != "");
  }
  if (!_wifiCred) {
    SerialOut(F("ERROR no Wifi credentials. Providing default..."));
    my_ssid = WIFI_SSID;
    my_psk = WIFI_PSK;
  }

  // Hysterese default
  if (hysteresespeicher > 40 || hysteresespeicher == 0) (hysteresespeicher = 5);
  hysterese = hysteresespeicher;
  hysterese = hysterese / 10;

  // Kochschwelle default
  if (kschwelle > 100 || kschwelle == 0) (kschwelle = KOCHSCHWELLE);

  watchdogSetup();

  setupWebserver();
  setupWIFI();

  ticker.attach_ms(1, encoderTicker);
}


//loop=============================================================
void loop()
{
  HTTP.handleClient();
  MDNS.update();

  sekunden = second();
  minutenwert = minute();
  stunden = hour();



  DS18B20.requestTemperatures();
  sensorwert = DS18B20.getTempC(insideThermometer);
  // sensorwert = DS18B20.getTempCByIndex(0); // getTempCByIndex(0)

  if ((sensorwert != isttemp) && (n < 5)) { // Messfehlervermeidung des Sensorwertes
    n++;
  }
  else {
    isttemp = sensorwert;
    n = 0;
  }


  // Sensorfehler -------------------------------------------------
  // Sensorfehler -127 => VCC fehlt
  // Sensorfehler 85.00 => interner Sensorfehler ggf. Leitung zu lang
  //                       nicht aktiviert
  // Sensorfehler 0.00 => Datenleitung oder GND fehlt

  if (regelung == REGL_MAISCHEN) {
    if ((int)isttemp == DEVICE_DISCONNECTED_C || (int)isttemp == 0 || isttemp == 85.0) {
      if (!sensorfehler) {
        rufmodus = modus;
        print_lcd("Sensorfehler", RIGHT, 2);
        regelung = REGL_AUS;
        heizung = false;
        sensorfehler = true;
        modus = BRAUMEISTERRUFALARM;
      }
    } else {
      sensorfehler = false;
    }
  }

  // Temperaturanzeige Istwert
  if (!sensorfehler) {
    print_lcd("ist ", 10, 3);
    printNumF_lcd(float(isttemp), 15, 3);
    lcd.setCursor(19, 3);
    lcd.write(8);
  } else {
    print_lcd("   ERR", RIGHT, 3);
  }

  // Heizregelung
  if (regelung == REGL_MAISCHEN) {
    print_lcd("soll ", 9, 1);
    printNumF_lcd(int(sollwert), 15, 1);
    lcd.setCursor(19, 1);
    lcd.write(8);

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

    // setzt Hysterese beim Hochfahren auf 0.5°C unter sollwert
    if ((isttemp <= (sollwert - 4)) && (heizung == 1)) {
      hysterese = hysteresespeicher;
      hysterese = hysterese / 10;
    }

    // Ausschalten wenn Sollwert-Hysterese erreicht und dann Wartezeit
    if (heizung && (isttemp >= (sollwert - hysterese)) && (millis() >= (wartezeit + 60000))) {
      heizung = false;
      hysterese = 0;
      wartezeit = millis();
    }

    // Einschalten wenn kleiner Sollwert und dann Wartezeit
    if ((!heizung) && (isttemp <= (sollwert - 0.5)) && (millis() >= (wartezeit + 60000))) {
      heizung = true;
      hysterese = 0;
      wartezeit = millis();
    }

    // Ausschalten vor der Wartezeit, wenn Sollwert um 0,5 überschritten
    if (heizung && (isttemp >= (sollwert + 0.5))) {
      heizung = false;
      hysterese = 0;
      wartezeit = millis();
    }
  }

  //Kochen => dauernd ein----------------------------------------------
  if (regelung == REGL_KOCHEN) {
    heizung = true;
    if (minuten == 85 && hendi_special) {    // HENDI-Spezial, welche nach 90 min abschaltet
      heizung = false;
      delay(1000);
      heizung = true;
      hendi_special = false;
    }
  }

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

#ifdef DEBUG
  if (millis() >= (serwartezeit + 1000)) {
    SerialOut(millis(), false);
    SerialOut(F("\t"), false);
    SerialOut(isttemp);
    serwartezeit = millis();
  }
#endif

  getButton();

  zeigeH = true;
  encoder.setAccelerationEnabled(true);

  switch (modus) {
    case HAUPTSCHIRM:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_hauptschirm();
      break;

    case MANUELL:
      regelung = REGL_MAISCHEN;
      zeigeH = true;
      encoder.setAccelerationEnabled(true);
      funktion_temperatur();
      break;

    case MAISCHEN:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_maischmenue();
      break;

    case NACHGUSS:
      regelung = REGL_MAISCHEN;
      zeigeH = true;
      encoder.setAccelerationEnabled(true);
      funktion_temperatur();
      break;

    case SETUP_MENU:
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_setupmenu();
      break;

    case SETUP_HYSTERESE:
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_hysterese();
      break;

    case SETUP_KOCHSCHWELLE:
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_kochschwelle();
      break;

    case ALARMTEST:
      regelung = REGL_AUS;
      zeigeH = false;
      rufmodus = HAUPTSCHIRM;
      modus = BRAUMEISTERRUFALARM;
      print_lcd("Alarmtest", RIGHT, 0);
      break;

    case EINGABE_RAST_ANZ:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_rastanzahl();
      break;

    case EINGABE_MAISCHTEMP:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(true);
      funktion_maischtemperatur();
      break;

    case EINGABE_RAST_TEMP:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(true);
      funktion_rasteingabe();
      break;

    case EINGABE_RAST_ZEIT:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(true);
      funktion_zeiteingabe();
      break;

    case EINGABE_BRAUMEISTERRUF:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_braumeister();
      break;

    case EINGABE_ENDTEMP:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(true);
      funktion_endtempeingabe();
      break;

    case AUTO_START:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_startabfrage(AUTO_MAISCHTEMP, "Auto");
      break;

    case AUTO_MAISCHTEMP:
      regelung = REGL_MAISCHEN;
      zeigeH = true;
      encoder.setAccelerationEnabled(true);
      funktion_maischtemperaturautomatik();
      break;

    case AUTO_RAST_TEMP:
      regelung = REGL_MAISCHEN;
      zeigeH = true;
      encoder.setAccelerationEnabled(true);
      funktion_tempautomatik();
      break;

    case AUTO_RAST_ZEIT:
      regelung = REGL_MAISCHEN;
      zeigeH = true;
      encoder.setAccelerationEnabled(true);
      funktion_zeitautomatik();
      break;

    case AUTO_ENDTEMP:
      regelung = REGL_MAISCHEN;
      zeigeH = true;
      encoder.setAccelerationEnabled(true);
      funktion_endtempautomatik();
      break;

    case BRAUMEISTERRUFALARM:
      zeigeH = false;
      funktion_braumeisterrufalarm();
      break;

    case BRAUMEISTERRUF:
      zeigeH = false;
      funktion_braumeisterruf();
      break;

    case KOCHEN:
      zeigeH = false;
      encoder.setAccelerationEnabled(true);
      funktion_kochzeit();
      break;

    case EINGABE_HOPFENGABEN_ANZAHL:
      zeigeH = false;
      encoder.setAccelerationEnabled(false);
      funktion_anzahlhopfengaben();
      break;

    case EINGABE_HOPFENGABEN_ZEIT:
      zeigeH = false;
      encoder.setAccelerationEnabled(true);
      funktion_hopfengaben();
      break;

    case KOCHEN_START_FRAGE:
      zeigeH = false;
      funktion_startabfrage(KOCHEN_AUFHEIZEN, "Kochen");
      break;

    case KOCHEN_AUFHEIZEN:
      regelung = REGL_KOCHEN;
      zeigeH = true;
      funktion_kochenaufheizen();
      break;

    case KOCHEN_AUTO_LAUF:
      regelung = REGL_KOCHEN;
      zeigeH = true;
      funktion_hopfenzeitautomatik();
      break;

    case TIMER:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(true);
      funktion_timer();
      break;

    case TIMERLAUF:
      regelung = REGL_AUS;
      zeigeH = false;
      encoder.setAccelerationEnabled(true);
      funktion_timerlauf();
      break;

    case ABBRUCH:
      zeigeH = true;
      funktion_abbruch();
      break;
  }

  wdt_reset();
}

void encoderTicker() {
  encoder.service();
  drehen += encoder.getValue();
}

boolean getButton()
{
  ButtonPressed = false;

  ClickEncoder::Button b = encoder.getButton();
  if (b != ClickEncoder::Open) {
    switch (b) {
      case ClickEncoder::Pressed:
        break;

      case ClickEncoder::Held:
        modus = ABBRUCH;
        break;

      case ClickEncoder::Released:
        ButtonPressed = false;
        break;
      case ClickEncoder::Clicked:
        ButtonPressed = true;
        break;

      case ClickEncoder::DoubleClicked:
        saveConfig();
        break;
    }
  }

  return ButtonPressed;
}

boolean einmaldruck = false;

boolean warte_und_weiter(MODUS naechsterModus)
{
  if (!ButtonPressed) {
    einmaldruck = true;
  }
  if (einmaldruck == true && ButtonPressed) {
    einmaldruck = false;
    modus = naechsterModus;
    anfang = true;
    return true;
  }
  return false;
}

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

void funktion_hauptschirm()
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
      rufmodus = SETUP_MENU;
      break;
  }

  if (warte_und_weiter(rufmodus)) {
    lcd.clear();
  }
}

void funktion_maischmenue()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Automatik", 2, 0);
    print_lcd("Manuell", 2, 1);
    print_lcd("Nachguss", 2, 2);
    drehen = 0;
    anfang = false;
  }

  drehen = constrain(drehen, 0, 2);

  menu_zeiger(drehen);
  switch (drehen) {
    case 0:
      rufmodus = AUTOMATIK ;
      break;
    case 1:
      rufmodus = MANUELL;
      break;
    case 2:
      rufmodus = NACHGUSS;
      break;
  }

  if (warte_und_weiter(rufmodus)) {
    if (modus == MANUELL) {
      //Übergabe an Modus1
      drehen = (int)isttemp + 10; // Vorgabewert 10°C über IstWert
    }
    if (modus == NACHGUSS) {
      //Übergabe an Modus2
      drehen = endtemp;  // Vorgabewert Nachgusstemperatur
    }
  }
}

void funktion_setupmenu()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Kochschwelle", 2, 0);
    print_lcd("Hysterese", 2, 1);
    drehen = 0;
    anfang = false;
  }

  drehen = constrain(drehen, 0, 1);

  menu_zeiger(drehen);
  switch (drehen) {
    case 0:
      rufmodus = SETUP_KOCHSCHWELLE;
      break;
    case 1:
      rufmodus = SETUP_HYSTERESE;
      break;
  }

  if (warte_und_weiter(rufmodus)) {
    lcd.clear();
  }
}

void funktion_temperatur()
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

  if ((modus == NACHGUSS || modus == MANUELL) && (isttemp >= sollwert) && (nachgussruf == false)) { // Nachguss -> Sollwert erreicht
    nachgussruf = true;
    rufmodus = modus;
    modus = BRAUMEISTERRUFALARM;
    y = 0;
    braumeister[y] = BM_ALARM_SIGNAL;        // nur Ruf und weiter mit Regelung
  }
}

void funktion_rastanzahl()
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

  printNumI_lcd(rasten, 19, 1);

  warte_und_weiter(EINGABE_MAISCHTEMP);
}

void funktion_maischtemperatur()
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

void funktion_rasteingabe()
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

void funktion_zeiteingabe()
{
  if (anfang) {
    drehen = rastZeit[x];
    anfang = false;
  }

  drehen = constrain( drehen, 1, 99);
  rastZeit[x] = drehen;

  print_lcd_minutes(rastZeit[x], RIGHT, 2);

  warte_und_weiter(EINGABE_BRAUMEISTERRUF);
}

void funktion_braumeister()
{
  if (anfang) {
    drehen = (int)braumeister[x];
    anfang = false;
  }

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
      modus = EINGABE_RAST_TEMP; // Sprung zur Rasttemperatureingabe
    } else {
      x = 1;
      modus = EINGABE_ENDTEMP; // Sprung zur Rastzeiteingabe
    }
  }
}

void funktion_endtempeingabe()
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
    {
      altsekunden = millis();
    }
  } else {
    print_lcd("Start ?", CENTER, 2);
  }

  if (warte_und_weiter(naechsterModus)) {
    saveConfig();
  }
}

void funktion_maischtemperaturautomatik()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Auto", LEFT, 0);
    print_lcd("Maischen", RIGHT, 0);
    drehen = maischtemp; // Vorgabewert
    anfang = false;
  }

  drehen = constrain( drehen, 10, 105);
  maischtemp = drehen;
  sollwert = maischtemp;

  if (isttemp >= sollwert) {
    rufmodus = AUTO_RAST_TEMP;
    y = 0;
    braumeister[y] = BM_ALARM_WAIT;
    modus = BRAUMEISTERRUFALARM;
  }
}

void funktion_tempautomatik()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Auto", LEFT, 0);
    printNumI_lcd(x, 13, 0);
    print_lcd(". Rast", RIGHT, 0);

    drehen = rastTemp[x];
    anfang = false;
    heizung = true;
    wartezeit = millis() - 60000;  // sofort aufheizen
  }

  drehen = constrain( drehen, 10, 105);

  rastTemp[x] = drehen;
  sollwert = rastTemp[x];

  if (isttemp >= sollwert) {
    modus = AUTO_RAST_ZEIT;
    anfang = true;
  }
}

void funktion_zeitautomatik()
{
  if (anfang) {
    drehen = rastZeit[x];
    wartezeit = millis() - 60000;  // sofort aufheizen
    heizung = true;
  }

  print_lcd_minutes(rastZeit[x], RIGHT, 2);

  // Zeitzählung---------------
  if (anfang) {
    print_lcd("Set Time", LEFT, 3);
    setTime(00, 00, 00, 00, 01, 01); // Sekunden auf 0 stellen
    delay(400); //test

    sekunden = second();
    minutenwert = minute();
    stunden = hour();

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
  rastZeit[x] = drehen; // Encoderzuordnung

  if (minuten >= rastZeit[x]) {
    anfang = true;
    y = x;
    if (x < rasten) {
      modus = AUTO_RAST_TEMP;
      x++;
    } else {
      x = 1; // Endtemperatur
      modus = AUTO_ENDTEMP;
    }

    if (braumeister[y] != BM_ALARM_AUS) {
      rufmodus = modus;
      modus = BRAUMEISTERRUFALARM;
    }
  }
}

void funktion_endtempautomatik()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Auto", LEFT, 0);
    print_lcd("Endtemp", RIGHT, 0);
    drehen = endtemp;    // Zuordnung Encoder
    wartezeit = millis() - 60000;  // sofort aufheizen
    heizung = true;
    anfang = false;
  }

  drehen = constrain( drehen, 10, 105);
  endtemp = drehen;
  sollwert = endtemp;

  if (isttemp >= sollwert) {
    rufmodus = ABBRUCH;
    modus = BRAUMEISTERRUFALARM;
    regelung = REGL_AUS;
    heizung = false;
    y = 0;
    braumeister[y] = BM_ALARM_WAIT;
  }
}

void funktion_braumeisterrufalarm()
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
  }

  // 20 Sekunden Rufsignalisierung wenn "Ruf Signal"
  if (braumeister[y] == BM_ALARM_SIGNAL && millis() >= (rufsignalzeit + 20000)) {
    anfang = true;
    pause = 0;
    beeperOn(false);
    modus = rufmodus;
    einmaldruck = false;
  }

  if (warte_und_weiter(BRAUMEISTERRUF)) {
    pause = 0;
    beeperOn(false);
    if (braumeister[y] == BM_ALARM_SIGNAL) {
      print_lcd("   ", LEFT, 3);
      modus = rufmodus;
    }
  }
}

void funktion_braumeisterruf()
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
    print_lcd("        ", LEFT, 3);     // Text "weiter ?" löschen
    print_lcd("             ", RIGHT, 3); // Löscht Text bei Sensorfehler oder Alarmtest
    sensorfehler = false;
    delay(500);     // kurze Wartezeit, damit nicht durch unbeabsichtigtes Drehen der nächste Vorgabewert verstellt wird
  }
}

void funktion_hysterese()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Hysterese", LEFT, 0);
    print_lcd("Eingabe", RIGHT, 0);

    drehen = hysteresespeicher;
    anfang = false;
  }

  drehen = constrain( drehen, 0, 40); //max. 4,0 Sekunden Hysterese
  hysteresespeicher = drehen;

  printNumF_lcd(float(hysteresespeicher) / 10, RIGHT, 1);

  if (warte_und_weiter(SETUP_MENU)) {
    saveConfig();
  }
}

void funktion_kochschwelle()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Kochschwelle", LEFT, 0);
    print_lcd("Eingabe", RIGHT, 0);
    drehen = kschwelle;
    anfang = false;
  }

  drehen = constrain( drehen, 20, 99);
  kschwelle = drehen;
  
  printNumI_lcd(kschwelle, RIGHT, 1);

  if (warte_und_weiter(SETUP_MENU)) {
    saveConfig();
  }
}

void funktion_kochzeit()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Kochen", LEFT, 0);
    print_lcd("Eingabe", RIGHT, 0);
    print_lcd("Zeit", LEFT, 1);

    drehen = kochzeit;
    anfang = false;
  }

  drehen = constrain( drehen, 20, 180);
  kochzeit = drehen;

  print_lcd_minutes( kochzeit, RIGHT, 1);

  warte_und_weiter(EINGABE_HOPFENGABEN_ANZAHL);
}

void funktion_anzahlhopfengaben()
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

void funktion_hopfengaben()
{
  if (anfang) {
    x = 1;
    drehen = hopfenZeit[x];
    anfang = false;
    lcd.clear();
    print_lcd("Kochen", LEFT, 0);
    print_lcd("Eingabe", RIGHT, 0);
  }

  printNumI_lcd(x, LEFT, 1);
  print_lcd(". Hopfengabe", 1, 1);
  print_lcd("nach", LEFT, 2);

  drehen = constrain(drehen, hopfenZeit[(x - 1)] + 5, kochzeit - 5);
  hopfenZeit[x] = drehen;

  print_lcd_minutes(hopfenZeit[x], RIGHT, 2);

  if (warte_und_weiter(modus)) {
    if (x < hopfenanzahl) {
      x++;
      drehen = hopfenZeit[x];
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

void funktion_kochenaufheizen()
{
  if (anfang) {
    lcd.clear();
    print_lcd("Kochen", LEFT, 0);
    anfang = false;
  }

  sollwert = kschwelle;
  if (isttemp >= sollwert) {
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

void funktion_hopfenzeitautomatik()
{
  if (anfang) {
    x = 1;
    lcd.clear();
    print_lcd("Kochen", LEFT, 0);
    setTime(00, 00, 00, 00, 01, 01); //.........Sekunden auf 0 stellen
    print_lcd_minutes(kochzeit, RIGHT, 0);

    sekunden = second();
    minutenwert = minute();
    stunden = hour();

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
    }

    if (warte_und_weiter(modus)) {
      anfang = false; // nicht zurücksetzen!!!
      _next_koch_step();
    }
  }

  if ((minuten > hopfenZeit[x]) && (x <= hopfenanzahl)) {  // Alarmende nach 1 Minute
    _next_koch_step();
  }

  if (minuten >= kochzeit) {   //Kochzeitende
    rufmodus = ABBRUCH;
    modus = BRAUMEISTERRUFALARM;
    regelung = REGL_AUS;
    heizung = false;
    y = 0;
    braumeister[y] = BM_ALARM_WAIT;
  }
}

void _next_koch_step() {
  print_lcd("   ", LEFT, 3);
  pause = 0;
  beeperOn(false);
  zeigeH = true;
  x++;
}

void funktion_timer()
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

void funktion_timerlauf()
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

    sekunden = second();
    minutenwert = minute();
    stunden = hour();

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
    rufmodus = ABBRUCH;
    modus = BRAUMEISTERRUFALARM;
    regelung = REGL_AUS;
    heizung = false;
    y = 0;
    braumeister[y] = BM_ALARM_WAIT;
  }
}

void funktion_abbruch()
{
  regelung = REGL_AUS;
  heizung = false;
  wartezeit = -60000;
  heizungOn(false);
  beeperOn(false);
  anfang = true;
  lcd.clear();
  rufmodus = HAUPTSCHIRM;
  x = 1;
  y = 1;
  n = 0;
  einmaldruck = false;
  nachgussruf = false;
  pause = 0;
  drehen = sollwert;
  modus = HAUPTSCHIRM;
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

#ifdef ESP8266
void watchdogSetup(void) {
  wdt_enable(WDTO_2S);
}

extern "C" void custom_crash_callback(struct rst_info * rst_info, uint32_t stack, uint32_t stack_end )
{
  heizungOn(false);
  beeperOn(true); // beeeeeeeeeeep
  while (true);
}
#else
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
  beeperOn(true); // beeeeeeeeeeep
  while (true);
}
#endif

void beeperOn(boolean value)
{
  if (value) {
    digitalWrite(beeperPin, HIGH); // einschalten
  } else {
    digitalWrite(beeperPin, LOW); // ausschalten
  }
}

void heizungOn(boolean value)
{
  if (value) {
    digitalWrite(heizungPin, LOW);   // einschalten
  } else {
    digitalWrite(heizungPin, HIGH);   // ausschalten
  }
}

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

void printNumF_lcd (double num, int x, int y, byte dec, int length)
{
  char st[27];

  dtostrf(num, length, dec, st );
  print_lcd(st, x, y);
}

void setupWebserver() {
  HTTP.on("/", handleRoot);
  HTTP.on("/data.json", HTTP_GET, [&]() {
    HTTP.sendHeader("Connection", "close");
    HTTP.sendHeader("Access-Control-Allow-Origin", "*");
    return handleDataJson();
  });

  HTTP.onNotFound(handleNotFound);
  HTTP.begin();
  MDNS.addService("http", "tcp", 80);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += HTTP.uri();
  message += "\nMethod: ";
  message += (HTTP.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += HTTP.args();
  message += "\n";
  for (uint8_t i = 0; i < HTTP.args(); i++) {
    message += " " + HTTP.argName(i) + ": " + HTTP.arg(i) + "\n";
  }
  HTTP.send(404, "text/plain", message);
}

void handleDataJson() {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& json = jsonBuffer.createObject();

  String title;
  String data;
  String title2 = "Details";

  char jetzt[10];
  sprintf(jetzt, "%02i:%02i", (stunden * 60 ) + minuten, sekunden);

  switch (modus) {
    case HAUPTSCHIRM:
      title = F("Hauptmenu");
      break;

    case MAISCHEN:
      title = F("Maischmenu");
      break;

    case MANUELL:
      title = F("Maischen: manuell");
      break;

    case BRAUMEISTERRUFALARM:
    case BRAUMEISTERRUF:
      title = F("Rufalarm");
      break;

    case EINGABE_RAST_ANZ:
      title = F("Maisch-Automatik: Eingabe");
      title2 = F("Rasteneingabe");
      data = F("<li>Anzahl: ");
      data += rasten;
      data += F("</li>");
      break;

    case EINGABE_MAISCHTEMP:
      title = F("Maisch-Automatik: Eingabe");
      title2 = F("Maischetemperatur");
      data = F("<li>Einmaischen bei ");
      data += maischtemp;
      data += F("&deg;C</li>");
      break;

    case EINGABE_RAST_TEMP:
      title = F("Maisch-Automatik: Eingabe");
      title2 = F("Rast ");
      title2 += x;
      title2 += F(" von ");
      title2 += rasten;

      data = F("<li>Rasttemperatur: ");
      data += rastTemp[x];
      data += F("&deg;C</li>");
      break;

    case EINGABE_RAST_ZEIT:
      title = F("Maisch-Automatik: Eingabe");
      title2 = F("Rast ");
      title2 += x;
      title2 += F(" von ");
      title2 += rasten;

      data = F("<li>Rasttemperatur: ");
      data += rastTemp[x];
      data += F("&deg;C</li>");

      data += F("<li>Rastzeit: ");
      data += rastZeit[x];
      data += F(" min.</li>");
      break;

    case EINGABE_BRAUMEISTERRUF:
      title = F("Maisch-Automatik");
      title2 = F("Rast ");
      title2 += x;
      title2 += F(" von ");
      title2 += rasten;

      data = F("<li>Rasttemperatur: ");
      data += rastTemp[x];
      data += F("&deg;C</li>");

      data += F("<li>Rastzeit: ");
      data += rastZeit[x];
      data += F(" min.</li>");

      data += "<li>Ruf:  ";
      switch (braumeister[x]) {
        case BM_ALARM_AUS:
          data += F("nein");
          break;

        case BM_ALARM_WAIT:
          data += F("anhalten");
          break;

        case BM_ALARM_SIGNAL:
          data += F("Signal");
          break;

        default:
          data += braumeister[x];
      }
      data += F("</li>");
      break;

    case EINGABE_ENDTEMP:
      title = F("Maisch-Automatik: Eingabe");
      title2 = F("Endtemperatur: ");
      data = F("<li>Abmaischen bei ");
      data += endtemp;
      data += F("&deg;C</li>");
      break;

    case AUTO_START:
      title = F("Maisch-Automatik: Start?");
      break;

    case AUTO_MAISCHTEMP:
      title = F("Maisch-Automatik");
      title2 = F("Aufheizen bis zum Einmaischen");
      data = F("<li>Einmaischen bei ");
      data += maischtemp;
      data += F("&deg;C</li>");
      break;

    case AUTO_RAST_TEMP:
      title = F("Maisch-Automatik");  // x (rasten), rastTemp[x]
      title2 = F("Rast ");
      title2 += x;
      title2 += F(" von ");
      title2 += rasten;
      data += F("<li>Aufheizen auf ");
      data += rastTemp[x];
      data += F("&deg;C<li>");
      break;

    case AUTO_RAST_ZEIT:
      title = F("Maisch-Automatik"); // x (rasten), rastZeit[x], minuten, sekunden
      title2 = F("Rast ");
      title2 += x;
      title2 += F(" von ");
      title2 += rasten;
      data += F("<li>");
      data += jetzt;
      data += F(" von ");
      data += rastZeit[x];
      data += F(" min.</li>");
      break;

    case AUTO_ENDTEMP:
      title = F("Maisch-Automatik");
      title2 = F("Aufheizen bis zum Abmaischen");
      data = F("<li>Abmaischen bei ");
      data += endtemp;
      data += F("&deg;C</li>");
      break;

    case KOCHEN:
      title = F("Kochen: Eingabe Kochzeit ");
      title += kochzeit;
      title += " min.";
      break;

    case EINGABE_HOPFENGABEN_ANZAHL:
      title = F("Kochen: Eingabe Anzahl Hopfengaben ");
      title += hopfenanzahl;
      break;

    case EINGABE_HOPFENGABEN_ZEIT:
      title = F("Kochen: Eingabe Hopfenzeit");
      break;

    case KOCHEN_START_FRAGE:
      title = F("Kochen: Warten auf Start");
      break;

    case KOCHEN_AUFHEIZEN:
      title = F("Kochen: Aufheizen");
      break;

    case KOCHEN_AUTO_LAUF:// x (hopfenanzahl), hopfenZeit[x], minuten, sekunden, kochzeit
      title = F("Kochen");

      title2 = "Kochzeit gesamt: ";
      title2 += kochzeit;
      title2 += " min";

      data = "<li>";
      data += x;
      data += ". Hopfengabe bei ";
      data += hopfenZeit[x];
      data += " min</li>";

      data += "<li>Aktuell: ";
      data += jetzt;
      data += " min</li>";
      break;

    case TIMER:
      title = F("Timer ");
      title += timer;
      title += " min.";
      break;

    case TIMERLAUF:
      title = F("Timer ");

      data += "<li>Soll: ";
      data += timer;
      data += " min</li>";

      data += "<li>Ist: ";
      data += jetzt;
      data += " min</li>";
      break;

    default:
      title = F("Modus: ");
      title += modus;
  }

  json["title"] = title;
  json["title2"] = title2;
  json["temp_ist"] = isttemp;
  json["temp_soll"] = sollwert;
  json["heizung"] = heizung ? "an" : "aus";

  json["data"] = data;

  json["modus"] = (int)modus;
  json["rufmodus"] = (int)rufmodus;

  json["maischtemp"] = maischtemp;
  json["rast_anzahl"] = rasten;
  json["rast_nr"] = x;
  json["rast_temp"] = rastTemp[x];
  json["rast_zeit_soll"] = rastZeit[x];
  json["rast_zeit_ist"] = jetzt;

  json["timer_soll"] = timer;
  json["timer_ist"] = jetzt;

  json["kochzeit"] = kochzeit;
  json["hopfenanzahl"] = hopfenanzahl;

  JsonArray& all_rast_temp = json.createNestedArray("all_rast_temp");
  all_rast_temp.copyFrom(rastTemp);
  all_rast_temp.removeAt(0);

  JsonArray& all_rast_zeit = json.createNestedArray("all_rast_zeit");
  all_rast_zeit.copyFrom(rastZeit);
  all_rast_zeit.removeAt(0);

  JsonArray& all_hopfen_zeit = json.createNestedArray("all_hopfen_zeit");
  all_hopfen_zeit.copyFrom(hopfenZeit);
  all_hopfen_zeit.removeAt(0);

  String message = "";
  json.printTo(message);
  HTTP.send(200, "application/json;charset=utf-8", message);
}

void handleRoot() {
  HTTP.send_P(200, "text/html", PAGE_Kochen);
}

bool setupWIFI() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(my_ssid.c_str(), my_psk.c_str());
  MDNS.begin("bk");

  SerialOut(F("\nEnabling WIFI"), false);
  unsigned long startTime = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - startTime < 10000) {
    delay(500);
    SerialOut(F("."), false);
  }
  SerialOut(F(""));

  if (WiFi.status() == WL_CONNECTED) {
    SerialOut(F("WiFi connected"));
    SerialOut(F("IP address: "));
    SerialOut(WiFi.localIP());

    SerialOut(F("signal strength (RSSI):"), false);
    SerialOut(WiFi.RSSI(), false);
    SerialOut(F(" dBm"));
    return true;
  } else {
    SerialOut(F("Can not connect to WiFi station. Go into AP mode."));
    WiFi.mode(WIFI_AP);

    delay(10);

    WiFi.softAP("bk", "bk");

    SerialOut(F("IP address: "), false);
    SerialOut(WiFi.softAPIP());
  }
  return false;
}

bool saveConfig()
{
  SerialOut(F("saving config..."));

  // if SPIFFS is not usable
  if (!SPIFFS.begin() || !SPIFFS.exists(CFGFILE) ||
      !SPIFFS.open(CFGFILE, "w"))
  {
    SerialOut(F("need to format SPIFFS: "));
    SPIFFS.end();
    SPIFFS.begin();
    SerialOut(SPIFFS.format());
  }

  DynamicJsonBuffer jsonBuffer;
  JsonObject &json = jsonBuffer.createObject();

  json["hysteresespeicher"] = hysteresespeicher;
  json["kschwelle"] = kschwelle;
  json["rasten"] = rasten;

  json["rastTemp_1"] = rastTemp[1];
  json["rastZeit_1"] = rastZeit[1];
  json["rastAlarm_1"] = (int)braumeister[1];
  json["rastTemp_2"] = rastTemp[2];
  json["rastZeit_2"] = rastZeit[2];
  json["rastAlarm_2"] = (int)braumeister[2];
  json["rastTemp_3"] = rastTemp[3];
  json["rastZeit_3"] = rastZeit[3];
  json["rastAlarm_3"] = (int)braumeister[3];
  json["rastTemp_4"] = rastTemp[4];
  json["rastZeit_4"] = rastZeit[4];
  json["rastAlarm_4"] = (int)braumeister[4];
  json["rastTemp_5"] = rastTemp[5];
  json["rastZeit_5"] = rastZeit[5];
  json["rastAlarm_5"] = (int)braumeister[5];

  json["maischtemp"] = maischtemp;
  json["endtemp"] = endtemp;

  json["kochzeit"] = kochzeit;
  json["hopfenanzahl"] = hopfenanzahl;

  json["hopfenZeit_1"] = hopfenZeit[1];
  json["hopfenZeit_2"] = hopfenZeit[2];
  json["hopfenZeit_3"] = hopfenZeit[3];
  json["hopfenZeit_4"] = hopfenZeit[4];
  json["hopfenZeit_5"] = hopfenZeit[5];

  // Store current Wifi credentials
  json["SSID"] = WiFi.SSID();
  json["PSK"] = WiFi.psk();


  File configFile = SPIFFS.open(CFGFILE, "w+");
  if (!configFile)
  {
    SerialOut(F("failed to open config file for writing"));
    SPIFFS.end();
    return false;
  }
  else
  {
    if (isDebugEnabled) {
      json.printTo(Serial);
    }
    json.printTo(configFile);
    configFile.close();
    SPIFFS.end();
    SerialOut(F("saved successfully"));
    return true;
  }
}

bool readConfig()
{
  SerialOut(F("mounting FS..."), false);

  if (SPIFFS.begin())
  {
    SerialOut(F(" mounted!"));
    if (SPIFFS.exists(CFGFILE))
    {
      // file exists, reading and loading
      SerialOut(F("reading config file"));
      File configFile = SPIFFS.open(CFGFILE, "r");
      if (configFile)
      {
        SerialOut(F("opened config file"));
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(buf.get());

        if (json.success())
        {
          SerialOut(F("parsed json"));
          int t = 0;
          if (json.containsKey("hysteresespeicher"))
            hysteresespeicher = json["hysteresespeicher"];
          if (json.containsKey("kschwelle"))
            kschwelle = json["kschwelle"];

          if (json.containsKey("rasten"))
            rasten = json["rasten"];

          if (json.containsKey("rastTemp_1"))
            rastTemp[1] = json["rastTemp_1"];
          if (json.containsKey("rastZeit_1"))
            rastZeit[1] = json["rastZeit_1"];
          if (json.containsKey("rastAlarm_1")) {
            t = json["rastAlarm_1"];
            braumeister[1] = (BM_ALARM_MODE)t;
          }
          if (json.containsKey("rastTemp_2"))
            rastTemp[2] = json["rastTemp_2"];
          if (json.containsKey("rastZeit_2"))
            rastZeit[2] = json["rastZeit_2"];
          if (json.containsKey("rastAlarm_2")) {
            t = json["rastAlarm_2"];
            braumeister[2] = (BM_ALARM_MODE)t;
          }

          if (json.containsKey("rastTemp_3"))
            rastTemp[3] = json["rastTemp_3"];
          if (json.containsKey("rastZeit_3"))
            rastZeit[3] = json["rastZeit_3"];
          if (json.containsKey("rastAlarm_3")) {
            t = json["rastAlarm_3"];
            braumeister[3] = (BM_ALARM_MODE)t;
          }

          if (json.containsKey("rastTemp_4"))
            rastTemp[4] = json["rastTemp_4"];
          if (json.containsKey("rastZeit_4"))
            rastZeit[4] = json["rastZeit_4"];
          if (json.containsKey("rastAlarm_4")) {
            t = json["rastAlarm_4"];
            braumeister[4] = (BM_ALARM_MODE)t;
          }
          if (json.containsKey("rastTemp_5"))
            rastTemp[5] = json["rastTemp_5"];
          if (json.containsKey("rastZeit_5"))
            rastZeit[5] = json["rastZeit_5"];
          if (json.containsKey("rastAlarm_5")) {
            t = json["rastAlarm_5"];
            braumeister[5] = (BM_ALARM_MODE)t;
          }

          if (json.containsKey("maischtemp"))
            maischtemp = json["maischtemp"];

          if (json.containsKey("endtemp"))
            endtemp = json["endtemp"];

          if (json.containsKey("kochzeit"))
            kochzeit = json["kochzeit"];

          if (json.containsKey("hopfenanzahl"))
            hopfenanzahl = json["hopfenanzahl"];

          if (json.containsKey("hopfenZeit_1"))
            hopfenZeit[1] = json["hopfenZeit_1"];
          if (json.containsKey("hopfenZeit_2"))
            hopfenZeit[2] = json["hopfenZeit_2"];
          if (json.containsKey("hopfenZeit_3"))
            hopfenZeit[3] = json["hopfenZeit_3"];
          if (json.containsKey("hopfenZeit_4"))
            hopfenZeit[4] = json["hopfenZeit_4"];
          if (json.containsKey("hopfenZeit_5"))
            hopfenZeit[5] = json["hopfenZeit_5"];

          if (json.containsKey("SSID"))
            my_ssid = (const char *)json["SSID"];
          if (json.containsKey("PSK"))
            my_psk = (const char *)json["PSK"];

          SerialOut(F("parsed config:"), true);
          if (isDebugEnabled) {
            json.printTo(Serial);
          }
          return true;
        }
        else
        {
          SerialOut(F("ERROR: failed to load json config"));
          return false;
        }
      }
      SerialOut(F("ERROR: unable to open config file"));
    }
  }
  else
  {
    SerialOut(F(" ERROR: failed to mount FS!"));
    return false;
  }
}
