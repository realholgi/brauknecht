

//start=============================================================

//Automatische Steuerung zum Bierbrauen
//von fg100

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


//LCD Initialisierung ----------------------------------------------
/*
 * LCD PIN – Uno Pin
 * 1    -    D11
 * 2    -    D12
 * 3    -    D10
 * 4    -    D9
 * 5    -    D8
 * 6    -    5V mit 220 Ohm geregelt auf ca. 3,3V
 * 7    -    GRD
 * 8    -    GRD
 
 *   Achtung: 
 *   In der Original LCD5110_Basic Bibliothek ist ein Fehler.
 *   Es muss in den Quelldateien LCD5110_Basic.cpp
 *   und LCD5110_Basic.h der Bibliothek die Zeile
 *   #include "WProgram.h"
 *   durch
 *   #include "Arduino.h"
 *   ersetzt werden.
 
 */

#include <LCD5110_Basic.h>
 
LCD5110 myGLCD(8,9,10,11,12);
extern uint8_t SmallFont[];
extern uint8_t MediumNumbers[];
extern uint8_t BigNumbers[];
//------------------------------------------------------------------


//Encoder Initialisierung ----------------------------------------------
volatile int number = 0;
volatile boolean halfleft = false;      // Used in both interrupt routines
volatile boolean halfright = false;     // Used in both interrupt routines

int oldnumber=0;
int ButtonVoltage = 0;
int ButtonPressed = 0;
#define BUTTON_SELECT    5
#define BUTTON_NONE      0

int drehen;        //drehgeber Werte
int fuenfmindrehen;    //drehgeber Werte 5 Minutensprünge

//------------------------------------------------------------------



//Temperatursensor DS1820 Initialisierung ---------------------------
#include <OneWire.h>
#include <DallasTemperature.h>

// Data wire is plugged into port 5 on the Arduino
#define ONE_WIRE_BUS 5

// Setup a oneWire instance to communicate with any OneWire devices (not just Maxim/Dallas temperature ICs)
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature. 
DallasTemperature sensors(&oneWire);

// arrays to hold device address
DeviceAddress insideThermometer;


//Zeitmessung----------------------------------------------------------
#include <Time.h>
//------------------------------------------------------------------



//Ausgänge an DigitalPIN --------------------------------------------
int schalterH1 = 6;                                   //Zuordnung Heizung Relais1
int schalterH2 = 7;                                   //Zuordnung Heizung Relais1
int schalterB = 19;                                   //Zuordnung Braumeisterruf
int schalterF = 18;                                   //Zuordnung Braumeisterruf
//-----------------------------------------------------------------


//Taster an Digital PIN--------------------------------------------
int taster = 4;     // Taster am Pin 4 für Bestätigung und Abbruch
int val = 0;           // Variable um Messergebnis zu speichern
//-----------------------------------------------------------------


//Allgemein Initialisierung------------------------------------------
int anfang=0;
float altsekunden;
int regelung=0;
int heizung=0;
int sensorfehler=0;
float hysterese=0;
float wartezeit=-60000;
float sensorwert;
float isttemp=20;                                     //Vorgabe 20 damit Sensorfehler am Anfang nicht anspricht
int isttemp_ganzzahl;                                 //für Übergabe der isttemp als Ganzzahl
int modus=0;            
int rufmodus=0;
float rufsignalzeit=0;
boolean nachgussruf=false;                            //Signal wenn Nachgusstemp erreicht
int x=1;                                              //aktuelle Rast Nummer
int y=1;                                              //Übergabewert von x für Braumeisterruf
int n=0;                                              //Counter Messungserhöhung zur Fehlervermeidung
int pause=0;                                          //Counter für Ruftonanzahl
unsigned long abbruchtaste;
boolean einmaldruck=false;                            //Überprüfung loslassen der Taste Null 

int sekunden=0;                                         //Zeitzählung
int minuten=0;                                          //Zeitzählung
int minutenwert=0;                                      //Zeitzählung
int stunden=0;                                          //Zeitzählung


//Vorgabewerte zur ersten Einstellung------------------------------------------- 
int sollwert=20;                                    //Sollwertvorgabe für Anzeige
int maischtemp=45;                                    //Vorgabe Einmasichtemperatur
int rasten=3;                                         //Vorgabe Anzahl Rasten
int rastTemp[]={0,55,64,72,72,72};        //Rasttemperatur Werte
int rastZeit[]={0,15,35,25,20,20};              //Rastzeit Werte
int braumeister[]={0,0,0,0,0,0};               //Braumeisterruf standart AUS
int endtemp=78;                                     //Vorgabewert Endtemperatur

int kochzeit=90;
int hopfenanzahl=2;
int hopfenZeit[]={0,10,40,40,40,40,40};
int timer=10;

//setup=============================================================
void setup()
{

  Serial.begin(9600);           //.........Test Serial
  Serial.println("Test");         //.........Test Serial
  
  drehen=sollwert;
  //drehen=sollwert;

  pinMode(schalterH1, OUTPUT);   // initialize the digital pin as an output.   
  pinMode(schalterH2, OUTPUT);   // initialize the digital pin as an output.     
  pinMode(schalterB, OUTPUT);   // initialize the digital pin as an output. 
  pinMode(schalterF, OUTPUT);   // initialize the digital pin as an output. 

  digitalWrite(schalterH1, HIGH);   // ausschalten
  digitalWrite(schalterH2, HIGH);   // ausschalten


  pinMode(taster, INPUT);                    // Pin für Taster
  digitalWrite(taster, HIGH);                // Turn on internal pullup resistor
  
  pinMode(2, INPUT);                    // Pin für Drehgeber
  digitalWrite(2, HIGH);                // Turn on internal pullup resistor
  pinMode(3, INPUT);                    // Pin für Drehgeber
  digitalWrite(3, HIGH);                // Turn on internal pullup resistor
  

  attachInterrupt(0, isr_2, FALLING);   // Call isr_2 when digital pin 2 goes LOW
  attachInterrupt(1, isr_3, FALLING);   // Call isr_3 when digital pin 3 goes LOW


  //LCD Setup ------------------------------------------------------
  myGLCD.InitLCD();
  myGLCD.clrScr();
  
  myGLCD.setFont(SmallFont);
  
  myGLCD.print("BK V2.2 - 5110", LEFT, 0);
  myGLCD.print("Arduino", LEFT, 12);
  myGLCD.print(":)", RIGHT, 24);
  myGLCD.print(":)", RIGHT, 32);
  myGLCD.print("fg100", RIGHT, 40);
  
  delay (2000);
  
  for (x=1; x<3; x++) 
  { 
  digitalWrite(schalterB, HIGH);   // Alarmtest
  digitalWrite(schalterF, HIGH);   // Funkalarmtest
  delay(200);
  digitalWrite(schalterB, LOW);   // Alarm ausschalten
  digitalWrite(schalterF, LOW);   // Funkalarm ausschalten
  delay(200);
  }
  x=1;
  
  myGLCD.clrScr();
  // --------------------------------------------------------------- 


  //Temperatursensor DS1820 ----------------------------------------
  sensors.getAddress(insideThermometer, 0);
  // set the resolution to 9 bit (Each Dallas/Maxim device is capable of several different resolutions)
  sensors.setResolution(insideThermometer, 9);
  //---------------------------------------------------------------
}


