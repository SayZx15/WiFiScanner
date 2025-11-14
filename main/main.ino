#include "Scan.h"
#include "LoRa.h"

// ----------------- CONFIGURATION -----------------
const unsigned long SCAN_INTERVAL = 30 * 1000UL; // toutes les 30s
unsigned long lastScan = 0;
String currentLocation = "UNKNOWN";   // localisation courante

AP results[10]; // tableau pour stocker les AP détectés

// ----------------- SETUP -----------------
void setup() {
    Serial.begin(115200);
    delay(1000);

    // Initialisation modules
    startWiFi();
    startLoRa();

    // Joindre le réseau LoRa
    if (joinNetwork()) {
        Serial.println("LoRa OTAA join successful!");
    } else {
        Serial.println("LoRa OTAA join failed!");
    }

    Serial.println("{\"info\":\"ESP32 Scan Ready. Use LOCATION:<name> to set position\"}");
    lastScan = 0;
}

// ----------------- LOOP -----------------
void loop() {

    // Lecture série pour changer la localisation
    if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        line.trim();
        if (line.startsWith("LOCATION:")) {
            currentLocation = line.substring(9);
            Serial.printf("LOCATION SET TO: %s\n", currentLocation.c_str());
        }
    }

    // Scan WiFi et envoi via LoRa
    unsigned long now = millis();
    if (now - lastScan >= SCAN_INTERVAL || lastScan == 0) {
        int n = doScan(results, 10); // scan WiFi
        String timestamp = String(millis()); // ou un vrai timestamp
        sendLoRa(results, n, currentLocation, timestamp); // envoi JSON via LoRa
        lastScan = now;
    }

    delay(100);
}
