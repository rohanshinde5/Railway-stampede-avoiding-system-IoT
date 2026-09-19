import React, { useState, useEffect } from 'react';

export default function App() {
  const [state, setState] = useState({
    deviceId: 'ESP32-RAILSAFE-01',
    crowdCount: 0,
    capacity: 20,
    occupancyPercent: 0,
    status: 'UNKNOWN',
    emergency: false,
    gateState: 'UNKNOWN',
    timestamp: 0,
    online: false
  });

  const [events, setEvents] = useState([]);
  const [mqttStatus, setMqttStatus] = useState({ connected: false, broker: 'test.mosquitto.org:1883' });
  const [sendingCmd, setSendingCmd] = useState(false);

  useEffect(() => {
    // Initial REST state fetch
    fetch('/api/state')
      .then(res => res.json())
      .then(data => setState(prev => ({ ...prev, ...data })))
      .catch(console.error);

    fetch('/api/events')
      .then(res => res.json())
      .then(data => setEvents(data))
      .catch(console.error);

    fetch('/api/status')
      .then(res => res.json())
      .then(data => setMqttStatus({ connected: data.mqttConnected, broker: data.broker }))
      .catch(console.error);

    // WebSocket live stream
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    const wsUrl = `${protocol}//${window.location.host}/ws`;
    
    let ws;
    function connectWs() {
      ws = new WebSocket(wsUrl);

      ws.onmessage = (event) => {
        try {
          const msg = JSON.parse(event.data);
          if (msg.type === 'state') {
            setState(prev => ({ ...prev, ...msg.data }));
          } else if (msg.type === 'event') {
            setEvents(prev => [msg.data, ...prev.slice(0, 49)]);
          } else if (msg.type === 'events') {
            setEvents(msg.data);
          } else if (msg.type === 'mqtt_status') {
            setMqttStatus(prev => ({ ...prev, connected: msg.connected }));
          } else if (msg.type === 'command_confirm') {
            setEvents(prev => [{ ...msg.data, type: 'COMMAND_EXECUTED' }, ...prev.slice(0, 49)]);
          }
        } catch (e) {
          console.error('[WS Parse Error]', e);
        }
      };

      ws.onclose = () => {
        setTimeout(connectWs, 3000);
      };
    }

    connectWs();
    return () => { if (ws) ws.close(); };
  }, []);

  const triggerCommand = async (cmd) => {
    setSendingCmd(true);
    try {
      await fetch(`/api/command/${cmd}`, { method: 'POST' });
    } catch (e) {
      alert(`Failed to send ${cmd} command: ` + e.message);
    } finally {
      setSendingCmd(false);
    }
  };

  const getStatusColor = (status) => {
    switch (status) {
      case 'NORMAL': return 'var(--status-normal)';
      case 'WARNING': return 'var(--status-warning)';
      case 'HIGH ALERT': return 'var(--status-alert)';
      case 'CRITICAL': return 'var(--status-critical)';
      default: return 'var(--text-muted)';
    }
  };

  const statusClass = state.status ? state.status.replace(/\s+/g, '-') : 'UNKNOWN';

  return (
    <div className="dashboard-container">
      {/* Header */}
      <header className="header">
        <div className="title-group">
          <span className="title-icon">🚆</span>
          <div className="title-text">
            <h1>RailSafe Monitor</h1>
            <p>Railway Station Crowd Density & Emergency Control System</p>
          </div>
        </div>
        <div className="status-pills">
          <div className="pill">
            <span className={`dot ${mqttStatus.connected ? 'online' : 'offline'}`}></span>
            MQTT Broker: {mqttStatus.broker}
          </div>
          <div className="pill">
            <span className={`dot ${state.online ? 'online' : 'offline'}`}></span>
            ESP32: {state.online ? 'ONLINE' : 'OFFLINE'}
          </div>
        </div>
      </header>

      {/* Main Grid */}
      <div className="grid-main">
        {/* Occupancy Radial Gauge */}
        <div className="card occupancy-card">
          <div className="card-title">Live Occupancy</div>
          <div 
            className="gauge-circle"
            style={{
              '--percentage': state.occupancyPercent,
              '--gauge-color': getStatusColor(state.status)
            }}
          >
            <div className="gauge-inner">
              <div className="gauge-value">{state.occupancyPercent}%</div>
              <div className="gauge-label">Capacity</div>
            </div>
          </div>
          <div className="count-badge">
            {state.crowdCount} / {state.capacity} People
          </div>
        </div>

        {/* Key Metrics */}
        <div className="stats-grid">
          <div className="stat-box">
            <span className="stat-label">Crowd Status</span>
            <div className="stat-value">
              <span className={`status-tag ${statusClass}`}>
                {state.status || 'NORMAL'}
              </span>
            </div>
          </div>

          <div className="stat-box">
            <span className="stat-label">Safety Gate State</span>
            <div className="stat-value" style={{ color: state.gateState === 'CLOSED' ? 'var(--status-critical)' : 'var(--status-normal)' }}>
              {state.gateState || 'OPEN'}
            </div>
          </div>

          <div className="stat-box">
            <span className="stat-label">Emergency Override</span>
            <div className="stat-value" style={{ color: state.emergency ? 'var(--status-critical)' : 'var(--text-secondary)' }}>
              {state.emergency ? '🚨 ACTIVE' : '✅ CLEAR'}
            </div>
          </div>

          <div className="stat-box">
            <span className="stat-label">Device Identifier</span>
            <div className="stat-value" style={{ fontSize: '18px', color: 'var(--text-secondary)' }}>
              {state.deviceId}
            </div>
          </div>
        </div>

        {/* Control Actions */}
        <div className="card control-card">
          <div className="card-title">Remote Emergency Operations</div>
          <div className="controls-row">
            <button 
              className="btn btn-danger" 
              onClick={() => triggerCommand('emergency')}
              disabled={sendingCmd}
            >
              🚨 Trigger Emergency Override
            </button>
            <button 
              className="btn btn-secondary" 
              onClick={() => triggerCommand('reset')}
              disabled={sendingCmd}
            >
              🔄 Reset Station System
            </button>
          </div>
        </div>

        {/* Real-time Event Log */}
        <div className="card log-card">
          <div className="card-title">
            <span>Live Activity Stream</span>
            <span style={{ fontSize: '11px', color: 'var(--text-muted)' }}>MQTT Topic: railway/railsafe/{state.deviceId}/#</span>
          </div>
          <div className="log-table-wrapper">
            {events.length === 0 ? (
              <div className="empty-state">No telemetry events received yet. Start the Wokwi simulation to view live events.</div>
            ) : (
              <table className="log-table">
                <thead>
                  <tr>
                    <th>Time</th>
                    <th>Event Type</th>
                    <th>Crowd Count</th>
                    <th>Details</th>
                  </tr>
                </thead>
                <tbody>
                  {events.map((evt, idx) => (
                    <tr key={idx}>
                      <td>{evt.timestamp || evt.receivedAt ? new Date(evt.timestamp || evt.receivedAt).toLocaleTimeString() : 'N/A'}</td>
                      <td>
                        <span className={`badge-event badge-${evt.event || evt.type || 'EVENT'}`}>
                          {evt.event || evt.type || 'EVENT'}
                        </span>
                      </td>
                      <td>{evt.crowdCount !== undefined ? `${evt.crowdCount} / 20` : '-'}</td>
                      <td>{evt.command ? `Command: ${evt.command}` : JSON.stringify(evt)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>
        </div>
      </div>
    </div>
  );
}