//loop=============================================================
void loop()
{

  
  // Zeitermittlung ------------------------------------------------------  
  sekunden=second();    //aktuell Sekunde abspeichern für die Zeitrechnung
  minutenwert=minute(); //aktuell Minute abspeichern für die Zeitrechnung
  stunden=hour();       //aktuell Stunde abspeichern für die Zeitrechnung
   //---------------------------------------------------------------
  

  // Temperatursensor DS1820 Temperaturabfrage ---------------------
  // call sensors.requestTemperatures() to issue a global temperature 
  sensors.requestTemperatures(); // Send the command to get temperatures
  sensorwert = sensors.getTempC(insideThermometer);
  if ((sensorwert != isttemp) && (n < 10)) // Messfehlervermeidung
    {                                      // des Sensorwertes
    n++;                                   // nach mehreren
    }                                      // Messungen
  else
    {
    isttemp=sensorwert;
    n=0;
     }
  //---------------------------------------------------------------
 
 
  // Sensorfehler ------------------------------------------------- 
  // Sensorfehler -127 => VCC fehlt
  // Sensorfehler 85.00 => interner Sensorfehler ggf. Leitung zu lang
  //                       nicht aktiviert
  // Sensorfehler 0.00 => Datenleitung oder GND fehlt
 
  
  if (regelung == 1 || regelung == 2) //nur bei Masichen bzw. Kühlen
  {
    if ((int)isttemp == -127 || (int)isttemp == 0 )
                  //zur besseren Erkennung Umwandling in (int)-Wert
                  //sonst Probleme mit der Erkennung gerade bei 0.00
     if (sensorfehler == 0)
     {
       rufmodus=modus;
       myGLCD.print("Sensorfehler", RIGHT, 32);
       regelung=0;
       heizung=0;
       sensorfehler=1;
       modus=31;
     }
  }
  //-------------------------------------------------------------------


 
 
  //Encoder drehen ------------------------------------------------
  if(number != oldnumber)
  {
    {
      if (number > oldnumber)   // < > Zeichen ändern = Encoderdrehrichtung ändern
      {
        ++drehen;
        //halbdrehen=halbdrehen+.5;
        fuenfmindrehen=fuenfmindrehen+5;
      }
      else
      {
        --drehen;
        //halbdrehen=halbdrehen-.5;
        fuenfmindrehen=fuenfmindrehen-5;
      } 
      oldnumber = number;
    }
  }
  //---------------------------------------------------------------
 
 
  // Temperaturanzeige Istwert --------------------------------------- 
  myGLCD.print("ist", 20, 40);
  myGLCD.printNumF(float(sensorwert), 1,46, 40);
  myGLCD.setFont(SmallFont);
  myGLCD.print("~C", RIGHT, 40);
  //-------------------------------------------------------------------


 //Heizregelung----------------------------------------------------
 if (regelung == 1)
 {
  // Temperaturanzeige Sollwert --------------------------------------- 
  myGLCD.printNumF(int(sollwert), 1,46, 8);
  myGLCD.print("~C", RIGHT, 8);
  //-------------------------------------------------------------------


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
    if ((isttemp <= (sollwert-4)) && (heizung == 1))
      {
        hysterese=0.5;
      }
      
    //Ausschalten wenn Sollwert-Hysterese erreicht und dann Wartezeit
    if ((heizung == 1) && (isttemp >= (sollwert-hysterese)) && (millis() >= (wartezeit+60000)))
      {                        // mit Wartezeit für eine Temperaturstabilität
      heizung=0;               // Heizung ausschalten
      hysterese=0;             //Verschiebung des Schaltpunktes um die Hysterese
      wartezeit=millis();      //Start Wartezeitzählung
      }
 
     //Einschalten wenn kleiner Sollwert und dann Wartezeit  
   if ((heizung == 0) && (isttemp <= (sollwert-0.5)) && (millis() >= (wartezeit+60000)))
      {                        // mit Wartezeit für eine Temperaturstabilität
      heizung=1;               // Heizung einschalten
      hysterese=0;             //Verschiebung des Schaltpunktes um die Hysterese
      wartezeit=millis();      //Start Wartezeitzählung
      }
      
     //Ausschalten vor der Wartezeit, wenn Sollwert um 0,5 überschritten
     if ((heizung == 1) && (isttemp >= (sollwert+0.5)))
     {
      heizung=0;               // Heizung ausschalten
      hysterese=0;             //Verschiebung des Schaltpunktes um die Hysterese
      wartezeit=millis();      //Start Wartezeitzählung
     }
  }
      
// Zeigt den Buchstaben "H" bzw. "K", wenn Heizen oder Kühlen----------
// und schalter die Pins--------------------------------------------
  if (heizung == 1)
    {
      if (regelung == 1)              //Maischen
        myGLCD.print("H", LEFT, 40);
      if (regelung == 2)              //Kühlen
        myGLCD.print("K", LEFT, 40);
      if (regelung == 3)              //Kochen
        myGLCD.print("H", LEFT, 40);
      digitalWrite(schalterH1, LOW);   // einschalten
      digitalWrite(schalterH2, LOW);   // einschalten
    }
    else
    {
      myGLCD.print(" ", LEFT, 40);
      digitalWrite(schalterH1, HIGH);   // ausschalten
      digitalWrite(schalterH2, HIGH);   // ausschalten
    }
 //Ende Heizregelung---------------------------------------------------
 
 
 //Kühlregelung -----------------------------------------------------
 if (regelung == 2)
 {
  // Temperaturanzeige Sollwert --------------------------------------- 
  myGLCD.printNumF(int(sollwert), 1,46, 8);
  myGLCD.print("~C", RIGHT, 8);
  //-------------------------------------------------------------------
  if ((isttemp >= (sollwert+1)) && (millis() >= (wartezeit+60000)))
      {                        // mit Wartezeit für eine Temperaturstabilität
      heizung=1;               // einschalten
      wartezeit=millis();      //Start Wartezeitzählung
      }
 
   if ((isttemp <= sollwert-1) && (millis() >= (wartezeit+60000)))
      {                        // mit Wartezeit für eine Temperaturstabilität
      heizung=0;               // ausschalten
      wartezeit=millis();      //Start Wartezeitzählung
      }
      
     //Ausschalten vor der Wartezeit, wenn Sollwert um 2 unterschritten
     if (isttemp < (sollwert-2))
     {
     heizung=0;               // ausschalten
     wartezeit=millis();      //Start Wartezeitzählung
     }
 }
 //Ende Kühlregelung ---------------------------------------------

 //Kochen => dauernd ein----------------------------------------------
 if (regelung == 3)
   {
   heizung=1;               // einschalten
   }
 //Ende Kochen -----------------------------------------------------------
 
 

  // Drehgeber und Tastenabfrage -------------------------------------------------
  isr_2();   //drehgeber abfragen
  isr_3();   //drehgeber abfragen
  getButton();  //Taster abfragen
  //---------------------------------------------------------------
  
    
  // Abfrage Modus

  if (modus == 0)   //Hauptschirm
    {
    regelung=0;
    funktion_hauptschirm();
    }
 
  if (modus == 11)   //Maischmenue
    {
    regelung=0;
    funktion_maischmenue();
    }   
    
  if (modus == 1)   //Nur Temperaturregelung
    {
    regelung=1;
    funktion_temperatur();
    }
    
  if (modus == 2)   //Nachgusswasserbereitung
    {
    regelung=1;
    funktion_temperatur();
    }
    
  if (modus == 5)   //Kühlen
    {
    regelung=2;
    funktion_temperatur();
    }
    
 if (modus == 10)   //Alarmtest
    {
    regelung=0;
    rufmodus=0;
    myGLCD.print("Alarmtest", RIGHT, 32);
    modus=31;
    }    
       
  if (modus == 19)   //Eingabe Anzahl der Rasten
    {
    regelung=0;
    funktion_rastanzahl();
    }
    
  if (modus == 20)   //Eingabe Einmaischtemperatur
    {
    regelung=0;
    funktion_maischtemperatur();
    } 

  if (modus == 21)   //Eingabe der Temperatur der Rasten
    {
    regelung=0;
    funktion_rasteingabe(); 
    }
    
  if (modus == 22)   //Eingabe der Rastzeitwerte
    {
    regelung=0;
    funktion_zeiteingabe(); 
    }
    
  if (modus == 23)  //Eingabe Rührwerk an/aus -> nicht eingebaut
    {
    regelung=0;
    funktion_ruehrwerk();
    }
    
  if (modus == 24)  //Eingabe Braumeisterruf an/aus ?
    {
    regelung=0;
    funktion_braumeister();   
    }
    
  if (modus == 25)   //Eingabe der Rasttemperaturwerte
    {
    regelung=0;
    funktion_enttempeingabe();
    }
    
  if (modus == 26)   //Startabfrage
    {
    regelung=0;
    funktion_startabfrage();
    }
    
  if (modus == 27)   //Automatik Maischtemperatur
    {
    regelung=1;
    funktion_maischtemperaturautomatik();
    }
    
  if (modus == 28)   //Automatik Temperatur
    {
    regelung=1;
    funktion_tempautomatik();
    }
    
  if (modus == 29)   //Automatik Zeit
    {
    regelung=1;
    funktion_zeitautomatik();
    }
    
  if (modus == 30)   //Automatik Endtemperatur
    {
    regelung=1;
    funktion_endtempautomatik();
    }
    
  if (modus == 31)   //Braumeisterrufalarm
    {
    funktion_braumeisterrufalarm();
    }

  if (modus == 32)   //Braumeisterruf
    {
    funktion_braumeisterruf();  
    }   
       
  if (modus == 40)   //Kochen Kochzeit
    {
    regelung=3;          //Kochen => dauernd eingeschaltet
    funktion_kochzeit();
    } 
          
  if (modus == 41)   //Kochen Anzahl der Hopfengaben
    {
    regelung=3;          //Kochen => dauernd eingeschaltet
    funktion_anzahlhopfengaben();
    }
  
  if (modus == 42)   //Kochen Eingabe der Zeitwerte
    {
    regelung=3;          //Kochen => dauernd eingeschaltet
    funktion_hopfengaben(); 
    }
    
  if (modus == 43)   //Startabfrage
    {
    regelung=3;          //Kochen => dauernd eingeschaltet
    funktion_starthopfengaben();
    }
     
  if (modus == 44)   //Kochen Automatik Zeit
    {
    regelung=3;          //Kochen => dauernd eingeschaltet
    funktion_hopfenzeitautomatik();
    }
   
  if (modus == 60)   //Timer
    {
    regelung=0;
    funktion_timer();
    }   

  if (modus == 61)   //Timerlauf
    {
    regelung=0;
     funktion_timerlauf();
    }  


  if (modus == 80)   //Abbruch
    {
    funktion_abbruch();
    }


  // -----------------------------------------------------------------

}
// Ende Loop
// ------------------------------------------------------------------




