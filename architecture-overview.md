# Vue d'ensemble — toutes les pistes explorées

Ce projet a été développé en plusieurs vagues de tests. Ce document relie
tous ces efforts entre eux pour qu'aucun ne soit isolé ou perdu de vue.

## Chronologie des pistes

### Piste A — ESP32 seul, MicroPython (exploratoire, premiers tests)
`firmware/esp32-micropython-standalone/` + `firmware/tests-isolated/micropython/`

Premiers essais matériels : un ESP32 seul en MicroPython, sans Arduino
Due, communiquant par câble USB avec un dashboard Streamlit sur PC.

- **Réellement testés à l'origine** : DHT11, LDR, HC-SR04, LED.
- **Ajoutés ensuite par l'assistant IA, jamais validés sur le matériel** :
  moteurs L298N, servo SG90, capteur de courant + tension, WiFi/MQTT.
  Ces scripts (`test_motors.py`, `test_servo.py`,
  `test_current_voltage.py`, `test_wifi_mqtt.py`) restent utiles comme
  base de test isolé, mais leurs constantes sont à étalonner.
- Un firmware intégré complet avait été écrit à ce stade
  (`main_mqtt.py`) pour combler l'absence de code réel couvrant tous les
  sous-systèmes. **Ce fichier est maintenant dépassé** par la Piste B
  (voir ci-dessous) et déplacé dans `firmware/archive-ia-draft/` — il
  n'est plus la référence, mais il est conservé pour trace.

### Piste B — Arduino Due + ESP32, C++/Arduino (piste actuelle et la plus complète)
`firmware/arduino-due/corps_test.ino` + `firmware/esp32-webserver/esp32_webserver.ino`

C'est la version la plus aboutie du projet : un Arduino Due gère tous les
capteurs (DHT11, LDR, capteur de courant LM358+shunt, HC-SR04 sur servo
en radar) et les moteurs (L298N) avec navigation réactive, puis transmet
tout en JSON par liaison série à un ESP32. L'ESP32 se connecte au WiFi et
sert lui-même un dashboard HTML complet (cartes, radar, graphique) via
son propre serveur web local — sans dépendance à un broker MQTT externe.

**C'est cette piste qui doit être considérée comme la référence actuelle
du projet.** Deux points restent ouverts avant de lui faire confiance à
100% (détaillés dans `docs/known_inconsistencies.md`) :
1. Calibration du capteur de courant à confirmer.
2. SoC actuellement simulé par décrémentation temporelle, pas mesuré.

Un test isolé du capteur de courant existe séparément, avec écran OLED :
`firmware/tests-isolated/arduino/CurrentSensor_OLED_test.ino`, ainsi que
sa simulation Proteus : `hardware/proteus/CurrentSensor.pdsprj`.

### Dashboards — trois versions, à des stades différents

| Dashboard | Fichier | Statut | Se connecte à |
|---|---|---|---|
| Série (PC ↔ USB) | `dashboards/serial-streamlit/app.py` | ✅ Fonctionnel | Piste A (ESP32 MicroPython seul) |
| Web local embarqué | *(intégré dans* `esp32_webserver.ino` *)* | ✅ Fonctionnel | Piste B (Arduino Due + ESP32) |
| MQTT / cloud (Streamlit) | `dashboards/mqtt-streamlit/app.py` | ⚠️ Écrit mais orphelin | Rien pour l'instant — aucun firmware ne publie sur MQTT |
| MQTT / cloud (HTML) | `dashboards/mqtt-web/index.html` | ⚠️ Écrit mais orphelin | Rien pour l'instant, même raison |

Les deux dashboards MQTT (Streamlit et HTML) ont été conçus pour une
architecture de supervision cloud, mais aucun firmware actuel ne publie
réellement sur un broker MQTT — le firmware ESP32 de la Piste B sert son
propre dashboard HTML en local à la place. Ils sont donc conservés
« prêts à l'emploi » pour le jour où une publication MQTT sera ajoutée au
firmware (voir la piste d'évolution dans `docs/known_inconsistencies.md`).

## Que faire selon ce qu'on veut démontrer

- **Démonstration rapide, autonome, sans PC** : flasher Piste B
  (`corps_test.ino` + `esp32_webserver.ino`), ouvrir l'IP de l'ESP32 dans
  un navigateur — dashboard complet inclus.
- **Test d'un capteur isolé avant intégration** : utiliser
  `firmware/tests-isolated/`.
- **Supervision centralisée multi-robots / cloud** : il faudra d'abord
  ajouter la publication MQTT au firmware ESP32 (Piste B), voir
  `docs/known_inconsistencies.md`.
