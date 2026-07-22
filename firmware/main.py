"""
DeliveryBot — Firmware ESP32 (MicroPython)
--------------------------------------------
Version consolidée à partir des scripts de test individuels du projet
(CIRCUIT COMPLETE 3.py, TEST COMPLETE.py, TEST DHT11.py,
TEST CAPTEUR DE LUMIERE.py, test esp32et led.py).

Cette version corrige les incohérences trouvées entre les scripts de test :
  - un seul jeu de broches, sans conflit (voir tableau ci-dessous) ;
  - le capteur LM35 (buggé dans TEST ML355.py — formule Kelvin incorrecte)
    est remplacé par le DHT11, qui correspond au capteur réellement câblé
    sur le prototype (cf. photos du montage) ;
  - une seule sortie série, au format JSON (plus robuste à parser côté PC
    qu'un simple CSV positionnel) ;
  - un timeout sur la mesure HC-SR04 pour éviter un blocage si l'écho
    n'arrive jamais (absent de test esp32et led.py).

Brochage utilisé :
  DHT11 (data)     -> GPIO4
  LDR (ADC)        -> GPIO34
  HC-SR04 TRIG     -> GPIO5
  HC-SR04 ECHO     -> GPIO18
  LED interne      -> GPIO2

Aucun capteur de courant ni liaison MQTT/WiFi n'est inclus ici : seuls les
éléments effectivement testés dans les scripts fournis sont repris. Ce
firmware communique uniquement par USB série avec le dashboard Streamlit
(dossier ../dashboard).
"""

from machine import Pin, ADC
import dht
import time

# ------------------------- Configuration matérielle -------------------------
DHT_PIN   = 4
LDR_PIN   = 34
TRIG_PIN  = 5
ECHO_PIN  = 18
LED_PIN   = 2

SAMPLE_PERIOD_S = 2          # période d'échantillonnage
ECHO_TIMEOUT_US = 30000      # 30 ms ~ 5 m aller-retour, évite un blocage
LDR_DARK_THRESHOLD = 1000    # seuil "sombre" (valeur brute ADC 0-4095)
OBSTACLE_THRESHOLD_CM = 10   # seuil de proximité pour allumer la LED

# ------------------------- Initialisation -------------------------
capteur_dht = dht.DHT11(Pin(DHT_PIN))

ldr = ADC(Pin(LDR_PIN))
ldr.atten(ADC.ATTN_11DB)     # plage de mesure élargie jusqu'à ~3.3V
ldr.width(ADC.WIDTH_12BIT)   # résolution 12 bits (0-4095)

trig = Pin(TRIG_PIN, Pin.OUT)
echo = Pin(ECHO_PIN, Pin.IN)

led = Pin(LED_PIN, Pin.OUT)


def mesure_distance_cm():
    """Mesure de distance HC-SR04 avec double timeout anti-blocage."""
    trig.value(0)
    time.sleep_us(2)
    trig.value(1)
    time.sleep_us(10)
    trig.value(0)

    attente_debut = time.ticks_us()
    while echo.value() == 0:
        if time.ticks_diff(time.ticks_us(), attente_debut) > ECHO_TIMEOUT_US:
            return -1.0  # pas d'écho reçu (hors de portée ou capteur absent)

    debut = time.ticks_us()
    while echo.value() == 1:
        if time.ticks_diff(time.ticks_us(), debut) > ECHO_TIMEOUT_US:
            return -1.0

    fin = time.ticks_us()
    duree = time.ticks_diff(fin, debut)
    return (duree / 2) / 29.1  # cm


def lire_dht11():
    """Retourne (temp, hum) ou (None, None) si la lecture échoue."""
    try:
        capteur_dht.measure()
        return capteur_dht.temperature(), capteur_dht.humidity()
    except OSError:
        return None, None


def to_json(temp, hum, ldr_raw, dist_cm, led_on):
    # Construction manuelle : pas de module json embarqué garanti sur toutes
    # les builds MicroPython, et cela reste très rapide pour 5 champs.
    temp_s = "null" if temp is None else str(temp)
    hum_s  = "null" if hum is None else str(hum)
    return (
        '{"temp":%s,"hum":%s,"ldr":%d,"dist":%.1f,"led":%d}'
        % (temp_s, hum_s, ldr_raw, dist_cm, 1 if led_on else 0)
    )


print("SYSTEM_START")

while True:
    temp, hum = lire_dht11()
    ldr_raw = ldr.read()
    dist_cm = mesure_distance_cm()

    obstacle_proche = (dist_cm >= 0) and (dist_cm < OBSTACLE_THRESHOLD_CM)
    est_sombre = ldr_raw < LDR_DARK_THRESHOLD
    led_on = obstacle_proche or est_sombre
    led.value(1 if led_on else 0)

    print(to_json(temp, hum, ldr_raw, dist_cm, led_on))

    time.sleep(SAMPLE_PERIOD_S)
