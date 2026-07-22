/*
 * DELIVERYBOT - ESP32 COMPLET
 * Affichage: Température, Humidité, Luminosité, Courant, Distance, Obstacle
 * Graphiques temps réel
 *
 * NOTE DE COHÉRENCE (ajoutée lors de la consolidation du dépôt) :
 * ------------------------------------------------------------------
 * Ce fichier provenait d'un dossier nommé "Interface_MQTT_Connection",
 * mais il n'utilise PAS le protocole MQTT : il monte son propre serveur
 * web local (WiFiServer sur le port 80) et sert une page HTML avec un
 * dashboard complet (cartes, radar, graphique Chart.js), rafraîchie par
 * polling JavaScript toutes les 500ms sur l'endpoint GET /data.
 *
 * dashboards/mqtt-streamlit/app.py, lui, attend de vraies données MQTT
 * publiées sur broker.hivemq.com/deliverybot/sensors — ce que CE firmware
 * ne fait jamais. Tant qu'aucune publication MQTT n'est ajoutée ici (voir
 * docs/known_inconsistencies.md pour un exemple d'ajout), le dashboard
 * MQTT restera à "En attente des données ESP32...".
 *
 * Pour utiliser ce firmware dès maintenant : ouvrir http://<IP_ESP32>
 * affiché au démarrage sur le moniteur série — pas besoin de dashboard
 * externe, tout est embarqué dans la page HTML servie ci-dessous.
 *
 * Autre point : en l'absence de données reçues de l'Arduino Due pendant
 * 3 secondes, ce firmware simule automatiquement des valeurs
 * (generateSimulationData()) pour que le dashboard reste animé. Utile en
 * démonstration, mais à garder en tête pour ne pas confondre données
 * réelles et simulées lors des tests.
 * ------------------------------------------------------------------
 */

#include <WiFi.h>
#include <ArduinoJson.h>

// ============================================================
// CONFIGURATION WIFI
// ============================================================
const char* ssid = "Redmi 10";
const char* password = "22222222";

// ============================================================
// PINS UART (Arduino Due)
// ============================================================
#define RX_PIN 16
#define TX_PIN 17

// ============================================================
// SERVEUR WEB
// ============================================================
WiFiServer server(80);

// ============================================================
// VARIABLES CAPTEURS
// ============================================================
float temperature = 0;
float humidity = 0;
int lux = 0;
int ldrRaw = 0;
float currentValue = 0;
float power = 0;
float soc = 100;
int distance = 0;
String obstacle = "AUCUN";
int radar[13] = {0};
int minDistance = 400;

unsigned long lastReceive = 0;
unsigned long lastDataUpdate = 0;

// ============================================================
// SIMULATION DE DONNÉES (en attendant Arduino)
// ============================================================
void generateSimulationData() {
  static float simTemp = 22.0;
  static float simHum = 55.0;
  static int simLux = 500;
  static float simCurrent = 0.5;
  static int simDist = 100;
  
  simTemp += (random(-10, 10) / 100.0);
  if (simTemp < 18) simTemp = 18;
  if (simTemp > 32) simTemp = 32;
  
  simHum += (random(-20, 20) / 100.0);
  if (simHum < 30) simHum = 30;
  if (simHum > 80) simHum = 80;
  
  simLux += random(-50, 50);
  if (simLux < 100) simLux = 100;
  if (simLux > 1000) simLux = 1000;
  
  simCurrent += (random(-10, 10) / 100.0);
  if (simCurrent < 0.1) simCurrent = 0.1;
  if (simCurrent > 2.0) simCurrent = 2.0;
  
  simDist += random(-20, 20);
  if (simDist < 20) simDist = 20;
  if (simDist > 400) simDist = 400;
  
  soc -= 0.01;
  if (soc < 0) soc = 100;
  
  temperature = simTemp;
  humidity = simHum;
  lux = simLux;
  ldrRaw = map(lux, 0, 1000, 0, 4095);
  currentValue = simCurrent;
  power = currentValue * 7.4;
  distance = simDist;
  minDistance = simDist;
  
  if (minDistance < 30) obstacle = "DEVANT";
  else if (minDistance < 60) obstacle = "PROCHE";
  else obstacle = "AUCUN";
  
  for (int i = 0; i < 13; i++) {
    radar[i] = distance + random(-30, 30);
    if (radar[i] < 10) radar[i] = 10;
    if (radar[i] > 400) radar[i] = 400;
  }
}

