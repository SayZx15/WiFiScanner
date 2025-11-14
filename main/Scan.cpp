#include "Scan.h"
#include <WiFi.h>

void startWiFi() {
    WiFi.mode(WIFI_MODE_STA);
    WiFi.disconnect();
    delay(100);
}

int doScan(AP results[], int maxResults) {
    int n = WiFi.scanNetworks(false, true);
    int count = min(n, maxResults);
    for (int i = 0; i < count; i++) {
        results[i].ssid = WiFi.SSID(i);
        results[i].bssid = WiFi.BSSIDstr(i);
        results[i].rssi = WiFi.RSSI(i);
        results[i].channel = WiFi.channel(i);
    }
    WiFi.scanDelete();
    return count;
}
