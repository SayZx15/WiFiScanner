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

# --- 3. Création de la table si elle n’existe pas ---
cur.execute("""
CREATE TABLE IF NOT EXISTS wifi_scans (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp INTEGER,
    location TEXT,
    ssid TEXT,
    bssid TEXT,
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
        cur.execute("""
            INSERT INTO wifi_scans (timestamp, location, ssid, bssid, rssi, channel)
            VALUES (?, ?, ?, ?, ?, ?)
        """, (timestamp, location, ssid, bssid, rssi, channel))

        conn.commit()

        print("Sauvegardé :", data)

    except json.JSONDecodeError:
        # Ignore les lignes non-JSON de l'ESP32 (ex : messages de debug)
        pass

