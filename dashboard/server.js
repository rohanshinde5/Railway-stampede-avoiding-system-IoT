// server.js - Railway Crowd Management Dashboard Backend
// MQTT bridge + WebSocket server + REST API
require('dotenv').config();
const express = require('express');
const cors = require('cors');
const { WebSocketServer } = require('ws');
const http = require('http');
const mqtt = require('mqtt');
const path = require('path');

// ── Configuration ─────────────────────────────────────────────
const MQTT_BROKER = process.env.MQTT_BROKER || 'test.mosquitto.org';
const MQTT_PORT   = parseInt(process.env.MQTT_PORT || '1883');
const DEVICE_ID   = process.env.DEVICE_ID || 'ESP32-RAILSAFE-01';
const PORT        = parseInt(process.env.PORT || '4000');
const MQTT_USER   = process.env.MQTT_USERNAME || undefined;
const MQTT_PASS   = process.env.MQTT_PASSWORD || undefined;

const TOPIC_PREFIX  = `railway/railsafe/${DEVICE_ID}`;
const TOPIC_STATE   = `${TOPIC_PREFIX}/crowd/state`;
const TOPIC_EVENT   = `${TOPIC_PREFIX}/crowd/event`;
const TOPIC_HBEAT   = `${TOPIC_PREFIX}/system/heartbeat`;
const TOPIC_COMMAND = `${TOPIC_PREFIX}/system/command`;
const TOPIC_CTRL_EMERGENCY = `${TOPIC_PREFIX}/control/emergency`;
const TOPIC_CTRL_RESET     = `${TOPIC_PREFIX}/control/reset`;

// ── In-memory state ───────────────────────────────────────────
let latestState = {
  deviceId: DEVICE_ID,
  crowdCount: 0,
  capacity: 20,
  occupancyPercent: 0,
  status: 'UNKNOWN',
  emergency: false,
  gateState: 'UNKNOWN',
  timestamp: 0,
  online: false,
  lastHeartbeat: null
};

// Rolling event log (last 100 events)
const eventLog = [];
function addEvent(evt) {
  eventLog.unshift(evt);
  if (eventLog.length > 100) eventLog.pop();
}

// ── Express app ───────────────────────────────────────────────
const app = express();
app.use(cors());
app.use(express.json());

// Serve React build (frontend)
app.use(express.static(path.join(__dirname, 'frontend', 'dist')));

// REST API endpoints
app.get('/api/state', (req, res) => res.json(latestState));
app.get('/api/events', (req, res) => res.json(eventLog));
app.get('/api/status', (req, res) => res.json({
  mqttConnected: mqttClient ? mqttClient.connected : false,
  broker: `${MQTT_BROKER}:${MQTT_PORT}`,
  deviceId: DEVICE_ID,
  topicPrefix: TOPIC_PREFIX
}));

// Control commands
app.post('/api/command/emergency', (req, res) => {
  mqttClient.publish(TOPIC_CTRL_EMERGENCY, '1');
  addEvent({ type: 'DASHBOARD_CMD', command: 'EMERGENCY', timestamp: Date.now() });
  broadcast({ type: 'command', command: 'EMERGENCY', timestamp: Date.now() });
  res.json({ ok: true, message: 'Emergency command sent' });
});

app.post('/api/command/reset', (req, res) => {
  mqttClient.publish(TOPIC_CTRL_RESET, '1');
  addEvent({ type: 'DASHBOARD_CMD', command: 'RESET', timestamp: Date.now() });
  broadcast({ type: 'command', command: 'RESET', timestamp: Date.now() });
  res.json({ ok: true, message: 'Reset command sent' });
});

// SPA fallback
app.get('*', (req, res) => {
  res.sendFile(path.join(__dirname, 'frontend', 'dist', 'index.html'));
});

// ── HTTP + WebSocket servers ──────────────────────────────────
const httpServer = http.createServer(app);
const wss = new WebSocketServer({ server: httpServer });

function broadcast(data) {
  const msg = JSON.stringify(data);
  wss.clients.forEach(ws => {
    if (ws.readyState === 1) ws.send(msg);
  });
}

wss.on('connection', (ws) => {
  console.log('[WS] Client connected');
  // Send current state immediately on connection
  ws.send(JSON.stringify({ type: 'state', data: latestState }));
  ws.send(JSON.stringify({ type: 'events', data: eventLog.slice(0, 20) }));
  ws.on('close', () => console.log('[WS] Client disconnected'));
});

// ── MQTT client ───────────────────────────────────────────────
const mqttOpts = {
  clientId: `dashboard-${Date.now()}`,
  clean: true,
  reconnectPeriod: 5000,
  connectTimeout: 10000,
};
if (MQTT_USER) mqttOpts.username = MQTT_USER;
if (MQTT_PASS) mqttOpts.password = MQTT_PASS;

const mqttClient = mqtt.connect(`mqtt://${MQTT_BROKER}:${MQTT_PORT}`, mqttOpts);

mqttClient.on('connect', () => {
  console.log(`[MQTT] Connected to ${MQTT_BROKER}:${MQTT_PORT}`);
  mqttClient.subscribe([TOPIC_STATE, TOPIC_EVENT, TOPIC_HBEAT, TOPIC_COMMAND], (err) => {
    if (err) console.error('[MQTT] Subscribe error:', err);
    else console.log('[MQTT] Subscribed to:', TOPIC_PREFIX + '/#');
  });
  broadcast({ type: 'mqtt_status', connected: true });
});

mqttClient.on('offline', () => {
  console.log('[MQTT] Disconnected');
  latestState.online = false;
  broadcast({ type: 'mqtt_status', connected: false });
});

mqttClient.on('error', (err) => console.error('[MQTT] Error:', err.message));

mqttClient.on('message', (topic, payload) => {
  let msg;
  try { msg = JSON.parse(payload.toString()); }
  catch { msg = { raw: payload.toString() }; }

  if (topic === TOPIC_STATE) {
    latestState = { ...latestState, ...msg, online: true };
    broadcast({ type: 'state', data: latestState });

  } else if (topic === TOPIC_EVENT) {
    const evt = { ...msg, receivedAt: Date.now() };
    addEvent(evt);
    broadcast({ type: 'event', data: evt });

  } else if (topic === TOPIC_HBEAT) {
    latestState.online = true;
    latestState.lastHeartbeat = Date.now();
    broadcast({ type: 'heartbeat', data: msg });

  } else if (topic === TOPIC_COMMAND) {
    const evt = { ...msg, topic, receivedAt: Date.now() };
    addEvent(evt);
    broadcast({ type: 'command_confirm', data: evt });
  }
});

// Mark device offline if no heartbeat for 30s
setInterval(() => {
  if (latestState.lastHeartbeat && Date.now() - latestState.lastHeartbeat > 30000) {
    if (latestState.online) {
      latestState.online = false;
      broadcast({ type: 'state', data: latestState });
    }
  }
}, 5000);

// ── Start server ──────────────────────────────────────────────
httpServer.listen(PORT, () => {
  console.log(`\n╔══════════════════════════════════════╗`);
  console.log(`║  Railway Dashboard Server            ║`);
  console.log(`║  http://localhost:${PORT}              ║`);
  console.log(`║  MQTT: ${MQTT_BROKER}:${MQTT_PORT}   ║`);
  console.log(`║  Device: ${DEVICE_ID}     ║`);
  console.log(`╚══════════════════════════════════════╝\n`);
});