//Funktionen============================================================= 
// ab hier Funktionen

// Funtionen für Drehgeber
void isr_2()
{                                              // Pin2 went LOW
  delay(1);                                                // Debounce time
  if(digitalRead(2) == LOW){                               // Pin2 still LOW ?
    if(digitalRead(3) == HIGH && halfright == false){      // -->
      halfright = true;                                    // One half click clockwise
    }  
    if(digitalRead(3) == LOW && halfleft == true){         // <--
      halfleft = false;                                    // One whole click counter-
      {
        number++;
      }                                                    // clockwise
    }
  }
}

void isr_3()
{                                             // Pin3 went LOW
  delay(1);                                               // Debounce time
  if(digitalRead(3) == LOW){                              // Pin3 still LOW ?
    if(digitalRead(2) == HIGH && halfleft == false){      // <--
      halfleft = true;                                    // One half  click counter-
    }                                                     // clockwise
    if(digitalRead(2) == LOW && halfright == true){       // -->
      halfright = false;                                  // One whole click clockwise
      {
        number--;
      } 
    }
  }
}
// ----------------------------------------------------------------------


// Funktion Tastendruck getButton-----------------------------------------------
int getButton(){
  //delay(100);  
  ButtonVoltage = digitalRead(taster);
  if (ButtonVoltage  == HIGH) 
  {
    ButtonPressed = 0;             
    abbruchtaste=millis();
  }

  else if (ButtonVoltage == LOW) 
  {
    ButtonPressed = 1;
    if (millis() >= (abbruchtaste + 2000))     //Taste 2 Sekunden drücken
      modus=80;                                //abbruchmodus=modus80

  }

  return ButtonPressed;
}
//--------------------------------------------------------------------


