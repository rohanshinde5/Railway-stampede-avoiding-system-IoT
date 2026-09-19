# 🚆 Railway Station Crowd Monitoring & Stampede Prevention System

An IoT-based real-time railway station crowd density management system featuring directional entry/exit PIR detection, automated gate control (Servo), OLED visual displays, emergency alarm system, and a live web dashboard connected via MQTT telemetry.

---

## 🏗 System Architecture

```
                                  ┌───────────────────────┐
                                  │   Wokwi Simulator /   │
                                  │   Physical ESP32      │
                                  └───────────┬───────────┘
                                              │ Wi-Fi (Wokwi-GUEST)
                                              ▼
                                  ┌───────────────────────┐
                                  │   MQTT Broker         │
                                  │   test.mosquitto.org  │
                                  └───────────┬───────────┘
                                              │ MQTT (Port 1883)
                                              ▼
┌───────────────────────────────────────────────────────────────────────────┐
│                           Dashboard Server (Node.js)                      │
│                                                                           │
│   ┌─────────────────────┐    WebSocket    ┌───────────────────────────┐   │
│   │   MQTT Bridge       │ ──────────────> │   React Web Frontend      │   │
│   │   & REST API        │ <────────────── │   (Live Gauge & Control)  │   │
│   └─────────────────────┘   HTTP POST     └───────────────────────────┘   │
└───────────────────────────────────────────────────────────────────────────┘
```

---

## 📡 MQTT Topic Scheme

All MQTT topics use a configurable, unique topic namespace pattern to avoid collision with other public broker users:

```
railway/railsafe/<DEVICE_ID>/<subtopic>
```

| Subtopic | Direction | Description | Example Payload |
|---|---|---|---|
| `crowd/state` | ESP32 → Broker | Periodic & event-driven state update | `{"deviceId":"ESP32-RAILSAFE-01","crowdCount":5,"capacity":20,"occupancyPercent":25,"status":"NORMAL","emergency":false,"gateState":"OPEN","timestamp":123456}` |
| `crowd/event` | ESP32 → Broker | Real-time entry / exit event | `{"event":"ENTRY","crowdCount":6,"timestamp":123500}` |
| `system/heartbeat` | ESP32 → Broker | Heartbeat pulse (every 15 seconds) | `{"deviceId":"ESP32-RAILSAFE-01","status":"ONLINE","crowdCount":6,"uptime":120,"timestamp":123600}` |
| `system/command` | ESP32 → Broker | Confirmation of executed command | `{"event":"COMMAND_EXECUTED","command":"EMERGENCY","success":true,"timestamp":123700}` |
| `control/emergency` | Dashboard → ESP32 | Remote emergency trigger command | `1` |
| `control/reset` | Dashboard → ESP32 | Remote system reset command | `1` |

---

## ⚙️ Firmware Configuration (`src/config.h`)

The firmware defaults are stored in `src/config.h` for Wokwi development:

```cpp
#define WIFI_SSID     "Wokwi-GUEST"
#define WIFI_PASSWORD ""
#define MQTT_BROKER   "test.mosquitto.org"
#define MQTT_PORT     1883
#define DEVICE_ID     "ESP32-RAILSAFE-01"
#define USE_LEDS      1
```

> **Note:** To switch to a production broker (e.g., HiveMQ Cloud, AWS IoT, or private EMQX broker), update `src/config.h` or supply build flags in `platformio.ini`.

---

## 🔒 Environment & Security Configuration (`.env.example`)

Backend deployment settings are managed via `.env` (referenced in `.env.example`).

```env
MQTT_BROKER=test.mosquitto.org
MQTT_PORT=1883
DEVICE_ID=ESP32-RAILSAFE-01
PORT=4000
JWT_SECRET=CHANGE_THIS_TO_A_LONG_RANDOM_SECRET
```

> [!IMPORTANT]
> The actual `.env` file contains sensitive credentials and is ignored by `.gitignore`. Never commit `.env` or expose JWT secrets / MongoDB / MQTT credentials to the frontend.

---

## 🚀 Running the Project

### 1. Build & Run Firmware (ESP32)

```bash
# Compile firmware using PlatformIO CLI
pio run

# Upload to physical board (or load firmware.bin into Wokwi)
pio run --target upload
```

### 2. Launch Dashboard Server & Web UI

```bash
cd dashboard

# Install backend & frontend dependencies
npm install
cd frontend && npm install && npm run build && cd ..

# Start the dashboard server
npm start
```

Open your browser to: **`http://localhost:4000`**

---

## 🎯 Verification & Testing Checklist

- [x] **Firmware Build:** Compiled clean with zero errors via PlatformIO.
- [x] **Wi-Fi Integration:** Connects non-blockingly using `Wokwi-GUEST`.
- [x] **MQTT Integration:** Connects to `test.mosquitto.org:1883`, auto-reconnects, subscribes to control commands, publishes 15s heartbeats.
- [x] **Hardware Fallback:** LED physical indicators remain intact when `USE_LEDS 1`.
- [x] **Dashboard:** Node.js WebSocket bridge + React UI displaying live occupancy gauge, status tags, remote emergency/reset buttons, and telemetry logs.
