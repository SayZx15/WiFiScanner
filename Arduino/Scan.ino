#include <WiFi.h>

// Intervalle entre scans (ms)
const unsigned long SCAN_INTERVAL = 30 * 1000UL; // toutes les 30s

unsigned long lastScan = 0;
String currentLocation = "UNKNOWN";   // ← localisation courante

void setup() {
  Serial.begin(115200);
  delay(1000);

  WiFi.mode(WIFI_MODE_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println("{\"info\":\"ESP32 Scan Ready. Use LOCATION:<name> to set position\"}");
  lastScan = 0;
}

void doScan() {
  int n = WiFi.scanNetworks(false, true);

  for (int i = 0; i < n; ++i) {
    String ssid = WiFi.SSID(i);
    String bssid = WiFi.BSSIDstr(i);
    int32_t rssi = WiFi.RSSI(i);
    int32_t channel = WiFi.channel(i);

    // JSON avec localisation
    Serial.printf(
      "{\"location\":\"%s\",\"ssid\":\"%s\",\"bssid\":\"%s\",\"rssi\":%d,\"channel\":%d}\n",
      currentLocation.c_str(),
      ssid.c_str(),
      bssid.c_str(),
      rssi,
      channel
    );
  }

  WiFi.scanDelete();
}

void loop() {

  // --- Lecture série pour changer la localisation ---
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();

    if (line.startsWith("LOCATION:")) {
      currentLocation = line.substring(9);
      Serial.printf("LOCATION SET TO: %s\n", currentLocation.c_str());
    }
  }
  // ---------------------------------------------------

  unsigned long now = millis();
  if (now - lastScan >= SCAN_INTERVAL || lastScan == 0) {
    doScan();
    lastScan = now;
  }

  delay(100);
}
