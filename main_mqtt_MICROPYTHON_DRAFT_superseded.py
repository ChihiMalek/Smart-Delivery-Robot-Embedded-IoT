"""
DeliveryBot — Firmware final integre (ESP32 / MicroPython)
--------------------------------------------------------------
Assemble l'ensemble des sous-systemes du rapport de projet : capteurs
(DHT11, LDR, HC-SR04+servo), moteurs (L298N), gestion d'energie (courant,
tension batterie, regimes SoC), et supervision (WiFi + MQTT).

STATUT : ce fichier N'A PAS ETE VALIDE SUR MATERIEL REEL. Il assemble des
briques individuellement testables (voir test_motors.py, test_servo.py,
test_current_voltage.py, test_wifi_mqtt.py, test_hcsr04.py) mais leur
integration complete doit etre verifiee pas a pas :
  1. Valider chaque test isole d'abord.
  2. Flasher ce fichier en laissant le robot immobilise (roues levees).
  3. Verifier les valeurs affichees en serie avant d'autoriser le
     mouvement reel (retirer le commentaire sur MOUVEMENT_AUTORISE).

Pour la version plus simple, deja testee (sans moteurs ni MQTT), voir
main.py.
"""

from machine import Pin, ADC, PWM
import network
import time
import dht

try:
    from umqtt.simple import MQTTClient
    MQTT_DISPONIBLE = True
except ImportError:
    MQTT_DISPONIBLE = False
    print("umqtt.simple absent : le mode MQTT sera desactive (voir test_wifi_mqtt.py).")

# ------------------------- Securite -------------------------
MOUVEMENT_AUTORISE = False  # passer a True uniquement apres validation banc

# ------------------------- Configuration WiFi / MQTT -------------------------
WIFI_SSID = "VOTRE_SSID"
WIFI_PASSWORD = "VOTRE_MOT_DE_PASSE"
MQTT_BROKER = "xxxxxxxx.s1.eu.hivemq.cloud"
MQTT_PORT = 1883
MQTT_USER = "deliverybot"
MQTT_PASSWORD = "VOTRE_MOT_DE_PASSE_MQTT"
MQTT_CLIENT_ID = "deliverybot-esp32"
TOPIC_SENSORS = b"deliverybot/sensors"
TOPIC_ALERTS = b"deliverybot/alerts"

# ------------------------- Brochage (cf. docs/wiring.md) -------------------------
DHT_PIN = 4
LDR_PIN = 34
TRIG_PIN, ECHO_PIN = 5, 18
LED_PIN = 2
SERVO_PIN = 13
ENA_PIN, IN1_PIN, IN2_PIN = 25, 26, 27
ENB_PIN, IN3_PIN, IN4_PIN = 14, 32, 33
CURRENT_PIN = 35
VBAT_PIN = 36

# ------------------------- Constantes (a etalonner, cf. test_current_voltage.py) -------------------------
CURRENT_VOFFSET = 1.65
CURRENT_SENSITIVITY = 0.100
VBAT_DIVIDER_RATIO = 2.545
VBAT_FULL = 8.4
VBAT_EMPTY = 6.0

SOC_NOMINAL_MIN = 50.0
SOC_DEGRADED_MIN = 20.0

OBSTACLE_STOP_CM = 15
LDR_DARK_THRESHOLD = 1000
SAMPLE_PERIOD_S = 2
ECHO_TIMEOUT_US = 30000

VITESSE_NOMINALE = 500   # duty 0-1023
VITESSE_DEGRADEE = 400   # -20% environ

# ------------------------- Initialisation matérielle -------------------------
capteur_dht = dht.DHT11(Pin(DHT_PIN))

ldr = ADC(Pin(LDR_PIN)); ldr.atten(ADC.ATTN_11DB); ldr.width(ADC.WIDTH_12BIT)
current_adc = ADC(Pin(CURRENT_PIN)); current_adc.atten(ADC.ATTN_11DB); current_adc.width(ADC.WIDTH_12BIT)
vbat_adc = ADC(Pin(VBAT_PIN)); vbat_adc.atten(ADC.ATTN_11DB); vbat_adc.width(ADC.WIDTH_12BIT)

trig = Pin(TRIG_PIN, Pin.OUT)
echo = Pin(ECHO_PIN, Pin.IN)
led = Pin(LED_PIN, Pin.OUT)
servo = PWM(Pin(SERVO_PIN), freq=50)

in1, in2 = Pin(IN1_PIN, Pin.OUT), Pin(IN2_PIN, Pin.OUT)
in3, in4 = Pin(IN3_PIN, Pin.OUT), Pin(IN4_PIN, Pin.OUT)
ena, enb = PWM(Pin(ENA_PIN), freq=1000), PWM(Pin(ENB_PIN), freq=1000)

mqtt_client = None


# ------------------------- Capteurs -------------------------

def lire_dht11():
    try:
        capteur_dht.measure()
        return capteur_dht.temperature(), capteur_dht.humidity()
    except OSError:
        return None, None


