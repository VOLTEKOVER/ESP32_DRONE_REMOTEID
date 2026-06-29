'use strict';

const state = {
  devices: {}, packetLog: [],
  rssiHistory: [], rateHistory: [], rateBuckets: {}, devCountHistory: [], devCountBuckets: {},
  maxLog: 500, maxRssi: 300, maxRate: 300,
  tab: 'devices', map: null, mapMarkers: {}, mapTrails: {},
  filterType: '', filterRssi: '', filterActive: false,
  selectedMac: null, notifications: false, recording: false,
  capSources: [],
};

const els = {};
function q(s) { els[s] = els[s] || document.getElementById(s); return els[s]; }

/* ---- IPC bridge ---- */
const api = window.RID;

api.onPacket((data) => onPacket(data));
api.onPcapDone(() => addLogLine('--- PCAP IMPORT DONE ---'));

async function loadSnapshot() {
  try {
    const snap = await api.getSnapshot();
    if (!snap) return;
    for (const d of (snap.devices || [])) restoreDevice(d);
  } catch (_) {}
}

function handleMsg(msg) {
  if (msg.type === 'packet') onPacket(msg.data);
}

function restoreDevice(d) {
  if (!d.mac || state.devices[d.mac]) return;
  state.devices[d.mac] = {
    mac: d.mac, firstSeen: d.first_seen || 0, lastSeen: d.last_seen || 0, rssiSamples: [],
    basicId: d.basic_id || '', operatorId: d.operator_id || '', selfId: d.self_id || '',
    uaType: d.ua_type || 0, lat: null, lon: null, packetCount: d.packet_count || 0,
    locationTrail: [], messagesSeen: [],
  };
  const dev = state.devices[d.mac];
  if (d.last_location) { dev.lat = d.last_location.latitude; dev.lon = d.last_location.longitude; }
  if (d.location_trail) dev.locationTrail = d.location_trail;
}

function onPacket(data) {
  const ts = data.ts || Date.now() / 1000;
  const mac = data.mac || '?';
  const rssi = data.rssi;
  if (!state.devices[mac]) {
    state.devices[mac] = {
      mac, firstSeen: ts, lastSeen: ts, rssiSamples: [],
      basicId: '', operatorId: '', selfId: '', uaType: 0,
      lat: null, lon: null, packetCount: 0, locationTrail: [], messagesSeen: [],
    };
    if (state.notifications && document.hidden && 'Notification' in window && Notification.permission === 'granted') {
      new Notification('New Drone Detected', { body: `${mac} appeared` });
    }
  }
  const dev = state.devices[mac];
  dev.lastSeen = ts;
  dev.packetCount++;
  if (rssi != null) {
    dev.rssiSamples.push(rssi);
    if (dev.rssiSamples.length > 500) dev.rssiSamples = dev.rssiSamples.slice(-500);
  }
  if (rssi != null) {
    state.rssiHistory.push({ ts, mac, rssi });
    if (state.rssiHistory.length > state.maxRssi) state.rssiHistory = state.rssiHistory.slice(-state.maxRssi);
  }
  const bKey = Math.floor(ts / 5) * 5;
  state.rateBuckets[bKey] = (state.rateBuckets[bKey] || 0) + 1;
  rebuildRateHistory(ts);
  const cBucket = Math.floor(ts / 10) * 10;
  state.devCountBuckets[cBucket] = Object.keys(state.devices).length;
  rebuildCountHistory(ts);
  const line = `[${fmtTime(ts)}] ${mac} ${rssi != null ? 'RSSI:' + rssi + 'dBm' : '       '}  ${data.summary || ''}`;
  state.packetLog.push(line);
  if (state.packetLog.length > state.maxLog) state.packetLog = state.packetLog.slice(-state.maxLog);
  if (data.summary) {
    const idM = data.summary.match(/ID:([^(]+)/);
    if (idM) dev.basicId = idM[1];
    const opM = data.summary.match(/Op:([^|]+)/);
    if (opM) dev.operatorId = opM[1].trim();
    const gpsM = data.summary.match(/GPS:([^,]+),([^| ]+)/);
    if (gpsM) {
      const lat = parseFloat(gpsM[1]), lon = parseFloat(gpsM[2]);
      if (!isNaN(lat) && !isNaN(lon)) {
        dev.lat = lat; dev.lon = lon;
        dev.locationTrail.push({ lat, lon, ts: Math.floor(ts) });
        if (dev.locationTrail.length > 500) dev.locationTrail = dev.locationTrail.slice(-500);
      }
    }
    const uaM = data.summary.match(/\(([^)]+)\)/);
    if (uaM) dev.uaType = uaM[1];
    const selfM = data.summary.match(/Desc:([^|]+)/);
    if (selfM) dev.selfId = selfM[1].trim();
  }
  render();
}

