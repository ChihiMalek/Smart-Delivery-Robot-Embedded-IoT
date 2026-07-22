"""
Test isolé — connexion WiFi et publication MQTT (ESP32 / MicroPython)

MANQUAIT dans les scripts fournis : aucun test WiFi ni MQTT n'était
present. Le rapport decrit une publication vers un broker HiveMQ Cloud,
jamais testee dans les scripts fournis.

Prerequis : la bibliotheque umqtt.simple n'est pas incluse par defaut
dans toutes les images MicroPython. L'installer avant d'executer ce
script, par exemple via mip (si l'ESP32 est deja connecte au WiFi) :

    import mip
    mip.install("umqtt.simple")

...ou en copiant manuellement le fichier umqtt/simple.py sur l'ESP32.

A VALIDER SUR BANC :
  - renseigner SSID/mot de passe WiFi ci-dessous ;
  - creer un cluster gratuit sur HiveMQ Cloud et renseigner ses
    identifiants (host, user, password) ;
  - ce test utilise une connexion NON chiffree (port 1883) pour rester
    simple sur un premier essai. Le port 8883 (TLS) necessite
    umqtt.robust ou une gestion manuelle de ussl, non couverte ici.
"""

import network
import time
from umqtt.simple import MQTTClient

WIFI_SSID = "VOTRE_SSID"
WIFI_PASSWORD = "VOTRE_MOT_DE_PASSE"

MQTT_BROKER = "xxxxxxxx.s1.eu.hivemq.cloud"
MQTT_PORT = 1883
MQTT_USER = "deliverybot"
MQTT_PASSWORD = "VOTRE_MOT_DE_PASSE_MQTT"
MQTT_CLIENT_ID = "deliverybot-esp32-test"
MQTT_TOPIC = b"deliverybot/test"


def connecter_wifi():
    sta = network.WLAN(network.STA_IF)
    sta.active(True)
    if not sta.isconnected():
        print("Connexion WiFi a", WIFI_SSID)
        sta.connect(WIFI_SSID, WIFI_PASSWORD)
        debut = time.time()
        while not sta.isconnected():
            if time.time() - debut > 15:
                print("Echec de connexion WiFi (timeout).")
                return False
            time.sleep(0.5)
    print("WiFi connecte, IP =", sta.ifconfig()[0])
    return True


def message_recu(topic, msg):
    print("Message recu sur", topic, ":", msg)


def main():
    if not connecter_wifi():
        return

    client = MQTTClient(
        MQTT_CLIENT_ID, MQTT_BROKER, port=MQTT_PORT,
        user=MQTT_USER, password=MQTT_PASSWORD,
    )
    client.set_callback(message_recu)
    client.connect()
    client.subscribe(MQTT_TOPIC)
    print("Connecte au broker MQTT, abonne a", MQTT_TOPIC)

    compteur = 0
    while True:
        client.check_msg()  # traite les messages entrants non bloquant
        payload = ('{"test":%d}' % compteur).encode()
        client.publish(MQTT_TOPIC, payload)
        print("Publie :", payload)
        compteur += 1
        time.sleep(2)


main()
