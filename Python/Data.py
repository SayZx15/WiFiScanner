import serial
import json
import sqlite3
import time
import os

# --- 1. Connexion au port série ---
ser = serial.Serial("/dev/cu.usbserial-0001", 115200)

db_path = "Data/wifi_data.db"
db_path = os.path.abspath(db_path)


os.makedirs(os.path.dirname(db_path), exist_ok=True)

# --- 2. Connexion à la base SQLite ---
conn = sqlite3.connect(db_path)
cur = conn.cursor()

# Définitions des mots clés et préfixes pour filtrer les AP fixes vs mobiles

mobile_keywords = [
    "android", "iphone", "galaxy", "mobile", "hotspot",
    "mifi", "ipad", "wifi-direct", "guest"
]

trusted_prefixes = [
    "tp-link", "netgear", "linksys", "asus", "dlink", "zyxel","Eduroam","eduroam"
    "xfinity", "comcast", "cisco", "belkin", "tplink",
    "huawei", "zte", "sagem", "sfr-", "neuf-", "bouygues",      
    "freebox", "orange-", "livebox", "bbox"
]

def is_fixed_ap(ssid, bssid):
    # Vérifie que le BSSID est valide
    if not bssid or bssid.lower() == "unknown":
        return False

    try:
        # 1️⃣ Vérifie le deuxième bit du premier octet du BSSID
        first_byte = int(bssid.split(":")[0], 16)
        if (first_byte & 0b10) != 0:  # si 2ème bit = 1 → adresse locale → ignore
            return False
    except ValueError:
        # Cas improbable où le BSSID n'est pas hex
        return False

    ssid_lower = ssid.lower() if ssid else ""
    # 2️⃣ Vérifie mots clés mobiles
    for kw in mobile_keywords:
        if kw in ssid_lower:
            return False

    # 3️⃣ Vérifie préfixes de routeurs connus
    for prefix in trusted_prefixes:
        if ssid_lower.startswith(prefix):
            return True

    # Si SSID non vide et pas clairement mobile, on le considère fixe
    if ssid and not any(kw in ssid_lower for kw in mobile_keywords):
        return True

    return False


# --- 3. Création de la table si elle n’existe pas ---
cur.execute("""
CREATE TABLE IF NOT EXISTS wifi_scans (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER,
    location TEXT,
    ssid TEXT,
    bssid TEXT UNIQUE,
    rssi INTEGER,
    channel INTEGER
)
""")
conn.commit()

print("Base SQLite prête. Attente des données...")

# --- 4. Boucle de lecture des données JSON envoyées par l’ESP32 ---
while True:
    line = ser.readline().decode(errors="ignore").strip()

    try:
        # JSON en provenance de l’ESP32
        data = json.loads(line)

        # Si l’ESP32 n’envoie pas de localisation, on met "unknown"
        location = data.get("location", "unknown")

        # Timestamp UNIX automatique si non fourni
        ssid = data.get("ssid", "unknown")
        bssid = data.get("bssid", "unknown")
        rssi = data.get("rssi", 0)
        channel = data.get("channel", 0)
        location = data.get("location", "unknown")
        timestamp = data.get("timestamp", int(time.time()))

        timestamp = data.get("timestamp", int(time.time()))

        # --- 5. Insertion dans SQLite ---
        if not filter(ssid, bssid):
            cur.execute("""
                INSERT INTO wifi_scans (timestamp, location, ssid, bssid, rssi, channel)
                VALUES (?, ?, ?, ?, ?, ?)
                ON CONFLICT(bssid) DO UPDATE SET
                    timestamp=excluded.timestamp,
                    location=excluded.location,
                    ssid=excluded.ssid,
                    rssi=excluded.rssi,
                    channel=excluded.channel
            """, (timestamp, location, ssid, bssid, rssi, channel))

            conn.commit()

            print("Sauvegardé :", data)
        else:
            print("Ignoré (mobile/fake) :", data)

    except json.JSONDecodeError:
        # Ignore les lignes non-JSON de l'ESP32 (ex : messages de debug)
        pass