// Funktion Hauptschirm---------------------------------
void funktion_hauptschirm()      //Modus=0
{ 
  if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Maischen", 16, 0);
    myGLCD.print("Kochen", 16, 8);
    myGLCD.print("Timer", 16, 16);
    myGLCD.print("Kuehlen", 16, 24);
    drehen=0;
    anfang=1;
  }
  
  
  if (drehen <= 0)
   {
   drehen=0;
   }
   if (drehen >= 3)
   {
   drehen=3;
   }  
   
    
  if (drehen == 0)
   {
   rufmodus=11;
   myGLCD.print("=>", LEFT, 0);
   myGLCD.print("  ", LEFT, 8);
   myGLCD.print("  ", LEFT, 16);
   myGLCD.print("  ", LEFT, 24);
   }
   
   if (drehen == 1)
   {
   rufmodus=40;
   myGLCD.print("  ", LEFT, 0);
   myGLCD.print("=>", LEFT, 8);
   myGLCD.print("  ", LEFT, 16);
   myGLCD.print("  ", LEFT, 24);
   }
   
   if (drehen == 2)
   {
   rufmodus=60;
   myGLCD.print("  ", LEFT, 0);
   myGLCD.print("  ", LEFT, 8);
   myGLCD.print("=>", LEFT, 16);
   myGLCD.print("  ", LEFT, 24);
   }
   
   if (drehen == 3)
   {
   rufmodus=5;
   myGLCD.print("  ", LEFT, 0);
   myGLCD.print("  ", LEFT, 8);
   myGLCD.print("  ", LEFT, 16);
   myGLCD.print("=>", LEFT, 24);
   }
  
     
   
  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus=rufmodus;
      if (modus == 5)
        {                               //Übergabe an Modus1
        isttemp_ganzzahl=isttemp;       //isttemp als Ganzzahl
        drehen=isttemp_ganzzahl;    //ganzzahliger Vorgabewert
        }                               //für Sollwert 
      
      anfang=0;
      myGLCD.clrScr();
    }
 
}
//-----------------------------------------------------------------


// Funktion Hauptschirm---------------------------------
void funktion_maischmenue()      //Modus=01
{ 
  if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Manuell", 16, 0);
    myGLCD.print("Automatik", 16, 8);
    myGLCD.print("Nachguss", 16, 16);
    drehen=0;
    anfang=1;
  }
  
  
  if (drehen <= 0)
   {
   drehen=0;
   }
   if (drehen >= 2)
   {
   drehen=2;
   }  
   
    
  if (drehen == 0)
   {
   rufmodus=1;
   myGLCD.print("=>", LEFT, 0);
   myGLCD.print("  ", LEFT, 8);
   myGLCD.print("  ", LEFT, 16);
   }
   
   if (drehen == 1)
   {
   rufmodus=19;
   myGLCD.print("  ", LEFT, 0);
   myGLCD.print("=>", LEFT, 8);
   myGLCD.print("  ", LEFT, 16);
   }
   
   if (drehen == 2)
   {
   rufmodus=2;
   myGLCD.print("  ", LEFT, 0);
   myGLCD.print("  ", LEFT, 8);
   myGLCD.print("=>", LEFT, 16);
   }
   
   
  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus=rufmodus;
      if (modus == 1)
        {                               //Übergabe an Modus1
        isttemp_ganzzahl=isttemp;       //isttemp als Ganzzahl
        drehen=(isttemp_ganzzahl + 10); //ganzzahliger Vorgabewert 10°C über Ist
        }                               //für Sollwert 
      if (modus == 2)
        {                               //Übergabe an Modus2
        drehen=78;                      //Nachgusstemperatur
        }                               //für Sollwert 
      anfang=0;
      myGLCD.clrScr();
    }
 
}
//-----------------------------------------------------------------


// Funktion nur Temperaturregelung---------------------------------
void funktion_temperatur()      //Modus=1 bzw.2
{ 
  sollwert=drehen;
  
  if (modus == 1)
     myGLCD.print("Manuell", LEFT, 0);
     
  if (modus == 2)
     myGLCD.print("Nachguss", LEFT, 0);
     
    if ((modus == 1) && (isttemp >= sollwert))   //Manuell -> Sollwert erreicht
    {
    modus=80;                  //Abbruch nach Rufalarm
    rufmodus=modus;
    modus=31;
    regelung=0;                //Regelung aus
    heizung=0;                 //Heizung aus
    y=0;
    braumeister[y]=1;          // Ruf und Abbruch
    }
    
    if ((modus == 2) && (isttemp >= sollwert) && (nachgussruf == false))   //Nachguss -> Sollwert erreicht
    {
    nachgussruf=true;
    rufmodus=modus;            //Rufalarm
    modus=31;
    y=0;
    braumeister[y]=2;          //nur Ruf und weiter mit Regelung
    }
     
  if (modus == 5)
     myGLCD.print("Kuehlen", LEFT, 0);
     
  myGLCD.print("soll", 20, 8);

}
//-----------------------------------------------------------------


// Funktion Eingabe der Rastanzahl------------------------------------
void funktion_rastanzahl()          //Modus=19
{
  if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Eingabe", LEFT, 0);
    myGLCD.print("Rasten", LEFT, 16);
    
    drehen=rasten;
    anfang=1;
  }
 
  //Vorgabewerte bei verschiedenen Rasten
  if (rasten != drehen)
  {
    if ((int)drehen == 1)
        {
        rastTemp[1]=67; rastZeit[1]=30;
        maischtemp=65;
        }
    if (drehen == 2)
        {
        rastTemp[1]=62; rastZeit[1]=30;
        rastTemp[2]=72; rastZeit[2]=35;
        maischtemp=55;
        }
    if (drehen == 3)
        {
        rastTemp[1]=55; rastZeit[1]=15;
        rastTemp[2]=64; rastZeit[2]=35;
        rastTemp[3]=72; rastZeit[3]=25;
        maischtemp=45;
        }
    if (drehen == 4)
        {
        rastTemp[1]=40; rastZeit[1]=20;
        rastTemp[2]=55; rastZeit[2]=15;
        rastTemp[3]=64; rastZeit[3]=35;
        rastTemp[4]=72; rastZeit[4]=25;
        maischtemp=35;
        }
    if (drehen == 5)
        {
        rastTemp[1]=35; rastZeit[1]=20;
        rastTemp[2]=40; rastZeit[2]=20;
        rastTemp[3]=55; rastZeit[3]=15;
        rastTemp[4]=64; rastZeit[4]=35;
        rastTemp[5]=72; rastZeit[5]=25;
        maischtemp=30;
        }
  }
       
  rasten=drehen;

  
  if (rasten <= 1)
  {
    rasten=1;
    drehen=1;
  }
  if (rasten >= 5)
  {
    rasten=5;
    drehen=5;
  } 
  
  myGLCD.printNumI(rasten, 40, 16);

  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus++;
      anfang=0;
    } 
}
//------------------------------------------------------------------


