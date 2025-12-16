#include <WiFi.h>
#include <HardwareSerial.h>

// ================= CONFIGURATION =================
#define LORA_RX 16     
#define LORA_TX 17     
#define LORA_BAUD 9600

#define APPEUI "0000000000000000"
#define APPKEY "3D0D6AA79FA5CD474E71E9A92D533BCC" 
#define DEVEUI "70B3D57ED0073262" 

// --- PARAMETRES ---
const int MAX_AP_TO_SEND = 3;   // Nombre max de réseaux à envoyer
const long sendInterval = 60000;       // Envoi toutes les 60s
const long joinRetryInterval = 15000;  // Tentative de connexion toutes les 15s

HardwareSerial LoRa(2);

// --- VARIABLES GLOBALES ---
long lastMsgTime = 0;
long lastJoinAttempt = 0;
bool isJoined = false;       
bool joinRequestSent = false; 

// ================= FONCTIONS UTILITAIRES =================

void sendCmd(String cmd) {
  Serial.println("CMD > " + cmd);
  LoRa.println(cmd);
}

void sendHexPayload(String hexData) {
  Serial.println("Envoi LoRa : " + hexData);
  sendCmd("AT+MSGHEX=" + hexData);
}

// --- FILTRE ANTI-HOTSPOT ---
// Retourne TRUE si c'est probablement un téléphone
// Retourne FALSE si c'est une Box/Routeur fixe
bool isLikelyHotspot(String ssid, uint8_t* bssid) {
  
  // 1. FILTRE MAC
  // Si le 2ème chiffre de la MAC est 2, 6, A ou E, c'est souvent une adresse aléatoire donc un mobile la plupart du temps
  uint8_t secondNibble = bssid[0] & 0x0F; 
  if (secondNibble == 0x2 || secondNibble == 0x6 || secondNibble == 0xA || secondNibble == 0xE) {
    return true;
  }

  // 2. FILTRE SSID (Noms communs de téléphones)
  String s = ssid;
  s.toUpperCase();
  if (s.indexOf("IPHONE") >= 0) return true;
  if (s.indexOf("ANDROID") >= 0) return true;
  if (s.indexOf("GALAXY") >= 0) return true;
  if (s.indexOf("HUAWEI") >= 0) return true;
  if (s.indexOf("POCO") >= 0) return true;
  
  return false; // C'est probablement une Box fixe
}

// ================= SETUP =================
void setup() {
  Serial.begin(115200);
  LoRa.begin(LORA_BAUD, SERIAL_8N1, LORA_RX, LORA_TX);
  
  // WiFi en mode Station pour scanner
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(); 

  delay(2000);
  Serial.println("\n=== SYSTEME PRET : FILTRE SMART + LORA ===");

  // Configuration LoRa
  sendCmd("AT"); 
  delay(500);
  sendCmd("AT+DR=EU868");    
  delay(500);
  sendCmd("AT+MODE=LWOTAA"); 
  delay(500);
  sendCmd("AT+CLASS=A");
  delay(500);
  sendCmd("AT+PORT=2");      
  delay(500);

  sendCmd("AT+ID=DevEui," + String(DEVEUI));
  delay(500);
  sendCmd("AT+ID=AppEui," + String(APPEUI));
  delay(500);
  sendCmd("AT+KEY=APPKEY," + String(APPKEY));
  delay(1000);
}

