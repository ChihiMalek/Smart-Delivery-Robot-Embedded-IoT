# DeliveryBot — Dépôt consolidé

Robot mobile autonome de livraison — Projet tutoré UE 17, ENIG Gabès,
2025–2026. Réalisé par Akkeri Farah, Chihi Malek et Harrathi Ilyes,
encadré par Dr. Naoui Mohammed.

## Lire en premier

Ce dépôt rassemble **tous** les essais matériels et logiciels menés sur
ce projet, à différentes étapes. Rien n'a été supprimé : chaque script,
chaque test, chaque dashboard est conservé et classé. Avant de foncer
dans le code, lire dans l'ordre :

1. **`docs/architecture-overview.md`** — explique les différentes pistes
   explorées (MicroPython seul vs Arduino Due + ESP32) et laquelle est
   aujourd'hui la référence.
2. **`docs/known_inconsistencies.md`** — les points de friction réels
   trouvés entre les fichiers (calibration du capteur de courant, MQTT
   annoncé mais non branché, SoC simulé) et comment les lever.
3. **`docs/wiring.md`** — câblage de la piste de référence actuelle.

## Structure du dépôt

```
deliverybot-coherent/
├── README.md                         Ce fichier
├── docs/
│   ├── architecture-overview.md      Toutes les pistes, laquelle est la référence
│   ├── known_inconsistencies.md      Contradictions trouvées + résolution
│   └── wiring.md                     Câblage de la piste de référence
├── firmware/
│   ├── arduino-due/
│   │   └── corps_test.ino            ★ RÉFÉRENCE — Due : capteurs + moteurs + radar + navigation
│   ├── esp32-webserver/
│   │   └── esp32_webserver.ino       ★ RÉFÉRENCE — ESP32 : UART + WiFi + dashboard HTML embarqué
│   ├── esp32-micropython-standalone/
│   │   └── main.py                   Piste antérieure : ESP32 seul, testé (DHT11+LDR+HC-SR04+LED)
│   ├── tests-isolated/
│   │   ├── arduino/CurrentSensor_OLED_test.ino    Test capteur de courant + OLED
│   │   └── micropython/              Tests unitaires de la piste MicroPython
│   └── archive-ia-draft/
│       └── main_mqtt_MICROPYTHON_DRAFT_superseded.py   Brouillon dépassé, conservé pour trace
├── dashboards/
│   ├── serial-streamlit/             Dashboard série (pour la piste MicroPython)
│   ├── mqtt-streamlit/               Dashboard MQTT (orphelin, voir known_inconsistencies.md §2)
│   └── mqtt-web/                     Dashboard MQTT web (même situation)
└── hardware/
    ├── proteus/CurrentSensor.pdsprj  Simulation ISIS du capteur de courant
    └── images/                       Photos et schémas composants
```

## Démarrage rapide (piste de référence)

1. Flasher `firmware/arduino-due/corps_test.ino` sur l'Arduino Due.
2. Flasher `firmware/esp32-webserver/esp32_webserver.ino` sur l'ESP32
   (renseigner `ssid`/`password` WiFi en haut du fichier).
3. Câbler selon `docs/wiring.md`.
4. Relever l'adresse IP affichée par l'ESP32 sur le moniteur série (115200 bauds).
5. Ouvrir cette IP dans un navigateur — dashboard complet déjà intégré,
   aucune installation supplémentaire nécessaire.

Avant de faire confiance aux valeurs de courant/puissance/batterie
affichées, effectuer la calibration décrite dans
`docs/known_inconsistencies.md` §1.

## Ce qui reste à faire

- Calibrer le capteur de courant sur le vrai montage (A1, 5V).
- Remplacer le SoC simulé par un calcul basé sur la mesure de courant.
- Si une supervision cloud est souhaitée : ajouter la publication MQTT
  au firmware ESP32 (exemple de code fourni dans
  `known_inconsistencies.md` §2) pour activer les dashboards
  `mqtt-streamlit` et `mqtt-web`, déjà prêts à l'emploi.
- Caractérisation réelle du panneau solaire et mesure d'autonomie
  (mesures physiques, cf. rapport chapitre 4).

## Licence et cadre

Projet académique réalisé dans le cadre de l'UE 17 (ENIG). Fourni à
titre pédagogique, sans garantie.