// Funktion Maischtemperatur-----------------------------------------
void funktion_maischtemperatur()      //Modus=20
{

  if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Eingabe", LEFT, 0);
    drehen=maischtemp;
    anfang=1;
  }
  
  maischtemp=drehen;

  if (maischtemp <= 10)
    maischtemp=10;

  if (maischtemp >= 105)
    maischtemp=105;

  
  myGLCD.print("Maischtemp", LEFT, 8);
  myGLCD.printNumF(int(maischtemp), 1, 44, 16);
  myGLCD.print("~C", 68, 16);

  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus++;
      anfang=0;
    } 
}
//------------------------------------------------------------------ 


// Funktion Rasteingabe Temperatur----------------------------------
void funktion_rasteingabe()      //Modus=21
{
  
  if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Eingabe", LEFT, 0);
    drehen=rastTemp[x];
    anfang=1;
  }
  
  rastTemp[x]=drehen;
  
  if (rastTemp[x] <= 10)
  {
    rastTemp[x]=9;
    drehen=9;
  } 
  if (rastTemp[x] >= 105)
  {
    rastTemp[x]=105;
    drehen=105;
  }


  myGLCD.printNumI(x, LEFT, 8);
  myGLCD.print(".Rast", 8, 8);
  myGLCD.printNumF(int(rastTemp[x]), 1, 48, 8);
  myGLCD.print("~C", RIGHT, 8);
  


  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus++;
      anfang=0;
    } 
}
//------------------------------------------------------------------


// Funktion Rasteingabe Zeit----------------------------------------
void funktion_zeiteingabe()      //Modus=22
{

  if (anfang == 0)
  {
    drehen=rastZeit[x];
    anfang=1;
  }
  
  rastZeit[x]=drehen;
  
  if (rastZeit[x] <= 1)
  {
    rastZeit[x]=1;
    drehen=1;
  } 
  if (rastZeit[x] >= 99)
  {
    rastZeit[x]=99;
    drehen=99;
  }

  if (rastZeit[x] < 10)
  {
  myGLCD.print(" ", 48, 16);
  myGLCD.printNumI(rastZeit[x], 54, 16);
  myGLCD.print("min", RIGHT, 16);
  }
  else
  {
  myGLCD.printNumI(rastZeit[x], 48, 16);
  myGLCD.print("min", RIGHT, 16);
  }
  

  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus++;
      anfang=0;
    } 
}
//------------------------------------------------------------------


// Funktion Rührwerk------------------------------------------------
void funktion_ruehrwerk() //Modus=23
 {
 modus++;       //Moduserhöhung
 }
// nicht ausgeführt
//------------------------------------------------------------------


// Funktion Braumeister---------------------------------------------
void funktion_braumeister() //Modus=24
{

  if (anfang == 0)
  {
    drehen=braumeister[x];
    anfang=1;
  }
 
  braumeister[x]=drehen;
  //delay(200);

  if (braumeister[x] < 0)
  {
    braumeister[x]=0;
    drehen=0;
  }
  
  if (braumeister[x] > 2)
  {
    braumeister[x]=2;
    drehen=2;
  }  
  
  myGLCD.print("Ruf", 18, 24);

  if (braumeister[x] == 0)
   myGLCD.print("  Nein  ", RIGHT, 24);
   
  if (braumeister[x] == 1)
   myGLCD.print("Anhalten", RIGHT, 24);
   
  if (braumeister[x] == 2)
   myGLCD.print("  Signal", RIGHT, 24);

 
  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    {
      einmaldruck=false;     //Überprüfung loslassen der Taste Null  
      if (x < rasten)
      {
        x++;
        modus=21;             //Sprung zur Rasttemperatureingabe 
        anfang=0;
      }      
      else
      {
        x=1;
        modus++;             //Sprung zur Rastzeiteingabe 
        anfang=0;
      }
    }
}
//------------------------------------------------------------------  


// Funktion Ende Temperatur-----------------------------------------
void funktion_enttempeingabe()      //Modus=25
{

  if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Eingabe", LEFT, 0);
    drehen=endtemp;
    anfang=1;
  }
  
  endtemp=drehen;

  if (endtemp <= 10)
    endtemp=10;

  if (endtemp >= 80)
    endtemp=80;

  myGLCD.print("Endtemperatur", LEFT, 8);
  myGLCD.printNumF(int(endtemp), 1, 44, 16);
  myGLCD.print("~C", 68, 16);

  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus++;
      anfang=0;
    } 
}
//------------------------------------------------------------------ 


// Funktion Startabfrage--------------------------------------------
void funktion_startabfrage()      //Modus=26
{
  if (anfang == 0)
    {
    myGLCD.clrScr();
    myGLCD.print("Automatik", LEFT, 0);
    anfang=1;
    altsekunden=millis();
    }

  if (millis() >= (altsekunden+1000))
  {
  myGLCD.print("       ", CENTER, 16);
   if (millis() >= (altsekunden+1500))
     altsekunden=millis();
  }
  else
  myGLCD.print("Start ?", CENTER, 16);
  

  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      anfang=0;
      modus++;
    } 
}
//------------------------------------------------------------------ 


// Funktion Automatik Maischtemperatur---------------------------------
void funktion_maischtemperaturautomatik()      //Modus=27
{
 if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Auto", LEFT, 0);
    myGLCD.print("Maischen", RIGHT, 0);
   drehen=maischtemp;      //Zuordnung Encoder
   anfang=1;
  }

  maischtemp=drehen;

  sollwert=maischtemp;
    
  if (isttemp >= sollwert)  // Sollwert erreicht ?
   {
   modus++;
   rufmodus=modus;
   y=0;
   braumeister[y]=1;
   modus=31;
   }
}   
//------------------------------------------------------------------


// Funktion Automatik Temperatur------------------------------------
void funktion_tempautomatik()      //Modus=28
{
  if (anfang == 0)
  {

    myGLCD.clrScr();
    myGLCD.print("Auto", LEFT, 0);
    myGLCD.printNumI(x, 48, 0);
    myGLCD.print(".Rast", RIGHT, 0);
   
    drehen=rastTemp[x];
    anfang=1;
  }
  
  rastTemp[x]=drehen;
  
  sollwert=rastTemp[x];
 
  if (isttemp >= sollwert)  // Sollwert erreicht ?
   {
    modus++;                //zur Zeitautomatik
    anfang=0;
   }
}
//------------------------------------------------------------------ 


