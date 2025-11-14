#ifndef LORA_H
#define LORA_H

#include <Arduino.h>
#include "Scan.h"

#define LORA_RX 16
#define LORA_TX 17
#define LORA_BAUD 9600

#define APPEUI "0000000000000000"
#define APPKEY "3D0D6AA79FA5CD474E71E9A92D533BCC"

extern HardwareSerial LoRa;

// Fonctions principales
void startLoRa();
bool joinNetwork();
void sendLoRa(AP results[], int n, String location, String timestamp);
void handleUplink(String msg);
void handleDownlink(String msg);

#endif
