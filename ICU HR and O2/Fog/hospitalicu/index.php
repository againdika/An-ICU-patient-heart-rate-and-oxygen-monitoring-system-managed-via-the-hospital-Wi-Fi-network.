<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>ICU Ward Monitor</title>
<script src="https://cdnjs.cloudflare.com/ajax/libs/mqtt/5.3.4/mqtt.min.js"></script>
<style>
  :root {
    --bg:        #293d3d;
    --panel:     #ffffff;
    --border:    #4a5f5f;
    --text-main: #f0f2f2;
    --text-dim:  #c3cccc;
    --blue:      #2f5fa3;
    --safe:      #2e7d4f;
    --safe-bg:   #e7f3ec;
    --yellow:    #8a6416;
    --yellow-bg: #faf1de;
    --red:       #a12c2c;
    --red-bg:    #f8e6e6;
    --nodata-bg: #eceeef;
  }
  * { box-sizing: border-box; }
  body {
    margin: 0;
    background: var(--bg);
    color: var(--text-main);
    font-family: "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
    font-weight: 700;
    min-height: 100vh;
    padding: 24px 24px 56px;
    font-size: 14px;
  }
  .wrap { max-width: 1080px; margin: 0 auto; }

  header {
    display: flex;
    align-items: baseline;
    gap: 12px;
    margin-bottom: 4px;
    padding-bottom: 14px;
    border-bottom: 2px solid var(--blue);
  }
  h1 {
    font-size: 19px;
    font-weight: 600;
    margin: 0;
    color: var(--text-main);
  }
  .ward-sub { font-size: 12px; color: var(--text-dim); }
  .conn-badge {
    font-size: 12px;
    padding: 3px 9px;
    border-radius: 3px;
    border: 1px solid var(--border);
    color: #000000;
    background: var(--panel);
  }
  .conn-badge.live { color: var(--safe); border-color: var(--safe); background: var(--safe-bg); }
  .subtitle { font-size: 12px; color: var(--text-dim); margin-left: auto; }

  .bed-grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 12px;
    margin: 18px 0 22px;
  }
  @media (max-width: 900px) { .bed-grid { grid-template-columns: repeat(2, 1fr); } }
  @media (max-width: 500px) { .bed-grid { grid-template-columns: 1fr; } }

  .tile {
    background: var(--panel);
    border: 1px solid var(--border);
    border-top: 3px solid var(--border);
    border-radius: 3px;
    padding: 14px 16px;
  }
  .tile.zone-safe   { background: var(--safe);   border-top-color: var(--safe); }
  .tile.zone-yellow { background: var(--yellow); border-top-color: var(--yellow); }
  .tile.zone-red    { background: var(--red);    border-top-color: var(--red); }

  .bed-id { font-size: 11px; color: #000000; text-transform: uppercase; letter-spacing: .04em; font-weight: 700; }
  .patient-name { font-size: 15px; font-weight: 700; margin: 2px 0 10px; color: #000000; }
  .tile.zone-safe .bed-id,
  .tile.zone-safe .patient-name,
  .tile.zone-safe .meta,
  .tile.zone-yellow .bed-id,
  .tile.zone-yellow .patient-name,
  .tile.zone-yellow .meta,
  .tile.zone-red .bed-id,
  .tile.zone-red .patient-name,
  .tile.zone-red .meta {
    color: #ffffff;
  }
  .vitals-row {
    display: flex;
    gap: 20px;
    margin-bottom: 10px;
    background: #bdbcbd;
    border: 1px solid var(--border);
    border-radius: 3px;
    padding: 8px 12px;
  }
  .vital .val { font-size: 24px; font-weight: 700; color: #000000; }
  .vital .unit { font-size: 11px; color: #000000; margin-left: 2px; }
  .zone-label {
    font-size: 15px; font-weight: 700;
    text-transform: uppercase; letter-spacing: .03em; padding: 4px 10px;
    border-radius: 3px; display: inline-block;
  }
  .zone-label.zone-safe   { background: var(--safe);   color: #fff; border: 1.5px solid #ffffff; }
  .zone-label.zone-yellow { background: var(--yellow); color: #fff; border: 1.5px solid #ffffff; }
  .zone-label.zone-red    { background: var(--red);    color: #fff; border: 1.5px solid #ffffff; }
  .zone-label.zone-nodata { background: var(--nodata-bg); color: #000000; }
  .meta { font-size: 11px; color: #000000; margin-top: 8px; }

  .panel { background: var(--panel); border: 1px solid var(--border); border-radius: 3px; padding: 16px 18px; }
  .label { font-size: 12px; font-weight: 700; text-transform: uppercase; letter-spacing: .03em; color: #000000; margin-bottom: 10px; }
  table { width: 100%; border-collapse: collapse; font-size: 13px; }
  th { text-align: left; color: #000000; font-weight: 700; font-size: 11px; text-transform: uppercase; letter-spacing: .03em; padding: 7px 8px; border-bottom: 2px solid var(--border); }
  td { padding: 7px 8px; border-bottom: 1px solid var(--border); color: #000000; }
  .lvl-red { color: var(--red); font-weight: 700; }
  .lvl-cleared { color: var(--safe); font-weight: 700; }
  .empty { font-size: 13px; color: #000000; padding: 14px 0; text-align: center; }
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>ICU Ward Monitor</h1>
    <span class="ward-sub">Bed status overview</span>
    <span class="conn-badge" id="connBadge">connecting</span>
    <span class="subtitle" id="lastUpdated"></span>
  </header>

  <div class="bed-grid" id="bedGrid"></div>

  <div class="panel">
    <div class="label">Recent Alert Log</div>
    <table>
      <thead><tr><th>Bed</th><th>Level</th><th>HR</th><th>SpO2</th><th>Time</th></tr></thead>
      <tbody id="alertBody"><tr><td colspan="5" class="empty">No alerts yet</td></tr></tbody>
    </table>
  </div>
</div>

<script>
// ── EDIT THESE to match your broker ─────────────────────────────────
const MQTT_WS_URL  = 'wss://192.168.43.110:9001';   // WebSocket TLS listener from mosquitto.conf
const MQTT_USER    = 'esp32user';
const MQTT_PASS    = 'pass@pass123';
// ─────────────────────────────────────────────────────────────────────

const bedState = {};   // bed_id -> {hr, spo2, zone, patient_name, recorded_at}

function zoneClass(zone) {
  if (zone === 'SAFE') return 'zone-safe';
  if (zone === 'YELLOW') return 'zone-yellow';
  if (zone === 'RED') return 'zone-red';
  return 'zone-nodata';
}

function fmtTime(dtStr) {
  if (!dtStr) return 'no data yet';
  const d = new Date(dtStr.replace(' ', 'T'));
  return d.toLocaleString(undefined, { month:'short', day:'numeric', hour:'2-digit', minute:'2-digit', second:'2-digit' });
}

function renderBeds() {
  const grid = document.getElementById('bedGrid');
  const bedIds = Object.keys(bedState).sort();
  grid.innerHTML = bedIds.map(id => {
    const b = bedState[id];
    const zc = zoneClass(b.zone);
    return `
      <div class="tile ${zc}">
        <div class="bed-id">${id}</div>
        <div class="patient-name">${b.patient_name || 'Unassigned'}</div>
        <div class="vitals-row">
          <div class="vital"><span class="val">${b.hr ?? '—'}</span> <span class="unit">bpm</span></div>
          <div class="vital"><span class="val">${b.spo2 ?? '—'}</span> <span class="unit">%SpO2</span></div>
        </div>
        <span class="zone-label ${zc}">${b.zone || 'NO DATA'}</span>
        <div class="meta">${fmtTime(b.recorded_at)}</div>
      </div>`;
  }).join('');
}

function renderAlerts(alerts) {
  const body = document.getElementById('alertBody');
  if (!alerts || alerts.length === 0) {
    body.innerHTML = '<tr><td colspan="5" class="empty">No alerts yet</td></tr>';
    return;
  }
  body.innerHTML = alerts.map(a => `
    <tr>
      <td>${a.bed_id}</td>
      <td class="${a.level === 'RED' ? 'lvl-red' : 'lvl-cleared'}">${a.level}</td>
      <td>${a.hr ?? '—'}</td>
      <td>${a.spo2 ?? '—'}</td>
      <td>${fmtTime(a.occurred_at)}</td>
    </tr>`).join('');
}

// ── Initial load + periodic fallback refresh from MySQL (via bridge) ──
async function loadFromApi() {
  try {
    const res = await fetch('api.php', { cache: 'no-store' });
    const data = await res.json();
    data.beds.forEach(b => { bedState[b.bed_id] = b; });
    renderBeds();
    renderAlerts(data.recent_alerts);
    document.getElementById('lastUpdated').textContent = 'DB sync ' + new Date().toLocaleTimeString();
  } catch (err) {
    console.error(err);
  }
}
loadFromApi();
// Full page reload every 61s — 1s after the ESP32's 60s sensor read cycle,
// so the freshly-published reading has time to reach the database first.
setInterval(() => location.reload(), 61000);

// ── Live push via MQTT over WebSockets ─────────────────────────────
const client = mqtt.connect(MQTT_WS_URL, {
  username: MQTT_USER,
  password: MQTT_PASS,
  reconnectPeriod: 3000,
});

const badge = document.getElementById('connBadge');

client.on('connect', () => {
  badge.textContent = 'live';
  badge.classList.add('live');
  client.subscribe('hospital/icu/+/vitals');
  client.subscribe('hospital/icu/+/alert');
  client.subscribe('hospital/icu/+/status');
});

client.on('reconnect', () => { badge.textContent = 'reconnecting...'; badge.classList.remove('live'); });
client.on('close',     () => { badge.textContent = 'disconnected';   badge.classList.remove('live'); });

client.on('message', (topic, message) => {
  const parts = topic.split('/');   // hospital/icu/BED-01/vitals
  const bedId = parts[2];
  const kind  = parts[3];
  let data;
  try { data = JSON.parse(message.toString()); } catch (e) { return; }

  if (kind === 'vitals') {
    bedState[bedId] = {
      ...bedState[bedId],
      bed_id: bedId,
      hr: data.hr, spo2: data.spo2, zone: data.zone,
      recorded_at: new Date().toISOString(),
    };
    renderBeds();
    document.getElementById('lastUpdated').textContent = 'live update ' + new Date().toLocaleTimeString();
  }
  // alert/status messages will show up in the alert log on the next loadFromApi() sync
});
</script>
</body>
</html>