// Funktion Automatik Zeit------------------------------------------
void funktion_zeitautomatik()      //Modus=29
{
  if (anfang == 0)
    {
    drehen=rastZeit[x];                //Zuordnung für Encoder
    }

  if (rastZeit[x] < 10)
   {
   myGLCD.print(" ", 46, 16);
   myGLCD.printNumI(rastZeit[x], 52, 16);
   myGLCD.print("min", 66, 16);
   }
  else
   {
   myGLCD.printNumI(rastZeit[x], 46, 16);
   myGLCD.print("min", 66, 16);
   }

  
  // Zeitzählung---------------
  if (anfang == 0)
  {
  myGLCD.print("Set Time", 0, 24);

  setTime(00,00,00,00,01,01);   //.........Sekunden auf 0 stellen

  delay(400); //test
  
  sekunden=second();    //aktuell Sekunde abspeichern für die Zeitrechnung
  minutenwert=minute(); //aktuell Minute abspeichern für die Zeitrechnung
  stunden=hour();       //aktuell Stunde abspeichern für die Zeitrechnung

  myGLCD.print("            ", 0, 24);
  anfang=1;
  }
  
                   
  myGLCD.print("00:00", 0, 16);
  
  if (sekunden < 10)
    {
    myGLCD.printNumI(sekunden, 24, 16);
    }
    else
    {
    myGLCD.printNumI(sekunden, 18, 16);
    }

  if (stunden == 0)
    minuten=minutenwert;
   else
    minuten=((stunden*60) + minutenwert);
    
  if (minuten < 10)
   {
    myGLCD.printNumI(minuten, 6, 16);
    }
    else
    {
    myGLCD.printNumI(minuten, 0, 16);
    }
  // Ende Zeitzählung---------------------
  
  rastZeit[x]=drehen;     //Encoderzuordnung
  
  if (minuten >= rastZeit[x])  // Sollwert erreicht ?
    { 
    anfang=0;
    y=x;
    if (x < rasten)
      {
      modus--;                // zur Temperaturregelung
      x++;                    // nächste Stufe
      }
    else
      {
      x=1;                                //Endtemperatur
      modus++;                            //Endtemperatur
      }
    
    if (braumeister[y] > 0)
      {
      rufmodus=modus;
      modus=31;
      }  
    }
 }
//------------------------------------------------------------------ 


// Funktion Automatik Endtemperatur---------------------------------
void funktion_endtempautomatik()      //Modus=30
{
 if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Auto", LEFT, 0);
    myGLCD.print("Endtemp", RIGHT, 0);
   drehen=endtemp;      //Zuordnung Encoder
   anfang=1;
  }

  endtemp=drehen;

  sollwert=endtemp;
    
  if (isttemp >= sollwert)  // Sollwert erreicht ?
    {
    modus=80;                  //Abbruch
    rufmodus=modus;
    modus=31;
    regelung=0;                //Regelung aus
    heizung=0;                 //Heizung aus
    y=0;
    braumeister[y]=1;
    }   
}
//------------------------------------------------------------------


// Funktion braumeisterrufalarm---------------------------------------
void funktion_braumeisterrufalarm()      //Modus=31
{
  if (anfang == 0)
   {
     rufsignalzeit=millis();
     anfang=1;
   }

  if (millis() >= (altsekunden+1000))   //Bliken der Anzeige und RUF
   {                                      
   myGLCD.print("            ", RIGHT, 24); 
   digitalWrite(schalterB, LOW); 
   if (millis() >= (altsekunden+1500))
     {
     altsekunden=millis();
     pause++;
     }
   }                                   
  else           
   {
   myGLCD.print("RUF", RIGHT, 24);
   if (pause <= 4)
     {
     digitalWrite(schalterB, HIGH);
      }
   if (pause > 8)
     pause=0;
   }                                   //Bliken der Anzeige und RUF
 
 
 if ((pause == 4) || (pause == 8))     //Funkalarm schalten
   digitalWrite(schalterF, HIGH);    //Funkalarm ausschalten
  else
   digitalWrite(schalterF, LOW);   // Funkalarm ausschalten


 //20 Sekunden Rufsignalisierung wenn "Ruf Signal" 
 if (braumeister[y] == 2 && millis()>= (rufsignalzeit+20000))
      {
        anfang=0;
        pause=0;
        digitalWrite(schalterB, LOW);   // Alarm ausschalten
        digitalWrite(schalterF, LOW);   // Funkalarm ausschalten
        modus=rufmodus;
        einmaldruck=false;
      }
 //weiter mit Programmablauf
 
  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      pause=0;
      anfang=0;
      digitalWrite(schalterB, LOW);   // Alarm ausschalten
      digitalWrite(schalterF, LOW);   // Funkalarm ausschalten
      if (braumeister[y] == 2)
      {
       myGLCD.print("   ", RIGHT, 24);
       modus=rufmodus;
      }
      else
       modus++;
    }             
}
//------------------------------------------------------------------


// Funktion braumeisterruf------------------------------------------
void funktion_braumeisterruf()      //Modus=32
{
  if (anfang == 0)
    {
    anfang=1;
    }


  if (millis() >= (altsekunden+1000))
   {
   myGLCD.print("        ", RIGHT, 24);
   if (millis() >= (altsekunden+1500))
     altsekunden=millis();
   }
   else
   myGLCD.print("weiter ?", RIGHT, 24);

  
  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
      { 
      einmaldruck=false;
      myGLCD.print("        ", RIGHT, 24);     //Text "weiter ?" löschen
      
      myGLCD.print("             ", RIGHT, 32); //Löscht Text bei
      sensorfehler=0;                           //Sensorfehler oder Alarmtest
      
      anfang=0;
      modus=rufmodus;
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
 if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Kochen", RIGHT, 0);
    myGLCD.print("Zeit", LEFT, 16);
    
    fuenfmindrehen=kochzeit;
    anfang=1;
  }
 
 kochzeit=fuenfmindrehen; //5min-Sprünge
 
 if (kochzeit <= 20)
  {
    kochzeit=20;
    fuenfmindrehen=20;
  } 
  if (kochzeit >= 180)
  {
    kochzeit=180;
    fuenfmindrehen=180;
  }
 
 if (kochzeit < 100)
  {  
    myGLCD.print(" ", 42, 16);
    myGLCD.printNumI(kochzeit, 48, 16);
    myGLCD.print("min", RIGHT, 16);
  }
  else
  {
    myGLCD.printNumI(kochzeit, 42, 16);
    myGLCD.print("min", RIGHT, 16);
  }
 
  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus++;
      anfang=0;
    } 
}
//------------------------------------------------------------------ 

