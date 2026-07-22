"""
Test isolé — capteur ultrasonique HC-SR04 (ESP32 / MicroPython)

Version corrigée de test esp32et led.py :
  - broches alignées avec main.py (TRIG=GPIO5, ECHO=GPIO18) au lieu de
    TRIG=19/ECHO=23 utilisées dans le fichier original, pour éviter toute
    confusion de câblage entre les scripts ;
  - ajout d'un timeout : le script original pouvait bloquer indéfiniment
    ("while echo.value() == 0: pass") si aucun écho n'était reçu.

Usage : flasher seul sur l'ESP32 pour valider le capteur avant d'exécuter
main.py qui intègre l'ensemble des capteurs.
"""

from machine import Pin
import time

TRIG_PIN = 5
ECHO_PIN = 18
TIMEOUT_US = 30000

trig = Pin(TRIG_PIN, Pin.OUT)
echo = Pin(ECHO_PIN, Pin.IN)


def mesure_distance_cm():
    trig.value(0)
    time.sleep_us(2)
    trig.value(1)
    time.sleep_us(10)
    trig.value(0)

    depart = time.ticks_us()
    while echo.value() == 0:
        if time.ticks_diff(time.ticks_us(), depart) > TIMEOUT_US:
            return -1.0

    debut = time.ticks_us()
    while echo.value() == 1:
        if time.ticks_diff(time.ticks_us(), debut) > TIMEOUT_US:
            return -1.0
    fin = time.ticks_us()

    duree = time.ticks_diff(fin, debut)
    return (duree / 2) / 29.1


while True:
    d = mesure_distance_cm()
    if d < 0:
        print("Distance: hors de portee / pas d'echo")
    else:
        print("Distance:", round(d, 1), "cm")
    time.sleep(1)
