"""
Test isolé — moteurs DC via pont en H L298N (ESP32 / MicroPython)

MANQUAIT dans les scripts fournis : aucun test moteur n'était présent.
À VALIDER SUR BANC avant d'utiliser main_mqtt.py — vérifier notamment
le sens de rotation réel de chaque moteur (inverser IN1/IN2 si besoin).

Brochage (voir docs/wiring.md) :
  ENA (PWM gauche) -> GPIO25
  IN1               -> GPIO26
  IN2               -> GPIO27
  ENB (PWM droit)  -> GPIO14
  IN3               -> GPIO32
  IN4               -> GPIO33

Sécurité : lever les roues du sol avant le premier essai.
"""

from machine import Pin, PWM
import time

ENA_PIN, IN1_PIN, IN2_PIN = 25, 26, 27
ENB_PIN, IN3_PIN, IN4_PIN = 14, 32, 33

TEST_SPEED = 400  # duty sur 1023, ~40% — vitesse réduite pour le premier test

in1 = Pin(IN1_PIN, Pin.OUT)
in2 = Pin(IN2_PIN, Pin.OUT)
in3 = Pin(IN3_PIN, Pin.OUT)
in4 = Pin(IN4_PIN, Pin.OUT)

ena = PWM(Pin(ENA_PIN), freq=1000)
enb = PWM(Pin(ENB_PIN), freq=1000)


def stop():
    in1.value(0); in2.value(0)
    in3.value(0); in4.value(0)
    ena.duty(0); enb.duty(0)


def avancer(vitesse=TEST_SPEED):
    in1.value(1); in2.value(0)
    in3.value(1); in4.value(0)
    ena.duty(vitesse); enb.duty(vitesse)


def reculer(vitesse=TEST_SPEED):
    in1.value(0); in2.value(1)
    in3.value(0); in4.value(1)
    ena.duty(vitesse); enb.duty(vitesse)


def tourner_gauche(vitesse=TEST_SPEED):
    in1.value(0); in2.value(1)
    in3.value(1); in4.value(0)
    ena.duty(vitesse); enb.duty(vitesse)


def tourner_droite(vitesse=TEST_SPEED):
    in1.value(1); in2.value(0)
    in3.value(0); in4.value(1)
    ena.duty(vitesse); enb.duty(vitesse)


print("Test moteurs — sequence avance / recul / gauche / droite / stop")
sequence = [
    ("Avance", avancer, 2),
    ("Stop", stop, 1),
    ("Recul", reculer, 2),
    ("Stop", stop, 1),
    ("Rotation gauche", tourner_gauche, 1),
    ("Stop", stop, 1),
    ("Rotation droite", tourner_droite, 1),
    ("Stop", stop, 1),
]

for nom, action, duree in sequence:
    print(nom)
    action()
    time.sleep(duree)

stop()
print("Test termine.")
