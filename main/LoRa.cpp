#include "LoRa.h"
#include <ArduinoJson.h>

HardwareSerial LoRa(2); // UART2 (Serial2)

// Initialisation du module LoRa-E5
void startLoRa() {
    LoRa.begin(LORA_BAUD, SERIAL_8N1, LORA_RX, LORA_TX);
    pinMode(2, OUTPUT); // LED pour indication (optionnel)
    Serial.println("LoRa module initialized");
}

// Envoi d'une commande AT et affichage de la réponse
void sendAT(String cmd, int waitMs = 1000) {
    LoRa.println(cmd);
    delay(waitMs);
    while (LoRa.available()) {
        String resp = LoRa.readStringUntil('\n');
        Serial.println(resp);
    }
}

// Join OTAA TTN
bool joinNetwork() {
    sendAT("AT");
    sendAT("AT+NJS?"); // Vérifie si déjà joint
    sendAT("AT+APPEUI=" + String(APPEUI));
    sendAT("AT+APPKEY=" + String(APPKEY));
    sendAT("AT+JOIN", 8000);
    sendAT("AT+NJS?");
    return true;
}

// Envoi d'un JSON via LoRa (uplink)
void sendLoRa(AP results[], int n, String location, String timestamp) {
    StaticJsonDocument<256> doc;
    doc["location"] = location;
    doc["timestamp"] = timestamp;

    // Inclut jusqu'à 5 AP détectés
    for (int i = 0; i < n && i < 5; i++) {
        doc["AP" + String(i+1) + "_MAC"] = results[i].bssid;
        doc["AP" + String(i+1) + "_RSSI"] = results[i].rssi;
    }

    char payload[256];
    serializeJson(doc, payload);

    sendAT("AT+SEND=1:" + String(payload), 2000);
    Serial.println("LoRa uplink sent: " + String(payload));
}

// Gestion d'un uplink reçu (affichage seulement)
void handleUplink(String msg) {
    Serial.println("Uplink received: " + msg);
}

// Gestion d'un downlink (à adapter selon projet)
void handleDownlink(String msg) {
    Serial.println("Downlink received: " + msg);
}
