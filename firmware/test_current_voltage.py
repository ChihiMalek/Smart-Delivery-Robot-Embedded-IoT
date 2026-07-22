"""
Test isolé — capteur de courant (PCB custom) et tension batterie
(ESP32 / MicroPython)

MANQUAIT dans les scripts fournis : aucun test de courant ni de tension
batterie n'était présent, alors que le rapport (§2.5.3) mentionne un
capteur de courant développé sur PCB custom, et que le calcul du SoC
(§3.3.1) en dépend directement.

À VALIDER SUR BANC — obligatoire avant toute utilisation réelle :
  1. CURRENT_VOFFSET : brancher le circuit sans consommation (0 A) et
     relever la tension de sortie du capteur -> reporter ici.
  2. CURRENT_SENSITIVITY : appliquer un courant connu (multimètre en
     série) et calculer (V_mesuree - CURRENT_VOFFSET) / I_connu.
  3. VBAT_DIVIDER_RATIO : mesurer la tension réelle du pack au multimètre
     et la comparer à la tension lue sur GPIO36, ajuster le ratio.

Brochage :
  Capteur de courant (sortie analogique) -> GPIO35
  Diviseur de tension batterie            -> GPIO36
"""

from machine import Pin, ADC
import time

CURRENT_PIN = 35
VBAT_PIN = 36

# --- Constantes a etalonner sur banc (valeurs de depart uniquement) ---
CURRENT_VOFFSET = 1.65        # V, tension de sortie a 0 A
CURRENT_SENSITIVITY = 0.100   # V/A
VBAT_DIVIDER_RATIO = 2.545    # sans unite, Vbat_reel = Vlu * ratio
VBAT_FULL = 8.4               # V, pack 2S plein
VBAT_EMPTY = 6.0              # V, seuil de coupure BMS

current_adc = ADC(Pin(CURRENT_PIN))
current_adc.atten(ADC.ATTN_11DB)
current_adc.width(ADC.WIDTH_12BIT)

vbat_adc = ADC(Pin(VBAT_PIN))
vbat_adc.atten(ADC.ATTN_11DB)
vbat_adc.width(ADC.WIDTH_12BIT)


def lire_courant_a():
    raw = current_adc.read()
    v_mesure = (raw / 4095) * 3.3
    return (v_mesure - CURRENT_VOFFSET) / CURRENT_SENSITIVITY


def lire_tension_batterie():
    raw = vbat_adc.read()
    v_adc = (raw / 4095) * 3.3
    return v_adc * VBAT_DIVIDER_RATIO


def calculer_soc(vbat):
    soc = (vbat - VBAT_EMPTY) / (VBAT_FULL - VBAT_EMPTY) * 100
    return max(0.0, min(100.0, soc))


print("Test capteur de courant + tension batterie")
print("ATTENTION : constantes non etalonnees, valeurs indicatives uniquement.")

while True:
    courant = lire_courant_a()
    vbat = lire_tension_batterie()
    soc = calculer_soc(vbat)
    print("I=%.2fA  Vbat=%.2fV  SoC=%.1f%%" % (courant, vbat, soc))
    time.sleep(1)
