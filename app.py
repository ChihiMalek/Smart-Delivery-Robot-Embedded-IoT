import streamlit as st
import paho.mqtt.client as mqtt
import pandas as pd
import plotly.graph_objects as go
from datetime import datetime
import json
from collections import deque
import time

# ============================================================
# CONFIG
# ============================================================
st.set_page_config(
    page_title="DeliveryBot",
    page_icon="🤖",
    layout="wide"
)

BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC = "deliverybot/sensors"

# ============================================================
# SESSION STATE
# ============================================================
if "messages" not in st.session_state:
    st.session_state.messages = deque(maxlen=100)

if "mqtt_started" not in st.session_state:
    st.session_state.mqtt_started = False

# ============================================================
# MQTT CALLBACKS
# ============================================================
def on_connect(client, userdata, flags, rc):

    if rc == 0:
        print("MQTT CONNECTE")
        client.subscribe(TOPIC)
    else:
        print("Erreur MQTT :", rc)

def on_message(client, userdata, msg):

    try:

        data = json.loads(msg.payload.decode())

        data["timestamp"] = datetime.now()

        st.session_state.messages.append(data)

        print("MESSAGE :", data)

    except Exception as e:

        print("Erreur JSON :", e)

# ============================================================
# START MQTT
# ============================================================
if not st.session_state.mqtt_started:

    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION1
    )

    client.on_connect = on_connect
    client.on_message = on_message

    try:

        client.connect(BROKER, PORT, 60)

        client.loop_start()

        st.session_state.client = client
        st.session_state.mqtt_started = True

        print("Broker connecte")

    except Exception as e:

        st.error(f"Erreur MQTT : {e}")

# ============================================================
# STYLE
# ============================================================
st.markdown("""
<style>

.main-header{
    background: linear-gradient(135deg,#003366,#00AAFF);
    padding: 20px;
    border-radius: 20px;
    text-align:center;
    color:white;
}

.metric-card{
    background:#111827;
    padding:20px;
    border-radius:15px;
    text-align:center;
    color:white;
}

.metric{
    font-size:32px;
    font-weight:bold;
}

</style>
""", unsafe_allow_html=True)

# ============================================================
# HEADER
# ============================================================
st.markdown("""
<div class="main-header">
<h1>🤖 DELIVERYBOT</h1>
<p>ESP32 → MQTT → Streamlit</p>
</div>
""", unsafe_allow_html=True)

st.write("")

# ============================================================
# DONNEES
# ============================================================
if len(st.session_state.messages) > 0:

    last = st.session_state.messages[-1]

    temp = last.get("temp", 0)
    hum = last.get("hum", 0)
    lux = last.get("lux", 0)
    ldr = last.get("ldr", 0)
    soc = last.get("soc", 0)

    # ========================================================
    # METRICS
    # ========================================================
    col1, col2, col3, col4, col5 = st.columns(5)

    with col1:
        st.markdown(f"""
        <div class="metric-card">
        🌡️ Temperature
        <div class="metric">{temp}°C</div>
        </div>
        """, unsafe_allow_html=True)

    with col2:
        st.markdown(f"""
        <div class="metric-card">
        💧 Humidite
        <div class="metric">{hum}%</div>
        </div>
        """, unsafe_allow_html=True)

    with col3:
        st.markdown(f"""
        <div class="metric-card">
        💡 Lux
        <div class="metric">{lux}</div>
        </div>
        """, unsafe_allow_html=True)

    with col4:
        st.markdown(f"""
        <div class="metric-card">
        📊 LDR
        <div class="metric">{ldr}</div>
        </div>
        """, unsafe_allow_html=True)

    with col5:
        st.markdown(f"""
        <div class="metric-card">
        🔋 Batterie
        <div class="metric">{soc}%</div>
        </div>
        """, unsafe_allow_html=True)

    # ========================================================
    # GRAPHIQUE
    # ========================================================
    st.write("")
    st.subheader("Historique luminosite")

    df = pd.DataFrame(list(st.session_state.messages))

    fig = go.Figure()

    fig.add_trace(
        go.Scatter(
            x=df["timestamp"],
            y=df["lux"],
            mode="lines+markers",
            name="Lux"
        )
    )

    fig.update_layout(height=400)

    st.plotly_chart(fig, use_container_width=True)

    # ========================================================
    # TABLEAU
    # ========================================================
    with st.expander("Donnees detaillees"):

        display_df = df.copy()

        display_df["timestamp"] = display_df[
            "timestamp"
        ].dt.strftime("%H:%M:%S")

        st.dataframe(display_df.tail(20))

else:

    st.warning("En attente des donnees ESP32...")

# ============================================================
# AUTO REFRESH
# ============================================================
time.sleep(2)
st.rerun()