/*
 * DELIVERYBOT - ARDUINO DUE
 * Lecture de tous les capteurs + Envoi vers ESP32
 *
 * Capteurs:
 * - DHT11 (Température/Humidité)
 * - LDR (Luminosité)
 * - Capteur courant (LM358 + Shunt)
 * - HC-SR04 (Ultrason) + Servo SG90 (Radar)
 * - L298N (Moteurs)
 *
 * NOTE DE COHÉRENCE (ajoutée lors de la consolidation du dépôt) :
 * ------------------------------------------------------------------
 * Ceci est le firmware Arduino Due FINAL et le plus complet du projet :
 * il intègre tous les capteurs, le radar (servo + HC-SR04), la
 * navigation réactive et les moteurs (L298N), et transmet tout en JSON
 * vers l'ESP32 via Serial (UART, 9600 bauds).
 *
 * Point ouvert : les constantes de calibration du capteur de courant
 * ci-dessous (R_SHUNT, GAIN, V_OFFSET, V_REF) n'ont pas encore été
 * confirmées par une mesure au multimètre sur CE montage précis (pin A1,
 * alimentation 5V). Le test isolé
 * firmware/tests-isolated/arduino/CurrentSensor_OLED_test.ino utilise
 * des constantes différentes (pin A0, gain 20, 3.3V, pas d'offset) —
 * voir docs/known_inconsistencies.md avant de faire confiance aux
 * valeurs de courant/puissance/SoC affichées.
 *
 * Point ouvert 2 : le SoC (updateSOC()) est actuellement une simulation
 * de décharge temporelle (-0.5%/minute), pas une mesure réelle basée sur
 * le courant. À remplacer par un calcul de comptage de coulombs une fois
 * le capteur de courant calibré.
 * ------------------------------------------------------------------
 */

#include <DHT.h>
#include <Servo.h>
#include <ArduinoJson.h>

// ============================================================
// DÉFINITIONS DES PINS
// ============================================================

// L298N (Moteurs) - Jumpers gardés (100% vitesse)
#define IN1 8
#define IN2 9
#define IN3 10
#define IN4 11

// Capteurs
#define DHTPIN 22
#define DHTTYPE DHT11
#define LDR_PIN A0
#define CURRENT_PIN A1
#define TRIG_PIN 23
#define ECHO_PIN 24
#define SERVO_PIN 21

// ============================================================
// PARAMÈTRES RADAR LENT
// ============================================================
#define ANGLE_MIN 30
#define ANGLE_MAX 150
#define ANGLE_STEP 10
#define SERVO_DELAY 80
#define NB_POSITIONS (((ANGLE_MAX - ANGLE_MIN) / ANGLE_STEP) + 1)

// ============================================================
// PARAMÈTRES CAPTEUR COURANT
// ============================================================
#define R_SHUNT 0.1
#define GAIN 1.0
#define V_OFFSET 2.5
#define ADC_MAX 4095.0
#define V_REF 5.0

// ============================================================
// VARIABLES GLOBALES
// ============================================================
DHT dht(DHTPIN, DHTTYPE);
Servo radarServo;

// Capteurs environnement
float temperature = 0;
float humidity = 0;
int ldrRaw = 0;
int lux = 0;

// Capteur courant
float currentValue = 0;
float power = 0;
float batteryVoltage = 7.4;

// Batterie
float soc = 100;

// Radar
int radarDistances[NB_POSITIONS];
int minDistance = 400;
String obstacleDirection = "AUCUN";

// Timers
unsigned long lastMeasure = 0;
unsigned long lastRadarScan = 0;
unsigned long lastSend = 0;
unsigned long lastSOCUpdate = 0;

