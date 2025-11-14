#ifndef SCAN_H
#define SCAN_H

#include <Arduino.h>


struct AP {
    String ssid;
    String bssid;
    int32_t rssi;
    int32_t channel;
};

void startWiFi();
int doScan(AP results[], int maxResults);

#endif
