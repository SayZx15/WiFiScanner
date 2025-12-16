import sqlite3
import os
import math
from fastapi import FastAPI, HTTPException
from fastapi.responses import HTMLResponse
from fastapi.staticfiles import StaticFiles

#  uvicorn main:app --reload

app = FastAPI()

# --- CONFIGURATION ---
DB_PATH = "/Users/johan/Documents/Cours/Polytech/S7/IoT/TP/ProjetGPS_WiFi/Data/loraWIFI.db" 

# --- FONCTION UTILITAIRE BDD ---
def get_db_connection():
    if not os.path.exists(DB_PATH):
        raise HTTPException(status_code=500, detail="Fichier Base de données introuvable. Vérifiez le chemin.")
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row 
    return conn

# --- FONCTION MATHS : RSSI -> DISTANCE ---
def rssi_to_distance(rssi):
    # A = RSSI à 1m (Environ -45dBm)
    # n = Indice d'atténuation (Environ 3.5 en intérieur)
    A = -45 
    n = 3.5 
    if rssi == 0: return 100.0 
    exponent = (A - rssi) / (10 * n)
    return 10 ** exponent

# --- API 1 : Les Scans bruts (Tableau) ---
@app.get("/api/scans")
async def read_scans(limit: int = 50):
    conn = get_db_connection()
    cursor = conn.cursor()
    cursor.execute("SELECT * FROM wifi_scans ORDER BY id DESC LIMIT ?", (limit,))
    rows = cursor.fetchall()
    conn.close()
    return rows

# --- API 2 : Les AP Connus (Pour afficher les points bleus sur la carte) ---
@app.get("/api/ap_connus")
async def read_known_aps():
    conn = get_db_connection()
    cursor = conn.cursor()
    try:
        cursor.execute("SELECT * FROM known_aps")
        rows = cursor.fetchall()
    except sqlite3.OperationalError:
        return [] 
    conn.close()
    return rows

# --- API 3 : CALCUL DE POSITION (Le cœur du projet) ---
@app.get("/api/position_estimee")
async def calculate_position():
    conn = get_db_connection()
    cursor = conn.cursor()
    
    # On récupère la moyenne des RSSI des 60 dernières secondes
    # UNIQUEMENT pour les bornes qu'on connait (JOIN known_aps)
    query = """
        SELECT s.mac_address, AVG(s.rssi) as avg_rssi, k.lat, k.lon, k.name
        FROM wifi_scans s
        JOIN known_aps k ON s.mac_address = k.mac_address
        WHERE s.scan_time > datetime('now', '-60 seconds')
        GROUP BY s.mac_address
    """
    try:
        cursor.execute(query)
        scans = cursor.fetchall()
    except Exception as e:
        print("Erreur SQL:", e)
        return {"error": str(e)}
        
    conn.close()

    if not scans:
        return {"error": "Pas de données récentes ou bornes inconnues"}

    sum_weights = 0
    weighted_lat = 0
    weighted_lon = 0
    details = []

    for row in scans:
        lat = row['lat']
        lon = row['lon']
        rssi = row['avg_rssi']
        
        # 1. Estimation Distance
        dist = rssi_to_distance(rssi)
        
        # 2. Calcul du Poids (Inverse du carré de la distance)
        # Plus c'est près, plus le poids est grand
        weight = 1 / (dist ** 2)
        
        weighted_lat += lat * weight
        weighted_lon += lon * weight
        sum_weights += weight
        
        details.append({
            "mac": row['mac_address'],
            "nom": row['name'],
            "rssi": round(rssi, 1),
            "dist_est": round(dist, 2)
        })

    # 3. Barycentre
    if sum_weights > 0:
        final_lat = weighted_lat / sum_weights
        final_lon = weighted_lon / sum_weights
        
        return {
            "latitude": final_lat,
            "longitude": final_lon,
            "ap_count": len(scans),
            "details": details
        }
    else:
        return {"error": "Calcul impossible"}

# --- ROUTE HTML ---
@app.get("/", response_class=HTMLResponse)
async def read_root():
    with open("index.html", "r", encoding='utf-8') as f:
        return f.read()