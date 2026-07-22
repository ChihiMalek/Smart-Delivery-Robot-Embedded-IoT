"""
DeliveryBot — Dashboard de supervision (lecture série)
--------------------------------------------------------
Version consolidée à partir de app.py, app3.py, interfacemethode3.py et
INTERFACE DE TEMP REEL/app.py.

Choix repris de app3.py (la version la plus aboutie de tes essais) :
  - contrôles Start/Stop et état conservés dans st.session_state ;
  - export CSV ;
  - lecture par petits paquets + st.rerun() pour rester réactif.

Corrections apportées par rapport aux scripts d'origine :
  - parsing JSON (`{"temp":...,"hum":...,"ldr":...,"dist":...,"led":...}`)
    au lieu d'un split CSV positionnel — plus robuste si l'ordre des
    champs change ou si un champ est "null" (lecture DHT11 manquée) ;
  - app.py (racine) mélangeait un serveur Flask et Streamlit dans le même
    process, ce qui relance un thread Flask à chaque rerun et finit par
    lever une erreur "port déjà utilisé" — non repris ici, la lecture se
    fait uniquement en série (USB), comme dans app3.py ;
  - interfacemethode3.py utilisait une boucle `while True` directement
    dans le script Streamlit, ce qui bloque l'interactivité (les boutons
    ne répondent plus) — remplacé par le motif start/stop + st.rerun().
"""

import streamlit as st
import pandas as pd
import serial
import json
import time
from datetime import datetime

st.set_page_config(page_title="DeliveryBot — Monitoring", layout="wide")
st.title("DeliveryBot — Monitoring capteurs (série)")

DEFAULT_PORT = "COM5"
DEFAULT_BAUD = 115200
MAX_ROWS = 200
COLUMNS = ["Temps", "Temp", "Hum", "LDR", "Distance", "LED"]

if "data" not in st.session_state:
    st.session_state.data = pd.DataFrame(columns=COLUMNS)
if "running" not in st.session_state:
    st.session_state.running = False
if "ser" not in st.session_state:
    st.session_state.ser = None
if "last_error" not in st.session_state:
    st.session_state.last_error = None

with st.sidebar:
    st.header("Connexion série")
    port_input = st.text_input("Port série", value=DEFAULT_PORT)
    baud_input = st.number_input("Baudrate", value=DEFAULT_BAUD, step=9600)

    col1, col2 = st.columns(2)
    with col1:
        start_btn = st.button("Démarrer", use_container_width=True)
    with col2:
        stop_btn = st.button("Arrêter", use_container_width=True)

    st.divider()
    if st.button("Effacer les données", use_container_width=True):
        st.session_state.data = pd.DataFrame(columns=COLUMNS)

    if not st.session_state.data.empty:
        csv = st.session_state.data.to_csv(index=False).encode("utf-8")
        st.download_button(
            label="Télécharger CSV",
            data=csv,
            file_name=f"deliverybot_data_{datetime.now().strftime('%Y%m%d_%H%M%S')}.csv",
            mime="text/csv",
            use_container_width=True,
        )

    st.divider()
    status = st.empty()
    if st.session_state.running:
        status.success("Lecture active")
    else:
        status.error("Arrêtée")

    if st.session_state.last_error:
        st.warning(st.session_state.last_error)

if start_btn and st.session_state.ser is None:
    try:
        st.session_state.ser = serial.Serial(port_input, int(baud_input), timeout=1)
        st.session_state.running = True
        st.session_state.last_error = None
    except Exception as e:
        st.session_state.last_error = f"Erreur ouverture port : {e}"

if stop_btn:
    st.session_state.running = False
    if st.session_state.ser:
        try:
            st.session_state.ser.close()
        except Exception:
            pass
        st.session_state.ser = None

col1, col2, col3, col4 = st.columns(4)
temp_metric = col1.empty()
hum_metric  = col2.empty()
ldr_metric  = col3.empty()
dist_metric = col4.empty()

if st.session_state.data.empty:
    temp_metric.metric("Température (°C)", "—")
    hum_metric.metric("Humidité (%)", "—")
    ldr_metric.metric("Luminosité (LDR)", "—")
    dist_metric.metric("Distance (cm)", "—")
else:
    last = st.session_state.data.iloc[-1]
    temp_metric.metric("Température (°C)", "—" if pd.isna(last["Temp"]) else f"{last['Temp']:.1f}")
    hum_metric.metric("Humidité (%)", "—" if pd.isna(last["Hum"]) else f"{last['Hum']:.1f}")
    ldr_metric.metric("Luminosité (LDR)", int(last["LDR"]))
    dist_metric.metric("Distance (cm)", f"{last['Distance']:.1f}")

st.subheader("Historique")
chart_area = st.empty()
table_area = st.empty()

if not st.session_state.data.empty:
    chart_data = st.session_state.data.set_index("Temps")[["Temp", "Hum", "LDR", "Distance"]]
    chart_area.line_chart(chart_data, use_container_width=True)
    table_area.dataframe(st.session_state.data.tail(10), use_container_width=True, hide_index=True)

if st.session_state.running and st.session_state.ser:
    lignes_lues = 0
    while lignes_lues < 5:
        try:
            raw = st.session_state.ser.readline()
            if not raw:
                break
            line = raw.decode("utf-8", errors="ignore").strip()
            if not line or line == "SYSTEM_START":
                break

            try:
                payload = json.loads(line)
            except json.JSONDecodeError:
                break  # ligne partielle ou bruit série, on l'ignore

            now = datetime.now().strftime("%H:%M:%S")
            new_row = pd.DataFrame([[
                now,
                payload.get("temp"),
                payload.get("hum"),
                payload.get("ldr"),
                payload.get("dist"),
                payload.get("led"),
            ]], columns=COLUMNS)

            st.session_state.data = pd.concat(
                [st.session_state.data, new_row], ignore_index=True
            ).tail(MAX_ROWS)

            st.session_state.data.to_csv("deliverybot_data.csv", index=False)
            lignes_lues += 1

        except Exception as e:
            st.session_state.last_error = f"Erreur lecture : {e}"
            st.session_state.running = False
            if st.session_state.ser:
                try:
                    st.session_state.ser.close()
                except Exception:
                    pass
                st.session_state.ser = None
            break

    time.sleep(1)
    st.rerun()