// ================= LOOP =================
void loop() {
  long now = millis();

  // ------------------------------------------------
  // 1. ÉCOUTE ET GESTION LORA
  // ------------------------------------------------
  while (LoRa.available()) {
    String line = LoRa.readStringUntil('\n');
    line.trim();
    
    if (line.length() > 0) {
      Serial.println("LORA RECU: " + line);

      // Succès de connexion
      if (line.indexOf("Network joined") >= 0 || line.indexOf("+JOIN: Success") >= 0) {
           isJoined = true;
           joinRequestSent = false;
           Serial.println(">>> SUCCES : CONNECTÉ AU RÉSEAU LoRaWAN !");
           lastMsgTime = now - sendInterval + 2000; // Force un scan rapide
      }
      
      // Échec ou besoin de reconnexion
      if (line.indexOf("Join failed") >= 0 || line.indexOf("Please join") >= 0) {
        Serial.println(">>> ECHEC / DECONNECTÉ. Nouvelle tentative bientôt...");
        isJoined = false;
        joinRequestSent = false;
        if (line.indexOf("Please join") >= 0) lastJoinAttempt = now - joinRetryInterval - 1000; 
        else lastJoinAttempt = now; 
      }
    }
  }

  // ------------------------------------------------
  // 2. LOGIQUE DE CONNEXION (SI NON CONNECTÉ)
  // ------------------------------------------------
  if (!isJoined) {
    if (!joinRequestSent && (now - lastJoinAttempt > joinRetryInterval)) {
        lastJoinAttempt = now;
        joinRequestSent = true;
        Serial.println("--- Tentative de connexion (AT+JOIN) ---");
        LoRa.println("AT+JOIN");
    }
  }

  // ------------------------------------------------
  // 3. SCAN WIFI ET ENVOI (SI CONNECTÉ)
  // ------------------------------------------------
  else { 
    if (now - lastMsgTime > sendInterval) {
      lastMsgTime = now;

      Serial.println("\n--- Scan WiFi en cours... ---");
      int n = WiFi.scanNetworks(false, true);
      
      if (n == 0) { Serial.println("Aucun réseau."); return; }

      // --- Construction Payload ---
      uint8_t rawPayload[64];
      int idx = 0;

      // Header
      rawPayload[idx++] = 0x55; // Marqueur 'U'
      uint16_t ts = millis() / 1000;
      rawPayload[idx++] = (ts >> 8) & 0xFF;
      rawPayload[idx++] = (ts & 0xFF);
      
      // On sauvegarde la position de l'octet "Nombre d'AP" pour le remplir plus tard
      int countIndex = idx++; 
      int validAPCount = 0;

      // Boucle de filtrage et remplissage
      for (int i = 0; i < n; i++) {
          if (validAPCount >= MAX_AP_TO_SEND) break; // On arrête si on a assez de réseaux

          String ssid = WiFi.SSID(i);
          uint8_t* bssid = WiFi.BSSID(i);

          // Si c'est un téléphone, on ignore
          if (isLikelyHotspot(ssid, bssid)) {
            Serial.println("X Ignoré (Mobile): " + ssid);
            continue; 
          }

          // C'est un bon réseau
          String macStr = "";
          for (int k = 0; k < 6; k++) {
              if (bssid[k] < 16) macStr += "0"; // Ajoute un zéro si l'octet est < 16 (hex)
              macStr += String(bssid[k], HEX);
              if (k < 5) macStr += ":";
          }
          macStr.toUpperCase();
          Serial.println("V Ajouté (Fixe): " + ssid + " [" + macStr + "] [" + String(WiFi.RSSI(i)) + "dBm]");
          
          // MAC (6 octets)
          for(int k=0; k<6; k++) rawPayload[idx++] = bssid[k];
          
          // RSSI & Channel
          rawPayload[idx++] = (uint8_t)WiFi.RSSI(i); 
          rawPayload[idx++] = (uint8_t)WiFi.channel(i);
          
          validAPCount++;
      }
      
      WiFi.scanDelete();

      // Mise à jour du nombre réel d'AP envoyés
      rawPayload[countIndex] = validAPCount;

      // Envoi LoRa seulement si on a trouvé des AP valides
      if (validAPCount > 0) {
        String hexPayload = "";
        for(int i=0; i<idx; i++) {
          if(rawPayload[i] < 16) hexPayload += "0";
          hexPayload += String(rawPayload[i], HEX);
        }
        hexPayload.toUpperCase();
        sendHexPayload(hexPayload);
      } else {
        Serial.println("Aucun réseau fixe valide trouvé après filtrage.");
      }
    }
  }
}