// Funktion Anzahl der Hopfengaben------------------------------------------   
void funktion_anzahlhopfengaben()      //Modus=41
{
 if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Kochen", RIGHT, 0);
    myGLCD.print("Hopfengaben", LEFT, 16);
    
    drehen=hopfenanzahl;
    anfang=1;
  }
 
  hopfenanzahl=drehen;
  
  if (hopfenanzahl <= 1)
  {
    hopfenanzahl=1;
    drehen=1;
  }
  if (hopfenanzahl >= 5)
  {
    hopfenanzahl=5;
    drehen=5;
  } 
  
  myGLCD.printNumI(hopfenanzahl, RIGHT, 16);

  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus++;
      anfang=0;
    } 
}
//------------------------------------------------------------------ 

// Funktion Hopfengaben-------------------------------------------   
void funktion_hopfengaben()      //Modus=42
{

  if (anfang == 0)
  {
    x=1;
    fuenfmindrehen=hopfenZeit[x];
    anfang=1;
    myGLCD.clrScr();
    myGLCD.print("Kochen", RIGHT, 0);
  }
  
  myGLCD.printNumI(x, LEFT, 8);
  myGLCD.print(". Hopfengabe", 8, 8);
  myGLCD.print("nach", LEFT, 16); 
  
  hopfenZeit[x]=fuenfmindrehen;
  
  
  if (hopfenZeit[x] <= (hopfenZeit[(x-1)]+5))
   {
    hopfenZeit[x]=(hopfenZeit[(x-1)]+5);
    fuenfmindrehen=(hopfenZeit[(x-1)]+5);
   } 
  if (hopfenZeit[x] >= (kochzeit-5))
   {
    hopfenZeit[x]=(kochzeit-5);
    fuenfmindrehen=(kochzeit-5);
   }
  

  if (hopfenZeit[x] < 10)
  {
  myGLCD.print("  ", 42, 16);
  myGLCD.printNumI(hopfenZeit[x], 54, 16);
  myGLCD.print("min", RIGHT, 16);
  }
  
  if ((hopfenZeit[x] < 100) && (hopfenZeit[x] >= 10))
  {
  myGLCD.print(" ", 42, 16);
  myGLCD.printNumI(hopfenZeit[x], 48, 16);
  myGLCD.print("min", RIGHT, 16);
  }
  
  if (hopfenZeit[x] >= 100)
  {
  myGLCD.printNumI(hopfenZeit[x], 42, 16);
  myGLCD.print("min", RIGHT, 16);
  }
  

  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      if (x < hopfenanzahl)
      {
        x++;
        myGLCD.print("  ", LEFT, 8);
        myGLCD.print("   ", 42, 16);
        delay(400);
      }      
      else
      {
      x=1;
      modus++;
      anfang=0;
      }
    } 
}
//------------------------------------------------------------------ 


// Funktion Hopfengabenstart-------------------------------------------   
void funktion_starthopfengaben()      //Modus=43
{
if (anfang == 0)
    {
    myGLCD.clrScr();
    myGLCD.print("Kochen", RIGHT, 0);
    anfang=1;
    delay(1000);    //kurze Wartezeit, damit die Startbestätigung
                    //nicht einfach "überdrückt" wird
    altsekunden=millis();
    }

  if (millis() >= (altsekunden+1000))
  {
  myGLCD.print("       ", CENTER, 16);
   if (millis() >= (altsekunden+1500))
     altsekunden=millis();
  }
  else
  myGLCD.print("Start ?", CENTER, 16);

  
  
  if (isttemp >= 98)
   {
   digitalWrite(schalterB, HIGH);
   digitalWrite(schalterF, HIGH);
   myGLCD.print("Kochbeginn", CENTER, 24);
   }
   else
   {
   myGLCD.print("Zeitablauf", CENTER, 24);
   }
    
    
  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      digitalWrite(schalterB, LOW);   //Ruf ausschalten
      digitalWrite(schalterF, LOW);   //Funkruf ausschalten
      einmaldruck=false;
      anfang=0;
      modus++;
    } 
}
//------------------------------------------------------------------ 


// Funktion Hopfengaben Benachrichtigung------------------------------------------   
void funktion_hopfenzeitautomatik()      //Modus=44
{
  if (anfang == 0)
  {
    x=1;
    anfang=1;
    myGLCD.clrScr();
    myGLCD.print("Kochen", RIGHT, 0);
    
   if (kochzeit < 10)
    {
     myGLCD.print("  ", 46, 8);
     myGLCD.printNumI(kochzeit, 58, 8);
     myGLCD.print("min", 66, 8);
    }
    
   if ((kochzeit >= 10) && (kochzeit < 100))
    {
     myGLCD.print(" ", 46, 8);
     myGLCD.printNumI(kochzeit, 52, 8);
     myGLCD.print("min", 66, 8);
    }
    
    if (kochzeit >= 100)
    {
     myGLCD.printNumI(kochzeit, 46, 8);
     myGLCD.print("min", 66, 8);
    }
    
    
    myGLCD.print("Set Time", 0, 24);
  
    setTime(00,00,00,00,01,01);   //.........Sekunden auf 0 stellen
  
    delay(400); //test
    myGLCD.print("         ", 0, 24);
    
    sekunden=second();    //aktuell Sekunde abspeichern für die Zeitrechnung
    minutenwert=minute(); //aktuell Minute abspeichern für die Zeitrechnung
    stunden=hour();       //aktuell Stunde abspeichern für die Zeitrechnung

    anfang=1;    
  }
   
   
  if (x <= hopfenanzahl)
   { 
    myGLCD.printNumI(x, LEFT, 24);
    myGLCD.print(".Gabe bei", 6, 24);
    if (hopfenZeit[x] < 10)
    {
     myGLCD.print("  ", 66, 24);
     myGLCD.printNumI(hopfenZeit[x], 78, 24);
    }
    if ((hopfenZeit[x] >= 10) && (hopfenZeit[x] < 100))
    {
     myGLCD.print(" ", 66, 24);
     myGLCD.printNumI(hopfenZeit[x], 72, 24);
    }
    if (hopfenZeit[x] > 100)
    {
     myGLCD.printNumI(hopfenZeit[x], 66, 24);
    }
   }
   else
     myGLCD.print("                    ", 0, 24);
 
  
  myGLCD.print("00:00", 6, 8);
  
  if (sekunden < 10)
    {
    myGLCD.printNumI(sekunden, 30, 8);
    }
    else
    {
    myGLCD.printNumI(sekunden, 24, 8);
    }

  minuten=((stunden*60) + minutenwert);
    
  if (minuten < 10)
    {
     myGLCD.printNumI(minuten, 12, 8);
    }
    
  if ((minuten >= 10) && (minuten < 100))
    {
    myGLCD.printNumI(minuten, 6, 8);
    }
    
  if (minuten >= 100)
    {
    myGLCD.printNumI(minuten, 0, 8);
    }
  
  
  if ((minuten == hopfenZeit[x]) && (x <= hopfenanzahl))    // Hopfengabe
  {
  //Alarm -----
   if (millis() >= (altsekunden+1000))   //Bliken der Anzeige und RUF
    {                                      
     myGLCD.print("        ", RIGHT, 24); 
     digitalWrite(schalterB, LOW); 
     if (millis() >= (altsekunden+1500))
      {
      altsekunden=millis();
      pause++;
      }
     }                                   
    else           
     {
      myGLCD.print("RUF", RIGHT, 24);
     if (pause <= 4)
       {
       digitalWrite(schalterB, HIGH);
       }
      if (pause > 8)
       pause=0;
     }                                   //Bliken der Anzeige und RUF
 
 
   if ((pause == 4) || (pause == 8))     //Funkalarm schalten
     digitalWrite(schalterF, HIGH);    //Funkalarm einschalten
    else
     digitalWrite(schalterF, LOW);   // Funkalarm ausschalten
    //-----------
 
    
    if (ButtonPressed == 0)
     einmaldruck=true;

    if (einmaldruck == true)
      if (ButtonPressed == 1)
      { 
       einmaldruck=false;
       pause=0;
       digitalWrite(schalterB, LOW);   // Alarm ausschalten
       digitalWrite(schalterF, LOW);   // Funkalarm ausschalten
       x++;
      }             
  }
  
 if ((minuten > hopfenZeit[x]) && (x <= hopfenanzahl))    // Alarmende nach 1 Minute
      {
      pause=0;
      digitalWrite(schalterB, LOW);   // Alarm ausschalten
      digitalWrite(schalterF, LOW);   // Funkalarm ausschalten
      x++;
      }
  
 
  if (minuten >= kochzeit)     //Kochzeitende
    {
    modus=80;                  //Abbruch nach Rufalarm
    rufmodus=modus;
    modus=31;
    regelung=0;                //Regelung aus
    heizung=0;                 //Heizung aus
    y=0;
    braumeister[y]=1;
    }
  
  
  
}
//------------------------------------------------------------------ 


