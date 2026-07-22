/*
 * NOTE DE COHÉRENCE (ajoutée lors de la consolidation du dépôt) :
 * ------------------------------------------------------------------
 * Ce test isolé du capteur de courant utilise des constantes DIFFÉRENTES
 * de celles employées dans le firmware final firmware/arduino-due/corps_test.ino :
 *
 *   Ce fichier (test OLED)      corps_test.ino (firmware final)
 *   ------------------------    -------------------------------
 *   Pin capteur : A0            Pin capteur : A1
 *   Vref        : 3.3 V         Vref        : 5.0 V
 *   Gain ampli  : 20.0          Gain ampli  : 1.0
 *   V_OFFSET    : non utilisé   V_OFFSET    : 2.5 V
 *
 * Ces deux jeux de constantes ne peuvent pas être corrects simultanément
 * pour le même capteur physique. Avant d'utiliser corps_test.ino pour une
 * mesure de courant fiable, il faut reprendre le protocole de calibration
 * de CE fichier (déconnecter toute charge, relever la tension de sortie
 * réelle au multimètre sur la broche A1 utilisée par corps_test.ino, avec
 * l'alimentation 5V réellement utilisée) et mettre à jour V_OFFSET/GAIN
 * dans corps_test.ino en conséquence. Voir docs/known_inconsistencies.md.
 * ------------------------------------------------------------------
 */

#include <aWire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// === PARAMÈTRES ===
const int capteur = A0;
const float Vref = 3.3;   // ⚠️ Arduino Due = 3.3V
const float Rshunt = 0.1; // résistance de mesure
const float gain   = 20.0; // gain de l’ampli

void setup() {
  Serial.begin(115200); // vitesse plus rapide sur Due

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Erreur OLED");
    while (1);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
}

void loop() {
  // === FILTRAGE (moyenne) ===
  float somme = 0;
  for (int i = 0; i < 20; i++) {
    somme += analogRead(capteur);
    delay(2);
  }
  float valeur = somme / 20.0;

  // === CALCUL ===
  float Vout = (valeur * Vref) / 4095.0; // ADC 12 bits
  float courant = (Vout / gain) / Rshunt;

  // === AFFICHAGE MONITEUR SÉRIE ===
  Serial.print("Tension = ");
  Serial.print(Vout, 3);
  Serial.print(" V   |   Courant = ");
  Serial.print(courant, 3);
  Serial.println(" A");

  // === AFFICHAGE OLED ===
  display.clearDisplay();

  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("MESURE COURANT");

  display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

  display.setCursor(0, 20);
  display.print("I =");

  display.setTextSize(2);
  display.setCursor(0, 35);
  display.print(courant, 3);
  display.print(" A");

  display.display();

  delay(200);
}