// ============================================================
// PAGE HTML COMPLÈTE
// ============================================================
String getHTMLPage() {
  
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=yes">
    <title>DeliveryBot - Dashboard Complet</title>
    <style>
        * {
            margin: 0;
            padding: 0;
            box-sizing: border-box;
        }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #f0f4f8 0%, #d9e2ec 100%);
            min-height: 100vh;
            padding: 20px;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
        }
        
        /* Header */
        .header {
            background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%);
            border-radius: 25px;
            padding: 25px 30px;
            margin-bottom: 25px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            flex-wrap: wrap;
            box-shadow: 0 10px 30px rgba(0,0,0,0.1);
        }
        
        .header h1 {
            color: white;
            font-size: 1.8rem;
            display: flex;
            align-items: center;
            gap: 10px;
        }
        
        .header p {
            color: rgba(255,255,255,0.9);
            font-size: 0.85rem;
            margin-top: 5px;
        }
        
        .status-badge {
            display: inline-flex;
            align-items: center;
            gap: 8px;
            background: rgba(255,255,255,0.2);
            padding: 8px 18px;
            border-radius: 50px;
            font-size: 0.85rem;
            color: white;
        }
        
        /* Cartes métriques */
        .metrics-grid {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 20px;
            margin-bottom: 25px;
        }
        
        .metric-card {
            background: white;
            border-radius: 20px;
            padding: 20px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.08);
            transition: all 0.3s ease;
            text-align: center;
        }
        
        .metric-card:hover {
            transform: translateY(-5px);
            box-shadow: 0 10px 25px rgba(0,0,0,0.15);
        }
        
        .metric-icon {
            font-size: 2rem;
            margin-bottom: 10px;
        }
        
        .metric-title {
            font-size: 0.75rem;
            color: #666;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        
        .metric-value {
            font-size: 2.2rem;
            font-weight: bold;
            margin: 10px 0;
            color: #1e3c72;
        }
        
        .metric-unit {
            font-size: 0.7rem;
            color: #999;
        }
        
        /* Cartes larges */
        .wide-card {
            background: white;
            border-radius: 20px;
            padding: 20px;
            margin-bottom: 25px;
            box-shadow: 0 4px 15px rgba(0,0,0,0.08);
        }
        
        .card-title {
            font-size: 1rem;
            color: #1e3c72;
            text-transform: uppercase;
            letter-spacing: 2px;
            margin-bottom: 20px;
            display: flex;
            align-items: center;
            gap: 10px;
            font-weight: bold;
        }
        
        /* Batterie */
        .battery-body {
            height: 35px;
            background: #e0e0e0;
            border-radius: 10px;
            overflow: hidden;
            margin-top: 10px;
        }
        
        .battery-fill {
            height: 100%;
            background: linear-gradient(90deg, #2ecc71, #f39c12);
            width: 0%;
            border-radius: 10px;
            transition: width 0.5s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            color: white;
            font-weight: bold;
            font-size: 0.8rem;
        }
        
        /* Obstacle alert */
        .obstacle-alert {
            padding: 15px;
            border-radius: 15px;
            margin-top: 10px;
            text-align: center;
            font-weight: bold;
        }
        
        .obstacle-danger {
            background: #fee2e2;
            color: #e74c3c;
            border-left: 4px solid #e74c3c;
        }
        
        .obstacle-warning {
            background: #fff3e0;
            color: #f39c12;
            border-left: 4px solid #f39c12;
        }
        
        .obstacle-safe {
            background: #e8f8f5;
            color: #2ecc71;
            border-left: 4px solid #2ecc71;
        }
        
        /* Radar */
        .radar-container {
            background: #f8f9fa;
            border-radius: 20px;
            padding: 20px;
            margin-top: 10px;
        }
        
        .radar-bars {
            display: flex;
            justify-content: space-around;
            align-items: flex-end;
            height: 200px;
            gap: 8px;
        }
        
        .radar-bar-wrapper {
            flex: 1;
            text-align: center;
        }
        
        .radar-bar {
            background: linear-gradient(180deg, #2a5298, #1e3c72);
            width: 100%;
            min-width: 25px;
            border-radius: 10px 10px 0 0;
            transition: height 0.3s ease;
            margin-bottom: 8px;
        }
        
        .radar-label {
            font-size: 0.7rem;
            color: #666;
        }
        
        /* Graphique */
        .chart-container {
            margin-top: 20px;
        }
        
        canvas {
            max-height: 350px;
            width: 100%;
        }
        
        /* Tableau */
        .data-table {
            width: 100%;
            border-collapse: collapse;
            font-size: 0.8rem;
        }
        
        .data-table th, .data-table td {
            padding: 12px 10px;
            text-align: left;
            border-bottom: 1px solid #e0e0e0;
        }
        
        .data-table th {
            color: #1e3c72;
            font-weight: 600;
            background: #f8f9fa;
        }
        
        .data-table tr:hover {
            background: #f8f9fa;
        }
        
        .footer {
            text-align: center;
            color: #888;
            font-size: 0.7rem;
            margin-top: 30px;
            padding-top: 20px;
            border-top: 1px solid #e0e0e0;
        }
        
        @keyframes pulse {
            0%, 100% { opacity: 1; transform: scale(1); }
            50% { opacity: 0.7; transform: scale(1.05); }
        }
        
        .pulse {
            animation: pulse 0.5s ease;
        }
        
        @media (max-width: 768px) {
            .metrics-grid {
                grid-template-columns: repeat(2, 1fr);
                gap: 15px;
            }
            .metric-value {
                font-size: 1.5rem;
            }
            .radar-bar {
                min-width: 15px;
            }
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <div>
                <h1>🤖 DELIVERYBOT</h1>
                <p>Arduino Due → ESP32 | Toutes les mesures temps réel</p>
            </div>
            <div class="status-badge" id="statusBadge">
                <span>🟢</span> <span id="statusText">CONNECTÉ</span>
            </div>
        </div>
        
        <!-- 6 cartes principales -->
        <div class="metrics-grid">
            <div class="metric-card">
                <div class="metric-icon">🌡️</div>
                <div class="metric-title">TEMPÉRATURE</div>
                <div class="metric-value" id="temp">---</div>
                <div class="metric-unit">°C</div>
            </div>
            
            <div class="metric-card">
                <div class="metric-icon">💧</div>
                <div class="metric-title">HUMIDITÉ</div>
                <div class="metric-value" id="hum">---</div>
                <div class="metric-unit">%</div>
            </div>
            
            <div class="metric-card">
                <div class="metric-icon">💡</div>
                <div class="metric-title">LUMINOSITÉ</div>
                <div class="metric-value" id="lux">---</div>
                <div class="metric-unit">lux</div>
            </div>
            
            <div class="metric-card">
                <div class="metric-icon">⚡</div>
                <div class="metric-title">COURANT</div>
                <div class="metric-value" id="current">---</div>
                <div class="metric-unit">A</div>
            </div>
            
            <div class="metric-card">
                <div class="metric-icon">📏</div>
                <div class="metric-title">DISTANCE MIN</div>
                <div class="metric-value" id="distance">---</div>
                <div class="metric-unit">cm</div>
            </div>
            
            <div class="metric-card">
                <div class="metric-icon">🔋</div>
                <div class="metric-title">BATTERIE</div>
                <div class="metric-value" id="soc">---</div>
                <div class="metric-unit">%</div>
                <div class="battery-body">
                    <div class="battery-fill" id="batteryFill"></div>
                </div>
            </div>
        </div>
        
        <!-- Alerte obstacle -->
        <div class="wide-card">
            <div class="card-title">🚨 DÉTECTION OBSTACLE</div>
            <div id="obstacleAlert" class="obstacle-alert obstacle-safe">
                ✅ AUCUN OBSTACLE DÉTECTÉ
            </div>
        </div>
        
        <!-- Radar -->
        <div class="wide-card">
            <div class="card-title">📡 RADAR - SCAN 30° À 150°</div>
            <div class="radar-container">
                <div class="radar-bars" id="radarBars"></div>
            </div>
        </div>
        
        <!-- Graphiques -->
        <div class="wide-card">
            <div class="card-title">📈 HISTORIQUE DES MESURES</div>
            <div class="chart-container">
                <canvas id="historyChart"></canvas>
            </div>
        </div>
        
        <!-- Tableau données -->
        <div class="wide-card">
            <div class="card-title">📋 DERNIÈRES MESURES</div>
            <div style="overflow-x: auto;">
                <table class="data-table">
                    <thead>
                        <tr><th>Temps</th><th>🌡️ Temp(°C)</th><th>💧 Hum(%)</th><th>💡 Lux</th><th>⚡ Courant(A)</th><th>📏 Dist(cm)</th><th>🔋 Batterie(%)</th><th>🚧 Obstacle</th></tr>
                    </thead>
                    <tbody id="tableBody">
                        <tr><td colspan="8">Chargement...</td></tr>
                    </tbody>
                </table>
            </div>
        </div>
        
        <div class="footer">
            <p>DeliveryBot - Projet Tutoré UE 17 | ENIG Gabès 2025-2026</p>
            <p>Mise à jour toutes les 500ms</p>
        </div>
    </div>
    
    <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
    <script>
        let historyData = [];
        let historyChart = null;
        let seconds = 0;
        
        function initChart() {
            const ctx = document.getElementById('historyChart').getContext('2d');
            historyChart = new Chart(ctx, {
                type: 'line',
                data: {
                    labels: [],
                    datasets: [
                        {
                            label: 'Température (°C)',
                            data: [],
                            borderColor: '#e74c3c',
                            backgroundColor: 'rgba(231,76,60,0.1)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.3,
                            pointRadius: 3,
                            yAxisID: 'y'
                        },
                        {
                            label: 'Humidité (%)',
                            data: [],
                            borderColor: '#3498db',
                            backgroundColor: 'rgba(52,152,219,0.1)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.3,
                            pointRadius: 3,
                            yAxisID: 'y'
                        },
                        {
                            label: 'Luminosité (lux)',
                            data: [],
                            borderColor: '#f39c12',
                            backgroundColor: 'rgba(243,156,18,0.1)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.3,
                            pointRadius: 3,
                            yAxisID: 'y1'
                        },
                        {
                            label: 'Batterie (%)',
                            data: [],
                            borderColor: '#2ecc71',
                            backgroundColor: 'rgba(46,204,113,0.1)',
                            borderWidth: 2,
                            fill: true,
                            tension: 0.3,
                            pointRadius: 3,
                            yAxisID: 'y'
                        }
                    ]
                },
                options: {
                    responsive: true,
                    maintainAspectRatio: true,
                    interaction: { mode: 'index', intersect: false },
                    plugins: {
                        legend: { position: 'top', labels: { color: '#333', font: { size: 11 } } },
                        tooltip: { mode: 'index', intersect: false }
                    },
                    scales: {
                        y: { 
                            title: { display: true, text: 'Température (°C) / Humidité (%) / Batterie (%)', color: '#666', font: { size: 10 } },
                            grid: { color: '#e0e0e0' },
                            min: 0,
                            max: 100
                        },
                        y1: { 
                            position: 'right',
                            title: { display: true, text: 'Luminosité (lux)', color: '#666', font: { size: 10 } },
                            grid: { drawOnChartArea: false },
                            min: 0,
                            max: 1000
                        },
                        x: { title: { display: true, text: 'Temps', color: '#666' }, grid: { color: '#e0e0e0' } }
                    }
                }
            });
        }
        
        function updateDashboard(data) {
            // Mise à jour des cartes
            document.getElementById('temp').innerHTML = data.temp;
            document.getElementById('hum').innerHTML = data.hum;
            document.getElementById('lux').innerHTML = data.lux;
            document.getElementById('current').innerHTML = data.current;
            document.getElementById('distance').innerHTML = data.distance;
            document.getElementById('soc').innerHTML = data.soc;
            
            // Batterie
            let socVal = parseFloat(data.soc);
            let fill = document.getElementById('batteryFill');
            fill.style.width = socVal + '%';
            fill.innerHTML = socVal + '%';
            fill.style.background = socVal < 20 ? '#e74c3c' : (socVal < 50 ? '#f39c12' : '#2ecc71');
            
            // Alerte obstacle
            let alertDiv = document.getElementById('obstacleAlert');
            let dist = parseInt(data.distance);
            if (dist < 30) {
                alertDiv.className = 'obstacle-alert obstacle-danger';
                alertDiv.innerHTML = '⚠️ OBSTACLE TRÈS PROCHE ! Distance: ' + dist + ' cm ⚠️';
            } else if (dist < 70) {
                alertDiv.className = 'obstacle-alert obstacle-warning';
                alertDiv.innerHTML = '⚡ ATTENTION - Obstacle à ' + dist + ' cm ⚡';
            } else {
                alertDiv.className = 'obstacle-alert obstacle-safe';
                alertDiv.innerHTML = '✅ VOIE LIBRE - Distance: ' + dist + ' cm ✅';
            }
            
            // Animation batterie
            document.getElementById('soc').classList.add('pulse');
            setTimeout(() => document.getElementById('soc').classList.remove('pulse'), 300);
            
            // Radar
            if (data.radar && data.radar.length > 0) {
                let radarHtml = '';
                let angles = [30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150];
                for (let i = 0; i < data.radar.length; i++) {
                    let dist = data.radar[i];
                    let height = Math.min((dist / 400) * 180, 180);
                    let color = dist < 30 ? '#e74c3c' : (dist < 100 ? '#f39c12' : '#2ecc71');
                    radarHtml += `
                        <div class="radar-bar-wrapper">
                            <div class="radar-bar" style="height: ${height}px; background: linear-gradient(180deg, ${color}, #1e3c72);"></div>
                            <div class="radar-label">${angles[i]}°</div>
                            <div class="radar-label" style="font-size:0.6rem;">${dist}cm</div>
                        </div>
                    `;
                }
                document.getElementById('radarBars').innerHTML = radarHtml;
            }
            
            // Historique graphique
            let now = new Date();
            let timeLabel = now.getHours().toString().padStart(2,'0') + ':' + now.getMinutes().toString().padStart(2,'0') + ':' + now.getSeconds().toString().padStart(2,'0');
            
            historyData.push({
                time: timeLabel,
                temp: parseFloat(data.temp),
                hum: parseFloat(data.hum),
                lux: parseInt(data.lux),
                soc: parseFloat(data.soc),
                current: parseFloat(data.current),
                distance: parseInt(data.distance)
            });
            
            if (historyData.length > 50) historyData.shift();
            
            if (historyChart) {
                historyChart.data.labels = historyData.map(d => d.time);
                historyChart.data.datasets[0].data = historyData.map(d => d.temp);
                historyChart.data.datasets[1].data = historyData.map(d => d.hum);
                historyChart.data.datasets[2].data = historyData.map(d => d.lux);
                historyChart.data.datasets[3].data = historyData.map(d => d.soc);
                historyChart.update();
            }
            
            // Tableau
            let tableHtml = '';
            let recentData = [...historyData].reverse().slice(0, 15);
            for (let d of recentData) {
                let obstacleText = d.distance < 30 ? '⚠️ PROCHE' : (d.distance < 70 ? '⚡ ATTENTION' : '✅ LIBRE');
                tableHtml += `<tr>
                    <td>${d.time}</td>
                    <td>${d.temp}</td>
                    <td>${d.hum}</td>
                    <td>${d.lux}</td>
                    <td>${d.current.toFixed(2)}</td>
                    <td>${d.distance}</td>
                    <td>${d.soc}</td>
                    <td>${obstacleText}</td>
                </tr>`;
            }
            document.getElementById('tableBody').innerHTML = tableHtml || '<tr><td colspan="8">Aucune donnée</td></tr>';
            
            seconds = 0;
        }
        
        function fetchData() {
            fetch('/data')
                .then(response => response.json())
                .then(data => {
                    updateDashboard(data);
                    document.getElementById('statusText').innerHTML = 'CONNECTÉ';
                    document.getElementById('statusText').style.color = '#2ecc71';
                })
                .catch(error => {
                    console.error('Erreur:', error);
                    document.getElementById('statusText').innerHTML = 'ATTENTE...';
                    document.getElementById('statusText').style.color = '#f39c12';
                });
        }
        
        setInterval(() => {
            seconds++;
            document.getElementById('lastUpdate').innerHTML = seconds + 's';
        }, 1000);
        
        window.onload = () => {
            initChart();
            setInterval(fetchData, 500);
            fetchData();
        };
    </script>
</body>
</html>
)rawliteral";
  
  return html;
}

// ============================================================
// DONNÉES JSON POUR AJAX
// ============================================================
String getJSONData() {
  String json = "{";
  json += "\"temp\":" + String(temperature, 1) + ",";
  json += "\"hum\":" + String(humidity, 1) + ",";
  json += "\"lux\":" + String(lux) + ",";
  json += "\"ldr\":" + String(ldrRaw) + ",";
  json += "\"current\":" + String(currentValue, 2) + ",";
  json += "\"power\":" + String(power, 1) + ",";
  json += "\"soc\":" + String(soc, 1) + ",";
  json += "\"distance\":" + String(distance) + ",";
  json += "\"obstacle\":\"" + obstacle + "\",";
  json += "\"radar\":[";
  for (int i = 0; i < 13; i++) {
    json += String(radar[i]);
    if (i < 12) json += ",";
  }
  json += "]}";
  return json;
}

// ============================================================
// TRAITEMENT DES DONNÉES UART (Arduino Due)
// ============================================================
void processUARTData(String jsonString) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (!error) {
    temperature = doc["temp"] | 22.0;
    humidity = doc["hum"] | 55.0;
    lux = doc["lux"] | 500;
    ldrRaw = doc["ldr"] | 2000;
    currentValue = doc["current"] | 0.5;
    power = doc["power"] | 3.7;
    soc = doc["soc"] | 100.0;
    distance = doc["distance"] | 100;
    obstacle = doc["obstacle"] | "AUCUN";
    
    JsonArray radarArray = doc["radar"];
    for (int i = 0; i < 13 && i < radarArray.size(); i++) {
      radar[i] = radarArray[i] | 100;
    }
    
    lastReceive = millis();
    Serial.println("✅ Données reçues de l'Arduino");
  } else {
    Serial.print("❌ Erreur JSON: ");
    Serial.println(error.c_str());
    // Utiliser la simulation en cas d'erreur
    generateSimulationData();
  }
}

// ============================================================
// SETUP
// ============================================================
void setup() {
  Serial2.begin(9600, SERIAL_8N1, RX_PIN, TX_PIN);
  Serial.begin(115200);
  
  Serial.println();
  Serial.println("╔════════════════════════════════════════════════════════════╗");
  Serial.println("║              DELIVERYBOT - ESP32 COMPLET                  ║");
  Serial.println("║     Température | Humidité | Luminosité | Courant         ║");
  Serial.println("║        Distance | Obstacle | Batterie                     ║");
  Serial.println("╚════════════════════════════════════════════════════════════╝");
  Serial.println();
  
  // Connexion WiFi
  Serial.print("📡 Connexion WiFi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi connecté !");
    Serial.print("📍 IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("❌ WiFi non connecté ! Vérifiez le mot de passe");
  }
  
  server.begin();
  Serial.println("🌐 Serveur web démarré");
  Serial.println();
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.print("🌐 OUVREZ VOTRE NAVIGATEUR: http://");
  Serial.println(WiFi.localIP());
  Serial.println("════════════════════════════════════════════════════════════");
  Serial.println();
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {
  // Lecture UART (Arduino Due)
  if (Serial2.available()) {
    String jsonString = Serial2.readStringUntil('\n');
    if (jsonString.length() > 0 && jsonString.indexOf("{") >= 0) {
      processUARTData(jsonString);
    }
  } else {
    // Si pas de données Arduino après 3 secondes, utiliser simulation
    static unsigned long lastSimulation = 0;
    if (millis() - lastSimulation > 3000) {
      lastSimulation = millis();
      generateSimulationData();
    }
  }
  
  // Serveur web
  WiFiClient client = server.available();
  
  if (client) {
    String request = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;
        if (c == '\n') break;
      }
    }
    
    if (request.indexOf("GET /data") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: application/json");
      client.println("Connection: close");
      client.println();
      client.println(getJSONData());
    } else {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");
      client.println();
      client.println(getHTMLPage());
    }
    
    delay(10);
    client.stop();
  }
  
  delay(10);
}