def mesure_distance_cm():
    trig.value(0); time.sleep_us(2)
    trig.value(1); time.sleep_us(10)
    trig.value(0)
    depart = time.ticks_us()
    while echo.value() == 0:
        if time.ticks_diff(time.ticks_us(), depart) > ECHO_TIMEOUT_US:
            return -1.0
    debut = time.ticks_us()
    while echo.value() == 1:
        if time.ticks_diff(time.ticks_us(), debut) > ECHO_TIMEOUT_US:
            return -1.0
    fin = time.ticks_us()
    return (time.ticks_diff(fin, debut) / 2) / 29.1


def lire_courant_a():
    raw = current_adc.read()
    v = (raw / 4095) * 3.3
    return (v - CURRENT_VOFFSET) / CURRENT_SENSITIVITY


def lire_soc_pct():
    raw = vbat_adc.read()
    vbat = (raw / 4095) * 3.3 * VBAT_DIVIDER_RATIO
    soc = (vbat - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100
    return max(0.0, min(100.0, soc))


def positionner_servo(angle):
    angle = max(0, min(180, angle))
    duty = int(40 + (angle / 180) * (115 - 40))
    servo.duty(duty)


# ------------------------- Moteurs -------------------------

def stop_moteurs():
    in1.value(0); in2.value(0); in3.value(0); in4.value(0)
    ena.duty(0); enb.duty(0)


def avancer(vitesse):
    in1.value(1); in2.value(0); in3.value(1); in4.value(0)
    ena.duty(vitesse); enb.duty(vitesse)


# ------------------------- Régime énergétique (cf. rapport §3.3.2) -------------------------

def calculer_regime(soc_pct):
    if soc_pct >= SOC_NOMINAL_MIN:
        return "NOMINAL"
    if soc_pct >= SOC_DEGRADED_MIN:
        return "DEGRADED"
    return "SURVIVAL"


def appliquer_comportement(regime, distance_cm):
    if not MOUVEMENT_AUTORISE:
        stop_moteurs()
        return

    if 0 <= distance_cm < OBSTACLE_STOP_CM:
        stop_moteurs()
        return

    if regime == "NOMINAL":
        avancer(VITESSE_NOMINALE)
    elif regime == "DEGRADED":
        avancer(VITESSE_DEGRADEE)
    else:  # SURVIVAL
        stop_moteurs()


# ------------------------- WiFi / MQTT -------------------------

def connecter_wifi(timeout_s=15):
    sta = network.WLAN(network.STA_IF)
    sta.active(True)
    if not sta.isconnected():
        sta.connect(WIFI_SSID, WIFI_PASSWORD)
        debut = time.time()
        while not sta.isconnected() and time.time() - debut < timeout_s:
            time.sleep(0.5)
    return sta.isconnected()


def connecter_mqtt():
    global mqtt_client
    if not MQTT_DISPONIBLE:
        return None
    try:
        client = MQTTClient(
            MQTT_CLIENT_ID, MQTT_BROKER, port=MQTT_PORT,
            user=MQTT_USER, password=MQTT_PASSWORD,
        )
        client.connect()
        mqtt_client = client
        return client
    except Exception as e:
        print("Echec connexion MQTT :", e)
        return None


def publier_telemetrie(temp, hum, ldr_raw, curr, soc, regime):
    temp_s = "null" if temp is None else str(temp)
    hum_s = "null" if hum is None else str(hum)
    payload = (
        '{"temp":%s,"hum":%s,"ldr":%d,"curr":%.2f,"soc":%.1f,"mot":"%s"}'
        % (temp_s, hum_s, ldr_raw, curr, soc, regime)
    ).encode()

    print(payload)  # toujours affiche en serie, meme sans WiFi (mode debug)

    if mqtt_client is not None:
        try:
            mqtt_client.publish(TOPIC_SENSORS, payload)
            if regime == "SURVIVAL":
                mqtt_client.publish(TOPIC_ALERTS, b'{"type":"SOC_CRITICAL"}')
        except Exception as e:
            print("Echec publication MQTT :", e)


# ------------------------- Programme principal -------------------------

def main():
    print("SYSTEM_START")
    print("MOUVEMENT_AUTORISE =", MOUVEMENT_AUTORISE)

    wifi_ok = connecter_wifi()
    print("WiFi connecte :", wifi_ok)
    if wifi_ok:
        connecter_mqtt()

    positionner_servo(90)
    stop_moteurs()

    while True:
        temp, hum = lire_dht11()
        ldr_raw = ldr.read()
        distance_cm = mesure_distance_cm()
        courant = lire_courant_a()
        soc = lire_soc_pct()
        regime = calculer_regime(soc)

        led.value(1 if (distance_cm >= 0 and distance_cm < OBSTACLE_STOP_CM) or ldr_raw < LDR_DARK_THRESHOLD else 0)

        appliquer_comportement(regime, distance_cm)
        publier_telemetrie(temp, hum, ldr_raw, courant, soc, regime)

        if mqtt_client is not None:
            try:
                mqtt_client.check_msg()
            except Exception:
                pass

        time.sleep(SAMPLE_PERIOD_S)


main()
