#ifndef _CONFIG_H
#define _CONFIG_H

#pragma once

#define DEBUG

#define WIFI_SSID "ssid"
#define WIFI_PSK "***password***"

#define FIRMWAREVERSION "3.0.0"

#define LCD_I2C_ADR 0x27 //# 0x27=proto / 0x3f=box
 
enum PinAssignments {
  encoderPinA = D5,
  encoderPinB = D6,
  tasterPin = D7,
  oneWirePin = D3,
  heizungPin = D4,
  beeperPin = D8,
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

#define CFGFILE "/config.json"
#define Hysterese 0             //int
#define KOCHSCHWELLE 98             //int 25?

#define ENCODER_STEPS_PER_NOTCH    4   // Change this depending on which encoder is used

#define RESOLUTION 12 // 12bit resolution == 750ms update rate

#define LEFT 0
#define RIGHT 9999
#define CENTER 9998

#ifndef DEBUG
#define DEBUG 1 // uncomment this line to enable serial diagnostic messages
#endif

#endif // _CONFIG_H
