# Câblage — Piste B (Arduino Due + ESP32), version de référence

Extrait directement de `firmware/arduino-due/corps_test.ino` et
`firmware/esp32-webserver/esp32_webserver.ino`.

## Arduino Due

| Élément | Broche |
|---|---|
| DHT11 (température/humidité) | D22 |
| LDR (luminosité) | A0 |
| Capteur de courant (LM358+shunt) | A1 *(calibration à confirmer, voir known_inconsistencies.md)* |
| HC-SR04 TRIG | D23 |
| HC-SR04 ECHO | D24 |
| Servomoteur SG90 (radar) | D21 |
| L298N IN1 | D8 |
| L298N IN2 | D9 |
| L298N IN3 | D10 |
| L298N IN4 | D11 |
| UART vers ESP32 | `Serial` (TX0/RX0), 9600 bauds |
| Debug (moniteur série) | `Serial1` |

Radar : balayage 30° à 150° par pas de 10°, toutes les 2,5 secondes.
Vitesse moteurs : jumpers gardés = 100% (pas de contrôle PWM de vitesse
dans cette version, seulement marche/arrêt/sens).

## ESP32

| Élément | Broche |
|---|---|
| UART depuis Arduino Due (Serial2 RX) | GPIO16 |
| UART vers Arduino Due (Serial2 TX) | GPIO17 |
| Débit UART | 9600 bauds, format JSON, `\n` en fin de trame |

Le serveur web local écoute sur le port 80. Au démarrage, l'IP de l'ESP32
s'affiche sur le moniteur série (115200 bauds) — c'est l'adresse à
ouvrir dans un navigateur pour voir le dashboard.

## Alimentation

Comme documenté dans le rapport de projet (chapitre 2) : panneau solaire
3,5W → BMS 2S 8A → pack Li-ion 2S 7,4V → LM2596 (5V pour Arduino/ESP32) +
alimentation directe 7,4V pour les moteurs via L298N.

**Attention** : le L298N et le servo tirent un courant que ni l'Arduino
Due ni l'ESP32 ne peuvent fournir seuls — ils doivent être alimentés
directement par le pack batterie / régulateur, masses reliées en commun
uniquement.