function rebuildRateHistory(now) {
  const keys = Object.keys(state.rateBuckets).map(Number).sort();
  const cutoff = now - state.maxRate;
  state.rateHistory = [];
  for (const k of keys) {
    if (k < cutoff) { delete state.rateBuckets[k]; continue; }
    state.rateHistory.push({ ts: k, count: state.rateBuckets[k] });
  }
}

function rebuildCountHistory(now) {
  const keys = Object.keys(state.devCountBuckets).map(Number).sort();
  const cutoff = now - 600;
  state.devCountHistory = [];
  for (const k of keys) {
    if (k < cutoff) { delete state.devCountBuckets[k]; continue; }
    state.devCountHistory.push({ ts: k, count: state.devCountBuckets[k] });
  }
}

function fmtTime(ts) { return new Date(ts * 1000).toLocaleTimeString('en-US', { hour12: false }); }

function fmtTimeAgo(ts) {
  const sec = Math.floor(Date.now() / 1000 - ts);
  if (sec < 5) return 'now';
  if (sec < 60) return sec + 's ago';
  if (sec < 3600) return Math.floor(sec / 60) + 'm ago';
  return Math.floor(sec / 3600) + 'h ago';
}

function avg(arr) { return arr.length ? arr.reduce((a, b) => a + b, 0) / arr.length : null; }

function rssiClass(val) {
  if (val == null) return '';
  if (val >= -60) return 'rssi-ok';
  if (val >= -75) return 'rssi-warn';
  return 'rssi-bad';
}

function uaTypeLabel(val) {
  const labels = { 0: 'Not defined', 1: 'Aeroplane', 2: 'Helicopter', 3: 'Gyroplane', 4: 'Hybrid Lift', 5: 'Ornithopter', 6: 'Fixed Wing', 7: 'Rotorcraft', 8: 'VTOL', 9: 'Other' };
  return labels[val] || val || '';
}