// Funktion Timer-------------------------------------------------
void funktion_timer()      //Modus=60
{
 if (anfang == 0)
  {
    myGLCD.clrScr();
    myGLCD.print("Timer", RIGHT, 0);
    myGLCD.print("Zeit", LEFT, 16);
    
    drehen=timer;
    anfang=1;
  }
 
 timer=drehen;
 
 if (timer <= 1)
  {
    timer=1;
    drehen=1;
  } 
  if (timer >= 99)
  {
    timer=99;
    drehen=99;
  }
 
 if (timer < 10)
  {  
    myGLCD.print(" ", 52, 16);
    myGLCD.printNumI(timer, 58, 16);
    myGLCD.print("min", RIGHT, 16);
  }
  else
  {
    myGLCD.printNumI(timer, 52, 16);
    myGLCD.print("min", RIGHT, 16);
  }
 
  if (ButtonPressed == 0)
    einmaldruck=true;

  if (einmaldruck == true)
    if (ButtonPressed == 1)
    { 
      einmaldruck=false;
      modus++;
      anfang=0;
    } 
}
//------------------------------------------------------------------ 


// Funktion Timerlauf-------------------------------------------------
void funktion_timerlauf()      //Modus=61
{
  if (anfang == 0)
  {
    drehen=timer;
    
    anfang=1;
    myGLCD.clrScr();
    myGLCD.print("Timer", RIGHT, 0);
     
    myGLCD.print("Set Time", 0, 16);
  
    setTime(00,00,00,00,01,01);   //.........Sekunden auf 0 stellen
  
    delay(400); //test
    myGLCD.print("         ", 0, 16);
    
    sekunden=second();    //aktuell Sekunde abspeichern für die Zeitrechnung
    minutenwert=minute(); //aktuell Minute abspeichern für die Zeitrechnung
    stunden=hour();       //aktuell Stunde abspeichern für die Zeitrechnung

    anfang=1;    
  }
   
   
  timer=drehen;
   
     
  if (timer >= 99)
   {
     timer=99;
     drehen=99;
   }
  
  if (timer < 10)
   {  
     myGLCD.print(" ", 52, 16);
     myGLCD.printNumI(timer, 58, 16);
     myGLCD.print("min", RIGHT, 16);
   }
   else
   {
     myGLCD.printNumI(timer, 52, 16);
     myGLCD.print("min", RIGHT, 16);
   }
  
  
  myGLCD.print("00:00", 6, 16);
  
  if (sekunden < 10)
    {
    myGLCD.printNumI(sekunden, 30, 16);
    }
    else
    {
    myGLCD.printNumI(sekunden, 24, 16);
    }

  minuten=((stunden*60) + minutenwert);
    
  if (minuten < 10)
    {
     myGLCD.printNumI(minuten, 12, 16);
    }
    else
    {
    myGLCD.printNumI(minuten, 6, 16);
    }
    
    
  if (minuten >= timer)     //Timerende
    {
    modus=80;                  //Abbruch nach Rufalarm
    rufmodus=modus;
    modus=31;
    regelung=0;                //Regelung aus
    heizung=0;                 //Heizung aus
    y=0;
    braumeister[y]=1;
    }
  
  
  
}
//------------------------------------------------------------------ 

// Funktion Abbruch-------------------------------------------------
void funktion_abbruch()       // Modus 80
{ 
  regelung=0;  
  heizung=0;
  wartezeit=-60000;
  digitalWrite(schalterH1, HIGH);
  digitalWrite(schalterH2, HIGH);  
  digitalWrite(schalterB, LOW);   // ausschalten
  digitalWrite(schalterF, LOW);   // ausschalten
  anfang=0;                       //Daten zurücksetzen
  myGLCD.clrScr();                //Rastwerteeingaben
  rufmodus=0;                     //bleiben erhalten
  x=1;                            //bei
  y=1;                            //asm volatile ("  jmp 0"); 
  n=0;                            //wird alles
  einmaldruck=false;              //zurückgesetetzt
  nachgussruf=false;
  pause=0;
  drehen=sollwert;            //Zuweisung für Funktion Temperaturregelung
 
  if (millis() >= (abbruchtaste + 5000)) //länger als 5 Sekunden drücken
    modus=10;                      //Alarmtest
  else
     modus=0;                      //Hauptmenue
  
  // asm volatile ("  jmp 0");       //reset Arduino
}
//------------------------------------------------------------------