// ============================================================
// SETUP
// ============================================================
void setup() {
  // Série pour ESP32 (UART)
  Serial.begin(9600);
  
  // Série pour debug (moniteur série)
  Serial1.begin(9600);
  
  Serial1.println();
  Serial1.println("╔════════════════════════════════════════════════════════════╗");
  Serial1.println("║           DELIVERYBOT - ARDUINO DUE                       ║");
  Serial1.println("║     Tous capteurs actifs - Envoi vers ESP32               ║");
  Serial1.println("╚════════════════════════════════════════════════════════════╝");
  Serial1.println();
  
  // ========== INITIALISATION DES CAPTEURS ==========
  
  Serial1.println("🔧 Initialisation des capteurs...");
  
  // DHT11
  dht.begin();
  Serial1.println("   ✅ DHT11 (Température/Humidité) - Pin 22");
  
  // Servo Radar
  radarServo.attach(SERVO_PIN);
  radarServo.write(90);
  Serial1.println("   ✅ Servo SG90 (Radar) - Pin 21");
  
  // Ultrason
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  Serial1.println("   ✅ HC-SR04 (Ultrason) - TRIG=23, ECHO=24");
  
  // LDR
  pinMode(LDR_PIN, INPUT);
  Serial1.println("   ✅ LDR (Luminosité) - Pin A0");
  
  // Capteur courant
  pinMode(CURRENT_PIN, INPUT);
  Serial1.println("   ✅ Capteur courant (LM358+Shunt) - Pin A1");
  
  // Moteurs L298N
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  stopMotors();
  Serial1.println("   ✅ L298N (Moteurs) - Pins 8,9,10,11");
  
  Serial1.println();
  Serial1.println("📡 Configuration radar:");
  Serial1.print("   Angle: ");
  Serial1.print(ANGLE_MIN);
  Serial1.print("° à ");
  Serial1.print(ANGLE_MAX);
  Serial1.print("° | Pas: ");
  Serial1.print(ANGLE_STEP);
  Serial1.print("° | Positions: ");
  Serial1.print(NB_POSITIONS);
  Serial1.print(" | Délai: ");
  Serial1.print(SERVO_DELAY);
  Serial1.println("ms");
  Serial1.println();
  
  Serial1.println("✅ Tous les capteurs sont prêts !");
  Serial1.println("========================================================\n");
  
  delay(2000);
}

// ============================================================
// LOOP PRINCIPAL
// ============================================================
void loop() {
  
  // 1. MESURE CAPTEURS ENVIRONNEMENT (toutes les 2 secondes)
  if (millis() - lastMeasure >= 2000) {
    lastMeasure = millis();
    
    readDHT11();
    readLDR();
    readCurrentSensor();
    updateSOC();
    
    printDebug();
  }
  
  // 2. SCAN RADAR (toutes les 2 secondes - plus stable)
  if (millis() - lastRadarScan >= 2500) {
    lastRadarScan = millis();
    performRadarScan();
  }
  
  // 3. ENVOI VERS ESP32 (toutes les 500ms)
  if (millis() - lastSend >= 500) {
    lastSend = millis();
    sendToESP32();
  }
  
  // 4. NAVIGATION (basée sur radar)
  reactiveNavigation();
  
  delay(50);
}

// ============================================================
// LECTURE DHT11 (Température/Humidité)
// ============================================================
void readDHT11() {
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();
  
  if (isnan(temperature) || isnan(humidity)) {
    temperature = 25.0;
    humidity = 55.0;
    Serial1.println("⚠️ DHT11: Lecture erronée, valeur par défaut");
  }
}

// ============================================================
// LECTURE LDR (Luminosité)
// ============================================================
void readLDR() {
  ldrRaw = analogRead(LDR_PIN);
  lux = map(ldrRaw, 0, 1023, 0, 1000);
}

// ============================================================
// LECTURE CAPTEUR COURANT (LM358 + Shunt)
// ============================================================
void readCurrentSensor() {
  float sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(CURRENT_PIN);
    delay(2);
  }
  
  int rawAvg = sum / 10;
  float voltage = (rawAvg / ADC_MAX) * V_REF;
  currentValue = (voltage - V_OFFSET) / (GAIN * R_SHUNT);
  
  if (currentValue < 0) currentValue = 0;
  if (currentValue > 5.0) currentValue = 5.0;
  
  power = currentValue * batteryVoltage;
}

// ============================================================
// MISE À JOUR BATTERIE (SoC)
// ============================================================
void updateSOC() {
  // Simulation de décharge (à remplacer par mesure réelle)
  if (millis() - lastSOCUpdate >= 60000) {
    lastSOCUpdate = millis();
    soc = max(0, soc - 0.5);
  }
  
  // Pour tester, garder SOC élevé
  if (soc < 10) soc = 10;
}

// ============================================================
// SCAN RADAR (Servo + Ultrason)
// ============================================================
void performRadarScan() {
  int index = 0;
  minDistance = 400;
  obstacleDirection = "AUCUN";
  
  Serial1.println("----------------------------------------");
  Serial1.println("📡 SCAN RADAR EN COURS...");
  
  // Balayage de ANGLE_MIN à ANGLE_MAX
  for (int angle = ANGLE_MIN; angle <= ANGLE_MAX; angle += ANGLE_STEP) {
    radarServo.write(angle);
    delay(SERVO_DELAY);
    
    int dist = measureDistance();
    radarDistances[index] = dist;
    
    Serial1.print("   Angle: ");
    Serial1.print(angle);
    Serial1.print("° → Distance: ");
    Serial1.print(dist);
    Serial1.println(" cm");
    
    if (dist < minDistance) {
      minDistance = dist;
      
      if (angle < 70) obstacleDirection = "GAUCHE";
      else if (angle > 110) obstacleDirection = "DROITE";
      else obstacleDirection = "DEVANT";
    }
    
    index++;
    delay(30);
  }
  
  // Retour à la position centrale
  radarServo.write(90);
  delay(100);
  
  Serial1.print("📊 Résultat: Obstacle à ");
  Serial1.print(minDistance);
  Serial1.print(" cm - Direction: ");
  Serial1.println(obstacleDirection);
  Serial1.println("----------------------------------------\n");
}