function esc(s) {
  if (!s) return '';
  return String(s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;');
}

function filterDevices(list) {
  const fType = (q('filter-input')?.value || '').toLowerCase();
  const fRssi = q('filter-rssi')?.value || '';
  const fActive = q('filter-active')?.checked || false;
  const now = Date.now() / 1000;
  return list.filter(d => {
    if (fActive && now - d.lastSeen >= 30) return false;
    if (fType) {
      const s = `${d.mac} ${d.basicId} ${d.operatorId} ${d.selfId} ${d.uaType}`.toLowerCase();
      if (!s.includes(fType)) return false;
    }
    if (fRssi) {
      const r = d.rssiSamples.length ? d.rssiSamples[d.rssiSamples.length - 1] : null;
      if (r == null) return false;
      if (fRssi === 'good' && r < -60) return false;
      if (fRssi === 'warn' && (r < -75 || r >= -60)) return false;
      if (fRssi === 'bad' && r >= -75) return false;
    }
    return true;
  });
}

function renderDevices() {
  const now = Date.now() / 1000;
  const list = Object.values(state.devices).sort((a, b) => b.lastSeen - a.lastSeen);
  const filtered = filterDevices(list);
  if (!filtered.length) {
    q('device-tbody').innerHTML = '<tr><td colspan="10" class="empty">No devices match filters</td></tr>';
    return;
  }
  let html = '';
  for (const d of filtered) {
    const active = now - d.lastSeen < 30;
    const rssiLast = d.rssiSamples.length ? d.rssiSamples[d.rssiSamples.length - 1] : null;
    const sel = state.selectedMac === d.mac ? ' row-selected' : '';
    html += `<tr class="${sel}" onclick="selectDevice('${esc(d.mac)}')" style="opacity:${active ? 1 : 0.4}">
      <td class="mac">${esc(d.mac)}</td><td>${esc(d.basicId || '')}</td>
      <td>${uaTypeLabel(d.uaType)}</td><td>${esc(d.operatorId || '')}</td>
      <td>${d.lat != null ? d.lat.toFixed(5) : ''}</td><td>${d.lon != null ? d.lon.toFixed(5) : ''}</td>
      <td class="rssi ${rssiClass(rssiLast)}">${rssiLast != null ? rssiLast + ' dBm' : ''}</td>
      <td>${d.packetCount}</td><td>${d.selfId ? esc(d.selfId.substring(0, 20)) : ''}</td>
      <td>${fmtTimeAgo(d.lastSeen)}</td>
    </tr>`;
  }
  q('device-tbody').innerHTML = html;
}

function renderLog() {
  const el = q('packet-log');
  if (!state.packetLog.length) { el.innerHTML = '<div class="placeholder">No packets yet.</div>'; return; }
  el.innerHTML = state.packetLog.map(l => `<div class="log-entry">${esc(l)}</div>`).join('');
  el.scrollTop = el.scrollHeight;
}

function addLogLine(text) {
  state.packetLog.push(text);
  if (state.packetLog.length > state.maxLog) state.packetLog = state.packetLog.slice(-state.maxLog);
  renderLog();
}

function renderStats() {
  const now = Date.now() / 1000;
  const total = Object.keys(state.devices).length;
  const active = Object.values(state.devices).filter(d => now - d.lastSeen < 30).length;
  const uniqueIds = new Set(Object.values(state.devices).map(d => d.basicId).filter(Boolean)).size;
  const allRssi = Object.values(state.devices).flatMap(d => d.rssiSamples);
  const avgRssi = allRssi.length ? (allRssi.reduce((a, b) => a + b, 0) / allRssi.length).toFixed(1) : '-';
  const recent = state.packetLog.length;
  q('stats-devices').textContent = `${active}/${total} devices`;
  q('stats-ids').textContent = `${uniqueIds} IDs`;
  q('stats-packets').textContent = `${recent} pkt`;
  if (q('total-dev-stats')) q('total-dev-stats').textContent = total;
  if (q('active-dev-stats')) q('active-dev-stats').textContent = active;
  if (q('unique-id-stats')) q('unique-id-stats').textContent = uniqueIds;
  if (q('pkt-rate-stats')) {
    const rate = state.rateHistory.length ? state.rateHistory[state.rateHistory.length - 1].count : 0;
    q('pkt-rate-stats').textContent = rate + '/5s';
  }
  if (q('avg-rssi-stats')) q('avg-rssi-stats').textContent = avgRssi + ' dBm';
  if (q('device-count')) q('device-count').textContent = total;
}

function renderUAtypes() {
  const counts = {};
  for (const d of Object.values(state.devices)) {
    const key = uaTypeLabel(d.uaType);
    counts[key] = (counts[key] || 0) + 1;
  }
  const el = q('ua-type-dist');
  if (!el) return;
  el.innerHTML = Object.entries(counts).map(([k, v]) =>
    `<span class="ua-type-tag">${k} <span class="count">${v}</span></span>`
  ).join('');
}

function renderTopRSSI() {
  const sorted = Object.values(state.devices)
    .filter(d => d.rssiSamples.length)
    .map(d => ({ mac: d.mac, avg: d.rssiSamples.reduce((a, b) => a + b, 0) / d.rssiSamples.length }))
    .sort((a, b) => b.avg - a.avg).slice(0, 10);
  const el = q('top-rssi-list');
  if (!el) return;
  el.innerHTML = sorted.length ? sorted.map(d =>
    `<div>${esc(d.mac)}: <strong>${d.avg.toFixed(1)} dBm</strong></div>`
  ).join('') : '<div class="empty">No data</div>';
}

/* ---- Detail Panel ---- */

async function selectDevice(mac) {
  if (state.selectedMac === mac) { closeDetail(); return; }
  state.selectedMac = mac;
  try {
    const data = await api.getDeviceDetail(mac);
    showDetailData(data);
  } catch (_) {}
}

function showDetailData(d) {
  if (!d) return;
  const trailLen = d.location_trail ? d.location_trail.length : 0;
  const msgs = d.messages_seen ? d.messages_seen.join(', ') : '';
  const firstSeen = new Date(d.first_seen * 1000).toLocaleString();
  const lastSeen = new Date(d.last_seen * 1000).toLocaleString();
  q('detail-body').innerHTML = `
    <div class="detail-grid">
      <div class="detail-section">
        <h4>Identity</h4>
        <dl>
          <dt>MAC</dt><dd class="mac">${esc(d.mac)}</dd>
          <dt>Basic ID</dt><dd>${esc(d.basic_id || '—')}</dd>
          <dt>UA Type</dt><dd>${uaTypeLabel(d.ua_type)}</dd>
          <dt>Operator ID</dt><dd>${esc(d.operator_id || '—')}</dd>
          <dt>Self ID</dt><dd>${esc(d.self_id || '—')}</dd>
        </dl>
      </div>
      <div class="detail-section">
        <h4>Signal</h4>
        <dl>
          <dt>Avg RSSI</dt><dd class="rssi ${rssiClass(d.rssi_avg)}">${d.rssi_avg != null ? d.rssi_avg + ' dBm' : '—'}</dd>
          <dt>Last RSSI</dt><dd class="rssi ${rssiClass(d.rssi_last)}">${d.rssi_last != null ? d.rssi_last + ' dBm' : '—'}</dd>
          <dt>Min RSSI</dt><dd>${d.rssi_min != null ? d.rssi_min + ' dBm' : '—'}</dd>
          <dt>Max RSSI</dt><dd>${d.rssi_max != null ? d.rssi_max + ' dBm' : '—'}</dd>
          <dt>Samples</dt><dd>${d.rssi_samples ? d.rssi_samples.length : 0}</dd>
        </dl>
      </div>
      <div class="detail-section">
        <h4>Movement</h4>
        <dl>
          <dt>Trail Points</dt><dd>${trailLen}</dd>
          <dt>First Seen</dt><dd>${firstSeen}</dd>
          <dt>Last Seen</dt><dd>${lastSeen}</dd>
          <dt>Packets</dt><dd>${d.packet_count}</dd>
          <dt>Messages</dt><dd>${msgs || '—'}</dd>
        </dl>
      </div>
    </div>
    ${d.last_location ? `
    <div class="detail-section">
      <h4>Last Location</h4>
      <dl class="detail-inline">
        <dt>Latitude</dt><dd>${d.last_location.latitude}</dd>
        <dt>Longitude</dt><dd>${d.last_location.longitude}</dd>
        <dt>Alt (Press)</dt><dd>${d.last_location.altitude_pressure !== undefined ? d.last_location.altitude_pressure + ' m' : '—'}</dd>
        <dt>Alt (Geo)</dt><dd>${d.last_location.altitude_geodetic !== undefined ? d.last_location.altitude_geodetic + ' m' : '—'}</dd>
        <dt>Speed H</dt><dd>${d.last_location.speed_horizontal !== undefined ? d.last_location.speed_horizontal + ' m/s' : '—'}</dd>
        <dt>Speed V</dt><dd>${d.last_location.speed_vertical !== undefined ? d.last_location.speed_vertical + ' m/s' : '—'}</dd>
        <dt>Direction</dt><dd>${d.last_location.direction !== undefined ? d.last_location.direction + '°' : '—'}</dd>
      </dl>
    </div>` : ''}
    ${trailLen > 0 ? `<div class="detail-section"><h4>Trail</h4><pre class="trail-preview">${d.location_trail.map(p => `[${fmtTime(p.ts)}] ${p.lat.toFixed(5)},${p.lon.toFixed(5)}`).join('\n')}</pre></div>` : ''}
  `;
  q('detail-panel').classList.add('open');
  if (d.last_location && state.map) {
    state.map.setView([d.last_location.latitude, d.last_location.longitude], 16);
  }
}

function closeDetail() {
  state.selectedMac = null;
  q('detail-panel').classList.remove('open');
}

/* ---- Charts ---- */
let chartRssi = null, chartRate = null, chartCount = null;
const chartColors = ['#58a6ff', '#3fb950', '#f0883e', '#f85149', '#bc8cff', '#ff7b72', '#79c0ff', '#a5d6ff', '#ffa657', '#7ee787'];
let rssiDsMap = {};

function initCharts() {
  const ctx1 = document.getElementById('chart-rssi');
  const ctx2 = document.getElementById('chart-rate');
  const ctx3 = document.getElementById('chart-count');
  if (!ctx1 || !ctx2) return;
  const gridCol = '#21262d', tickCol = '#8b949e';
  chartRssi = new Chart(ctx1, {
    type: 'scatter', data: { datasets: [] },
    options: {
      animation: false, responsive: true, maintainAspectRatio: false,
      scales: {
        x: { type: 'linear', min: Date.now() / 1000 - 120, max: Date.now() / 1000, grid: { color: gridCol }, ticks: { color: tickCol, maxTicksLimit: 8 } },
        y: { min: -100, max: -20, title: { display: true, text: 'RSSI (dBm)', color: tickCol }, grid: { color: gridCol }, ticks: { color: tickCol } },
      },
      plugins: { legend: { display: false } },
    },
  });
  chartRate = new Chart(ctx2, {
    type: 'bar', data: { datasets: [{ label: 'Packets/5s', data: [], backgroundColor: '#58a6ff', borderRadius: 2 }] },
    options: {
      animation: false, responsive: true, maintainAspectRatio: false,
      scales: {
        x: { grid: { display: false }, ticks: { color: tickCol, maxTicksLimit: 10 } },
        y: { beginAtZero: true, title: { display: true, text: 'Count', color: tickCol }, grid: { color: gridCol }, ticks: { color: tickCol } },
      },
      plugins: { legend: { display: false } },
    },
  });
  if (ctx3) {
    chartCount = new Chart(ctx3, {
      type: 'line', data: { datasets: [{ label: 'Devices', data: [], borderColor: '#58a6ff', backgroundColor: 'rgba(88,166,255,0.1)', fill: true, tension: 0.4, pointRadius: 1, pointHoverRadius: 4, borderWidth: 2 }] },
      options: {
        animation: false, responsive: true, maintainAspectRatio: false,
        scales: {
          x: { grid: { display: false }, ticks: { color: tickCol, maxTicksLimit: 8 } },
          y: { beginAtZero: true, title: { display: true, text: 'Count', color: tickCol }, grid: { color: gridCol }, ticks: { color: tickCol, stepSize: 1 } },
        },
        plugins: { legend: { display: false } },
      },
    });
  }
}

function updateCharts() {
  if (!chartRssi || !chartRate) return;
  const now = Date.now() / 1000;
  const history = state.rssiHistory;
  const macs = [...new Set(history.map(h => h.mac))].slice(0, 8);
  for (const mac of Object.keys(rssiDsMap)) {
    if (!macs.includes(mac)) {
      const idx = chartRssi.data.datasets.indexOf(rssiDsMap[mac]);
      if (idx !== -1) chartRssi.data.datasets.splice(idx, 1);
      delete rssiDsMap[mac];
    }
  }
  let ci = 0;
  for (const mac of macs) {
    if (!rssiDsMap[mac]) {
      const color = chartColors[ci++ % chartColors.length];
      rssiDsMap[mac] = { label: mac.length > 12 ? mac.slice(-12) : mac, data: [], backgroundColor: color, borderColor: color, pointRadius: 2, pointHoverRadius: 5 };
      chartRssi.data.datasets.push(rssiDsMap[mac]);
    }
    const ds = rssiDsMap[mac];
    ds.data = history.filter(h => h.mac === mac).map(h => ({ x: h.ts, y: h.rssi }));
  }
  chartRssi.options.scales.x.min = now - 120;
  chartRssi.options.scales.x.max = now;
  chartRssi.update('none');
  chartRate.data.labels = state.rateHistory.map(r => fmtTime(r.ts));
  chartRate.data.datasets[0].data = state.rateHistory.map(r => r.count);
  chartRate.update('none');
  if (chartCount) {
    chartCount.data.labels = state.devCountHistory.map(r => fmtTime(r.ts));
    chartCount.data.datasets[0].data = state.devCountHistory.map(r => r.count);
    chartCount.update('none');
  }
}

/* ---- Map ---- */
function initMap() {
  const el = document.getElementById('map');
  if (!el) return;
  state.map = L.map('map', { attributionControl: false }).setView([45, 10], 3);
  L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png', { maxZoom: 18, opacity: 0.7 }).addTo(state.map);
}

function updateMap() {
  if (!state.map) return;
  const now = Date.now() / 1000;
  const newMacs = new Set();
  for (const [mac, dev] of Object.entries(state.devices)) {
    if (dev.lat == null || dev.lon == null) continue;
    if (now - dev.lastSeen > 60) continue;
    newMacs.add(mac);
    if (!state.mapMarkers[mac]) {
      const icon = L.circleMarker([dev.lat, dev.lon], { radius: 8, color: '#58a6ff', fillColor: '#58a6ff', fillOpacity: 0.5, weight: 2 });
      icon.bindTooltip(`<b>${esc(dev.basicId || mac)}</b><br>${mac}<br>${dev.lat.toFixed(5)}, ${dev.lon.toFixed(5)}`);
      icon.on('click', () => selectDevice(mac));
      icon.addTo(state.map);
      state.mapMarkers[mac] = icon;
    } else {
      state.mapMarkers[mac].setLatLng([dev.lat, dev.lon]);
      state.mapMarkers[mac].setTooltipContent(`<b>${esc(dev.basicId || mac)}</b><br>${mac}<br>${dev.lat.toFixed(5)}, ${dev.lon.toFixed(5)}`);
    }
    if (dev.locationTrail.length >= 2) {
      const coords = dev.locationTrail.map(p => [p.lat, p.lon]);
      if (state.mapTrails[mac]) {
        state.mapTrails[mac].setLatLngs(coords);
      } else {
        state.mapTrails[mac] = L.polyline(coords, { color: '#58a6ff', weight: 2, opacity: 0.5, dashArray: '5, 5' }).addTo(state.map);
      }
    }
  }
  for (const mac of Object.keys(state.mapMarkers)) { if (!newMacs.has(mac)) { state.map.removeLayer(state.mapMarkers[mac]); delete state.mapMarkers[mac]; } }
  for (const mac of Object.keys(state.mapTrails)) {
    if (!newMacs.has(mac)) {
      const dev = state.devices[mac];
      if (dev && now - dev.lastSeen > 120) { state.map.removeLayer(state.mapTrails[mac]); delete state.mapTrails[mac]; }
    }
  }
  if (Object.keys(state.mapMarkers).length) {
    const group = L.featureGroup(Object.values(state.mapMarkers));
    state.map.fitBounds(group.getBounds().pad(0.1), { maxZoom: 15 });
  }
}

/* ---- Recording ---- */
async function toggleRecording() {
  if (state.recording) { await api.stopRecording(); state.recording = false; }
  else { await api.startRecording(); state.recording = true; }
  updateRecordBtn();
}

function updateRecordBtn() {
  if (!q('btn-record')) return;
  q('btn-record').textContent = state.recording ? '⏹ Stop' : '⏺ Record';
  q('btn-record').className = state.recording ? 'btn btn-danger' : 'btn';
  q('stats-recording').style.display = state.recording ? 'inline' : 'none';
}

/* ---- Exports ---- */
async function exportCSV() {
  try {
    const csv = await api.exportCSV();
    await api.saveFile(csv, 'drones.csv', [{ name: 'CSV Files', extensions: ['csv'] }]);
  } catch (_) {}
}

async function exportKML() {
  try {
    const kml = await api.exportKML();
    await api.saveFile(kml, 'drones.kml', [{ name: 'KML Files', extensions: ['kml'] }]);
  } catch (_) {}
}

async function exportSession() {
  try {
    const session = await api.getSession();
    const json = JSON.stringify(session, null, 2);
    await api.saveFile(json, 'session.json', [{ name: 'JSON Files', extensions: ['json'] }]);
  } catch (_) {}
}

/* ---- Reset ---- */
async function resetStats() {
  await api.resetStats();
  state.devices = {}; state.packetLog = []; state.rssiHistory = [];
  state.rateHistory = []; state.rateBuckets = {}; state.devCountHistory = []; state.devCountBuckets = {};
  state.selectedMac = null;
  for (const k of Object.keys(state.mapMarkers)) state.map.removeLayer(state.mapMarkers[k]);
  for (const k of Object.keys(state.mapTrails)) state.map.removeLayer(state.mapTrails[k]);
  state.mapMarkers = {}; state.mapTrails = {};
  closeDetail();
  render();
}

/* ---- Capture Tab ---- */
let capWifiActive = false, capBLEActive = false, capSerialActive = false;

async function refreshPorts() {
  try {
    const ports = await api.listPorts();
    const sel = q('serial-port-select');
    sel.innerHTML = ports.map(p => `<option value="${p.path}">${p.path}${p.manufacturer ? ' (' + p.manufacturer + ')' : ''}</option>`).join('');
    if (!ports.length) sel.innerHTML = '<option value="">No serial ports found</option>';
  } catch (_) {}
}

function initCaptureTab() {
  q('btn-wifi-start').addEventListener('click', async () => {
    await api.startWiFi();
    capWifiActive = true;
    q('btn-wifi-start').style.display = 'none';
    q('btn-wifi-stop').style.display = 'inline';
    q('wifi-status').textContent = 'Capturing';
    q('wifi-status').className = 'badge badge-on';
    q('cap-source').textContent = 'WiFi';
    q('cap-source').className = 'badge badge-on';
  });
  q('btn-wifi-stop').addEventListener('click', async () => {
    await api.stopWiFi();
    capWifiActive = false;
    q('btn-wifi-start').style.display = 'inline';
    q('btn-wifi-stop').style.display = 'none';
    q('wifi-status').textContent = 'Not connected';
    q('wifi-status').className = 'badge badge-off';
    q('cap-source').textContent = 'No Capture';
    q('cap-source').className = 'badge badge-off';
  });
  q('btn-ble-start').addEventListener('click', async () => {
    await api.startBLE();
    capBLEActive = true;
    q('btn-ble-start').style.display = 'none';
    q('btn-ble-stop').style.display = 'inline';
    q('ble-status').textContent = 'Scanning';
    q('ble-status').className = 'badge badge-on';
    q('cap-source').textContent = 'BLE';
    q('cap-source').className = 'badge badge-on';
  });
  q('btn-ble-stop').addEventListener('click', async () => {
    await api.stopBLE();
    capBLEActive = false;
    q('btn-ble-start').style.display = 'inline';
    q('btn-ble-stop').style.display = 'none';
    q('ble-status').textContent = 'Not connected';
    q('ble-status').className = 'badge badge-off';
    q('cap-source').textContent = 'No Capture';
    q('cap-source').className = 'badge badge-off';
  });
  q('btn-serial-connect').addEventListener('click', async () => {
    const port = q('serial-port-select').value;
    const baud = parseInt(q('serial-baud').value) || 115200;
    if (!port) return;
    await api.connectSerial(port, baud);
    capSerialActive = true;
    q('btn-serial-connect').style.display = 'none';
    q('btn-serial-disconnect').style.display = 'inline';
    q('serial-status').textContent = `Connected (${baud} baud)`;
    q('serial-status').className = 'badge badge-on';
    q('cap-source').textContent = 'Serial';
    q('cap-source').className = 'badge badge-on';
  });
  q('btn-serial-disconnect').addEventListener('click', async () => {
    await api.disconnectSerial();
    capSerialActive = false;
    q('btn-serial-connect').style.display = 'inline';
    q('btn-serial-disconnect').style.display = 'none';
    q('serial-status').textContent = 'Not connected';
    q('serial-status').className = 'badge badge-off';
    q('cap-source').textContent = 'No Capture';
    q('cap-source').className = 'badge badge-off';
  });
  q('btn-import-pcap').addEventListener('click', async () => {
    const result = await api.importPcap();
    if (result && result.error) {
      q('pcap-status').textContent = 'Error: ' + result.error;
      q('pcap-status').className = 'badge badge-off';
    } else if (result) {
      q('pcap-status').textContent = 'Imported: ' + result;
      q('pcap-status').className = 'badge badge-on';
    }
  });
  refreshPorts();
}

/* ---- Notifications ---- */
function toggleNotifications() {
  if (!('Notification' in window)) return;
  if (Notification.permission === 'granted') {
    state.notifications = !state.notifications;
  } else if (Notification.permission === 'default') {
    Notification.requestPermission().then(perm => { state.notifications = perm === 'granted'; });
  }
  updateNotifBtn();
}

function updateNotifBtn() {
  if (q('notif-toggle')) q('notif-toggle').textContent = state.notifications ? '🔔 On' : '🔕 Off';
}

/* ---- Tabs ---- */
document.querySelectorAll('.tab').forEach(tab => {
  tab.addEventListener('click', () => {
    document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
    document.querySelectorAll('.tab-content').forEach(t => t.classList.remove('active'));
    tab.classList.add('active');
    const target = document.getElementById('tab-' + tab.dataset.tab);
    if (target) target.classList.add('active');
    state.tab = tab.dataset.tab;
    if (state.tab === 'map') { setTimeout(() => state.map && state.map.invalidateSize(), 50); updateMap(); }
  });
});

/* ---- Main loop ---- */
function tick() {
  renderDevices();
  renderLog();
  renderStats();
  updateCharts();
  if (state.tab === 'map') updateMap();
}

/* ---- Init ---- */
async function init() {
  await loadSnapshot();
  initCharts();
  initMap();
  initCaptureTab();
  if (q('detail-close')) q('detail-close').addEventListener('click', closeDetail);
  if (q('filter-input')) q('filter-input').addEventListener('input', renderDevices);
  if (q('filter-rssi')) q('filter-rssi').addEventListener('change', renderDevices);
  if (q('filter-active')) q('filter-active').addEventListener('change', renderDevices);
  if (q('export-csv')) q('export-csv').addEventListener('click', exportCSV);
  if (q('export-kml')) q('export-kml').addEventListener('click', exportKML);
  if (q('export-session')) q('export-session').addEventListener('click', exportSession);
  if (q('btn-record')) q('btn-record').addEventListener('click', toggleRecording);
  if (q('notif-toggle')) q('notif-toggle').addEventListener('click', toggleNotifications);
  setInterval(tick, 1000);
}

/* ---- Dark mode ---- */
function toggleDark() {
  const html = document.documentElement;
  const isDark = html.getAttribute('data-theme') === 'dark';
  html.setAttribute('data-theme', isDark ? 'light' : 'dark');
  localStorage.setItem('rid-theme', isDark ? 'light' : 'dark');
}

(function initTheme() {
  const saved = localStorage.getItem('rid-theme');
  if (saved === 'dark' || (!saved && window.matchMedia('(prefers-color-scheme: dark)').matches)) {
    document.documentElement.setAttribute('data-theme', 'dark');
  }
})();

document.addEventListener('DOMContentLoaded', init);
