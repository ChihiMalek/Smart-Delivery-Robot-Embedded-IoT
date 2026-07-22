"""
Test isolé — servomoteur SG90 (ESP32 / MicroPython)

MANQUAIT dans les scripts fournis : aucun test servo n'était présent,
alors qu'il est visible sur les photos du montage (orientation du HC-SR04).
À VALIDER SUR BANC : les constantes DUTY_MIN/DUTY_MAX ci-dessous sont des
valeurs typiques pour un SG90 sur ESP32 MicroPython (duty 0-1023, 50 Hz),
mais varient légèrement d'un servo à l'autre — ajuster si la course
0°-180° n'est pas atteinte proprement.

Brochage : signal SG90 -> GPIO13
Alimentation : dédiée recommandée (le SG90 peut consommer un pic de
courant que le régulateur de l'ESP32 ne supporte pas correctement).
"""

from machine import Pin, PWM
import time

SERVO_PIN = 13
DUTY_MIN = 40    # correspond a ~0 degre
DUTY_MAX = 115   # correspond a ~180 degres

servo = PWM(Pin(SERVO_PIN), freq=50)


def angle_vers_duty(angle):
    angle = max(0, min(180, angle))
    return int(DUTY_MIN + (angle / 180) * (DUTY_MAX - DUTY_MIN))


def positionner(angle):
    servo.duty(angle_vers_duty(angle))


print("Test servo — balayage 30 a 150 degres")
for angle in range(30, 151, 10):
    positionner(angle)
    time.sleep(0.15)

for angle in range(150, 29, -10):
    positionner(angle)
    time.sleep(0.15)

positionner(90)
print("Test termine, servo centre a 90 degres.")