// ============================================================
// MESURE DISTANCE ULTRASON
// ============================================================
int measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  int dist = duration * 0.034 / 2;
  
  if (dist <= 0 || dist > 400) {
    dist = 400;
  }
  
  return dist;
}

// ============================================================
// COMMANDES MOTEURS (L298N)
// ============================================================
void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void turnLeft() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
}

void stopMotors() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

// ============================================================
// NAVIGATION RÉACTIVE (basée sur le radar)
// ============================================================
void reactiveNavigation() {
  if (soc < 20) {
    stopMotors();
    return;
  }
  
  if (minDistance < 30) {
    stopMotors();
    delay(100);
    
    if (obstacleDirection == "GAUCHE") {
      turnRight();
      delay(500);
    } else if (obstacleDirection == "DROITE") {
      turnLeft();
      delay(500);
    } else {
      backward();
      delay(300);
      turnRight();
      delay(300);
    }
  } else {
    forward();
  }
}

// ============================================================
// ENVOI VERS ESP32 (JSON)
// ============================================================
void sendToESP32() {
  // Création du document JSON
  StaticJsonDocument<1024> doc;
  
  // Données environnement
  doc["temp"] = temperature;
  doc["hum"] = humidity;
  doc["lux"] = lux;
  doc["ldr"] = ldrRaw;
  
  // Données électriques
  doc["current"] = currentValue;
  doc["power"] = power;
  doc["soc"] = soc;
  
  // Données radar
  doc["distance"] = minDistance;
  doc["obstacle"] = obstacleDirection;
  
  // Données radar (tableau)
  JsonArray radar = doc.createNestedArray("radar");
  for (int i = 0; i < NB_POSITIONS; i++) {
    radar.add(radarDistances[i]);
  }
  
  // Sérialisation et envoi
  serializeJson(doc, Serial);
  Serial.println();  // Fin de ligne
  
  // Debug
  Serial1.print("📤 Envoi ESP32: ");
  Serial1.print("T=");
  Serial1.print(temperature, 1);
  Serial1.print("°C | Lux=");
  Serial1.print(lux);
  Serial1.print(" | Dist=");
  Serial1.print(minDistance);
  Serial1.print("cm | SoC=");
  Serial1.print(soc, 1);
  Serial1.println("%");
}

// ============================================================
// AFFICHAGE DEBUG (Moniteur série)
// ============================================================
void printDebug() {
  Serial1.println("╔════════════════════════════════════════════════════════════╗");
  Serial1.println("║                    MESURES CAPTEURS                       ║");
  Serial1.println("╠════════════════════════════════════════════════════════════╣");
  
  Serial1.print("║ 🌡️ Température: ");
  Serial1.print(temperature, 1);
  Serial1.print("°C  |  💧 Humidité: ");
  Serial1.print(humidity, 1);
  Serial1.println("%             ║");
  
  Serial1.print("║ 💡 Luminosité: ");
  Serial1.print(lux);
  Serial1.print(" lux  |  📊 LDR brut: ");
  Serial1.print(ldrRaw);
  Serial1.println("                 ║");
  
  Serial1.print("║ ⚡ Courant: ");
  Serial1.print(currentValue, 2);
  Serial1.print(" A  |  ⚡ Puissance: ");
  Serial1.print(power, 1);
  Serial1.print(" W  |  🔋 Batterie: ");
  Serial1.print(soc, 1);
  Serial1.println("%      ║");
  
  Serial1.print("║ 📡 Distance min: ");
  Serial1.print(minDistance);
  Serial1.print(" cm  |  🚧 Obstacle: ");
  Serial1.print(obstacleDirection);
  
  int spaces = 20 - obstacleDirection.length();
  for (int i = 0; i < spaces; i++) Serial1.print(" ");
  Serial1.println("║");
  
  Serial1.println("╚════════════════════════════════════════════════════════════╝");
  Serial1.println();
}