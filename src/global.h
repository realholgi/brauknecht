#pragma once

#define RESOLUTION 12 // 12bit resolution == 750ms update rate

enum MODUS {HAUPTSCHIRM = 0,
            MANUELL, NACHGUSS, MAISCHEN,
            SETUP_MENU, SETUP_HYSTERESE, SETUP_KOCHSCHWELLE,
            EINGABE_RAST_ANZ, AUTOMATIK = EINGABE_RAST_ANZ, EINGABE_MAISCHTEMP, EINGABE_RAST_TEMP, EINGABE_RAST_ZEIT, EINGABE_BRAUMEISTERRUF, EINGABE_ENDTEMP,
            AUTO_START, AUTO_MAISCHTEMP, AUTO_RAST_TEMP, AUTO_RAST_ZEIT, AUTO_ENDTEMP,
            BRAUMEISTERRUFALARM, BRAUMEISTERRUF,
            KOCHEN, EINGABE_HOPFENGABEN_ANZAHL, EINGABE_HOPFENGABEN_ZEIT, KOCHEN_START_FRAGE, KOCHEN_AUFHEIZEN, KOCHEN_AUTO_LAUF,
            TIMER, TIMERLAUF,
            ABBRUCH, ALARMTEST,
            NIX
           };

enum REGEL_MODE {REGL_AUS = 0, REGL_MAISCHEN, REGL_KOCHEN};

enum BM_ALARM_MODE {BM_ALARM_AUS = 0, BM_ALARM_MIN = BM_ALARM_AUS, BM_ALARM_WAIT, BM_ALARM_SIGNAL, BM_ALARM_MAX = BM_ALARM_SIGNAL};

struct MENU {
  char* text;
  MODUS modus;
};

#define LEFT 0
#define RIGHT 9999
#define CENTER 9998

bool isDebugEnabled() {
#ifdef DEBUG
    return true;
#endif // DEBUG
    return false;
}

// generic serial output
template<typename T>
void SerialOut(const T aValue, bool newLine = true) {
    if (!isDebugEnabled())
        return;
    Serial.print(aValue);
    if (newLine)
        Serial.print("\n");
}

void SerialOut() {
    SerialOut("");
}