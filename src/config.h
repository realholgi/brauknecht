#pragma once

#define DEBUG 1

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

#define CFGFILE "/config.json"
#define KOCHSCHWELLE 98

#define ENCODER_STEPS_PER_NOTCH 4   // Change this depending on which encoder is used

//#ifndef DEBUG
//#define DEBUG 1 // uncomment this line to enable serial diagnostic messages
//#endif

