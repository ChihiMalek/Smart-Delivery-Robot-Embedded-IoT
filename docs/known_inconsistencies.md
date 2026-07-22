# Incohérences connues et résolution proposée

Ce document liste les contradictions réelles trouvées entre les fichiers
du projet, pour que rien ne soit appliqué aveuglément.

## 1. Deux formules différentes pour le capteur de courant

**Fichiers concernés** :
- `firmware/tests-isolated/arduino/CurrentSensor_OLED_test.ino` (test isolé)
- `firmware/arduino-due/corps_test.ino` (firmware final, fonction `readCurrentSensor()`)

| Constante | Test OLED | Firmware final |
|---|---|---|
| Broche | A0 | A1 |
| Vref | 3.3 V (Due) | 5.0 V |
| Gain ampli | 20.0 | 1.0 |
| V_OFFSET | non utilisé (formule sans offset) | 2.5 V |

Ces deux configurations ne peuvent pas être justes simultanément pour un
même capteur LM358+shunt. Deux hypothèses possibles :
- Soit le test OLED a été fait sur un premier montage (ampli x20 sur A0),
  puis le montage final a changé (branchement direct sans ampli sur A1,
  d'où gain=1) — auquel cas les constantes de `corps_test.ino` sont à
  vérifier isolément.
- Soit `corps_test.ino` a été écrit par anticipation sans re-tester sur
  le vrai capteur, en reprenant des valeurs génériques (V_REF=5V,
  V_OFFSET=2.5V « au milieu de l'échelle »).

**Résolution proposée (à faire sur banc, avec un multimètre) :**
1. Flasher `CurrentSensor_OLED_test.ino` tel quel une première fois pour
   confirmer qu'il fonctionne (retour visuel OLED).
2. Rebrancher le capteur de courant exactement comme dans
   `corps_test.ino` (pin A1, alimentation 5V, sans ampli x20 si c'est
   bien le montage réel).
3. Sans aucune charge branchée (0 A), relever la tension de sortie réelle
   du capteur au multimètre sur A1 → c'est le vrai `V_OFFSET`.
4. Appliquer un courant connu (ampèremètre en série) et calculer le gain
   réel : `gain_reel = (V_mesure - V_OFFSET) / (I_connu * R_SHUNT)`.
5. Mettre à jour `GAIN` et `V_OFFSET` dans `corps_test.ino` avec ces
   valeurs mesurées.

Tant que cette calibration n'est pas faite, les valeurs de `current`,
`power` et `soc` transmises par `corps_test.ino` doivent être considérées
comme indicatives, pas fiables.

## 2. MQTT annoncé mais jamais publié

**Fichiers concernés** :
- `dashboards/mqtt-streamlit/app.py` (attend du MQTT sur `broker.hivemq.com`, topic `deliverybot/sensors`)
- `dashboards/mqtt-web/index.html` (même attente, via WebSocket)
- `firmware/esp32-webserver/esp32_webserver.ino` (ne publie jamais en MQTT — sert un serveur HTTP local à la place)

Le dossier d'origine du firmware ESP32 s'appelait
`Interface_MQTT_Connection`, ce qui laissait penser qu'une connexion MQTT
existait déjà — ce n'est pas le cas dans le code actuel.

**Deux résolutions possibles, au choix :**

**Option 1 — Ignorer les dashboards MQTT pour l'instant.** Le dashboard
HTML embarqué dans `esp32_webserver.ino` fonctionne déjà en autonomie
(pas besoin de MQTT). Les deux dashboards MQTT restent disponibles pour
plus tard.

**Option 2 — Ajouter la publication MQTT au firmware ESP32.** Cela
demande d'ajouter la bibliothèque `PubSubClient` et un appel de
publication dans `esp32_webserver.ino`, par exemple juste après
`processUARTData()` :

```cpp
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient mqttClient(espClient);
// dans setup() : mqttClient.setServer("broker.hivemq.com", 1883);
// dans loop(), après processUARTData(jsonString) :
if (!mqttClient.connected()) {
  mqttClient.connect("deliverybot-esp32");
}
mqttClient.publish("deliverybot/sensors", jsonString.c_str());
```

Cette option n'a pas été appliquée directement dans le firmware fourni
ici pour éviter de modifier un fichier fonctionnel sans validation sur
le vrai matériel — à intégrer et tester quand ce sera souhaité.

## 3. SoC simulé, pas mesuré

**Fichier concerné** : `firmware/arduino-due/corps_test.ino`, fonction `updateSOC()`.

Le SoC actuel décroît artificiellement de 0,5 %/minute, indépendamment
de la consommation réelle. C'est un espace réservé (« pour tester,
garder SOC élevé »), pas un calcul basé sur le capteur de courant.

**Résolution proposée** : une fois le capteur de courant calibré (point
1), remplacer `updateSOC()` par un comptage de coulombs — intégrer
`currentValue` dans le temps pour estimer l'énergie consommée, puis la
soustraire de la capacité nominale du pack (1500 mAh, cf. rapport §2.4.2).

## 4. Deux jeux de champs JSON différents selon le firmware

Le firmware Piste A (MicroPython) envoie
`{"temp","hum","ldr","dist","led"}`. Le firmware Piste B (Arduino Due)
envoie `{"temp","hum","lux","ldr","current","power","soc","distance","obstacle","radar"}`.
Ce n'est pas une erreur — ce sont deux firmwares différents avec des
capacités différentes — mais un dashboard écrit pour l'un ne fonctionnera
pas tel quel avec l'autre. `dashboards/serial-streamlit/app.py` est
prévu pour la Piste A ; le dashboard de la Piste B est already intégré
dans `esp32_webserver.ino`.
