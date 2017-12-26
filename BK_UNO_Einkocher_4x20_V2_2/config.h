#ifndef _CONFIG_H
#define _CONFIG_H

#pragma once

#define DEBUG

#define WIFI_SSID "ssid"
#define WIFI_PSK "***password***"

#define FIRMWAREVERSION "3.0.0"

enum PinAssignments {
  encoderPinA = D5,
  encoderPinB = D6,
  tasterPin = D7,
  oneWirePin = D3,
  heizungPin = D4,
  beeperPin = D8,
};

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
