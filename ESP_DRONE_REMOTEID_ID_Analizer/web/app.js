"use strict";

/* ---- State ---- */
const state = {
  devices: {},
  packetLog: [],
  rssiHistory: [],  // [{ts, mac, rssi}]
  rateHistory: [],   // [{ts, count}]
  rateBuckets: {},
  maxLog: 200,
  maxRssi: 120,
  maxRate: 180,
  tab: "devices",
  map: null,
  mapMarkers: {},
};

/* ---- Elements ---- */
const els = {
  status: document.getElementById("ws-status"),
  statsDevices: document.getElementById("stats-devices"),
  statsIds: document.getElementById("stats-ids"),
  statsPackets: document.getElementById("stats-packets"),
  tbody: document.getElementById("device-tbody"),
  packetLog: document.getElementById("packet-log"),
};

/* ---- WebSocket ---- */
const WS_URL = typeof WS_CONFIG !== "undefined" ? WS_CONFIG : "__WS_URL__";

let ws = null;

function connectWs() {
  if (ws && ws.readyState === WebSocket.OPEN) return;
  const url = WS_URL.startsWith("__") ? "ws://127.0.0.1:8765" : WS_URL;
  ws = new WebSocket(url);
  ws.onopen = () => {
    els.status.textContent = "Connected";
    els.status.className = "badge badge-on";
  };
  ws.onclose = () => {
    els.status.textContent = "Disconnected";
    els.status.className = "badge badge-off";
    setTimeout(connectWs, 2000);
  };
  ws.onmessage = (e) => {
    try { handleMsg(JSON.parse(e.data)); } catch (_) {}
  };
}

function handleMsg(msg) {
  if (msg.type === "reset") {
    state.devices = {};
    state.packetLog = [];
    state.rssiHistory = [];
    state.rateHistory = [];
    state.rateBuckets = {};
    clearMapMarkers();
    render();
    return;
  }
  if (msg.type === "packet") {
    onPacket(msg.data);
  }
}

function onPacket(data) {
  const ts = data.ts || Date.now() / 1000;
  const mac = data.mac || "?";
  const rssi = data.rssi;

  /* device state */
  if (!state.devices[mac]) {
    state.devices[mac] = {
      mac,
      firstSeen: ts,
      lastSeen: ts,
      rssiSamples: [],
      basicId: "",
      operatorId: "",
      uaType: 0,
      lat: null,
      lon: null,
      packetCount: 0,
    };
  }
  const dev = state.devices[mac];
  dev.lastSeen = ts;
  dev.packetCount++;
  if (rssi !== null && rssi !== undefined) {
    dev.rssiSamples.push(rssi);
    if (dev.rssiSamples.length > 200) dev.rssiSamples = dev.rssiSamples.slice(-200);
  }

  /* RSSI history for chart */
  if (rssi !== null && rssi !== undefined) {
    state.rssiHistory.push({ ts, mac, rssi });
    if (state.rssiHistory.length > state.maxRssi) state.rssiHistory = state.rssiHistory.slice(-state.maxRssi);
  }

  /* Packet rate */
  const bucketKey = Math.floor(ts / 5) * 5;
  state.rateBuckets[bucketKey] = (state.rateBuckets[bucketKey] || 0) + 1;
  rebuildRateHistory(ts);

  /* Packet log */
  const line = `[${fmtTime(ts)}] ${mac} ${rssi !== null ? "RSSI:" + rssi + "dBm" : "       "}  ${data.summary || ""}`;
  state.packetLog.push(line);
  if (state.packetLog.length > state.maxLog) state.packetLog = state.packetLog.slice(-state.maxLog);

  /* Check if summary has IDs/location to extract */
  if (data.summary) {
    const idMatch = data.summary.match(/ID:([^(]+)/);
    if (idMatch) dev.basicId = idMatch[1];
    const opMatch = data.summary.match(/Op:([^\|]+)/);
    if (opMatch) dev.operatorId = opMatch[1].trim();
    const gpsMatch = data.summary.match(/GPS:([^,]+),([^\| ]+)/);
    if (gpsMatch) {
      dev.lat = parseFloat(gpsMatch[1]);
      dev.lon = parseFloat(gpsMatch[2]);
    }
    const uaMatch = data.summary.match(/\(([^)]+)\)/);
    if (uaMatch) dev.uaType = uaMatch[1];
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

/* ---- Render ---- */

function fmtTime(ts) {
  const d = new Date(ts * 1000);
  return d.toLocaleTimeString("en-US", { hour12: false });
}

function fmtTimeAgo(ts) {
  const sec = Math.floor(Date.now() / 1000 - ts);
  if (sec < 5) return "now";
  if (sec < 60) return sec + "s ago";
  if (sec < 3600) return Math.floor(sec / 60) + "m ago";
  return Math.floor(sec / 3600) + "h ago";
}

function avg(arr) {
  if (!arr.length) return null;
  return arr.reduce((a, b) => a + b, 0) / arr.length;
}

function rssiClass(val) {
  if (val === null || val === undefined) return "";
  if (val >= -60) return "rssi-ok";
  if (val >= -75) return "rssi-warn";
  return "rssi-bad";
}

function renderDevices() {
  const now = Date.now() / 1000;
  const list = Object.values(state.devices).sort((a, b) => b.lastSeen - a.lastSeen);

  if (list.length === 0) {
    els.tbody.innerHTML = '<tr><td colspan="9" class="empty">Waiting for RID packets...</td></tr>';
    return;
  }

  let html = "";
  for (const d of list) {
    const active = now - d.lastSeen < 30;
    const rssiAvg = avg(d.rssiSamples);
    const rssiLast = d.rssiSamples.length > 0 ? d.rssiSamples[d.rssiSamples.length - 1] : null;
    const latStr = d.lat !== null ? d.lat.toFixed(5) : "";
    const lonStr = d.lon !== null ? d.lon.toFixed(5) : "";
    html += `<tr style="opacity:${active ? 1 : 0.5}">
      <td class="mac">${d.mac}</td>
      <td>${esc(d.basicId || "")}</td>
      <td>${d.uaType || ""}</td>
      <td>${esc(d.operatorId || "")}</td>
      <td>${latStr}</td>
      <td>${lonStr}</td>
      <td class="rssi ${rssiClass(rssiLast)}">${rssiLast !== null ? rssiLast + " dBm" : ""}</td>
      <td>${d.packetCount}</td>
      <td>${fmtTimeAgo(d.lastSeen)}</td>
    </tr>`;
  }
  els.tbody.innerHTML = html;
}

function renderLog() {
  if (state.packetLog.length === 0) {
    els.packetLog.innerHTML = '<div class="placeholder">No packets yet.</div>';
    return;
  }
  els.packetLog.innerHTML = state.packetLog.map(l => `<div class="log-entry">${esc(l)}</div>`).join("");
  els.packetLog.scrollTop = els.packetLog.scrollHeight;
}

function renderStats() {
  const now = Date.now() / 1000;
  const total = Object.keys(state.devices).length;
  const active = Object.values(state.devices).filter(d => now - d.lastSeen < 30).length;
  const uniqueIds = new Set(Object.values(state.devices).map(d => d.basicId).filter(Boolean)).size;
  const recent = state.packetLog.length;
  els.statsDevices.textContent = `${active}/${total} devices`;
  els.statsIds.textContent = `${uniqueIds} IDs`;
  els.statsPackets.textContent = `${recent} pkt`;
}

/* ---- Charts ---- */
let chartRssi = null;
let chartRate = null;
const chartColors = ["#58a6ff", "#3fb950", "#f0883e", "#f85149", "#bc8cff", "#ff7b72", "#79c0ff", "#a5d6ff"];
let rssiDsMap = {};

function initCharts() {
  const ctx1 = document.getElementById("chart-rssi");
  const ctx2 = document.getElementById("chart-rate");
  if (!ctx1 || !ctx2) return;

  const gridCol = "#21262d";
  const tickCol = "#8b949e";

  chartRssi = new Chart(ctx1, {
    type: "scatter",
    data: { datasets: [] },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        x: { type: "linear", min: Date.now() / 1000 - 120, max: Date.now() / 1000, title: { display: false }, grid: { color: gridCol }, ticks: { color: tickCol, maxTicksLimit: 8 } },
        y: { min: -100, max: -20, title: { display: true, text: "RSSI (dBm)", color: tickCol }, grid: { color: gridCol }, ticks: { color: tickCol } },
      },
      plugins: { legend: { display: false } },
    },
  });

  chartRate = new Chart(ctx2, {
    type: "bar",
    data: {
      datasets: [{
        label: "Packets/5s",
        data: [],
        backgroundColor: "#58a6ff",
        borderRadius: 2,
      }],
    },
    options: {
      animation: false,
      responsive: true,
      maintainAspectRatio: false,
      scales: {
        x: { grid: { display: false }, ticks: { color: tickCol, maxTicksLimit: 10 } },
        y: { beginAtZero: true, title: { display: true, text: "Count", color: tickCol }, grid: { color: gridCol }, ticks: { color: tickCol } },
      },
      plugins: { legend: { display: false } },
    },
  });
}

function updateCharts() {
  if (!chartRssi || !chartRate) return;

  const now = Date.now() / 1000;
  const history = state.rssiHistory;
  const macs = [...new Set(history.map(h => h.mac))].slice(0, 8);

  /* Prune stale datasets */
  for (const mac of Object.keys(rssiDsMap)) {
    if (!macs.includes(mac)) {
      const idx = chartRssi.data.datasets.indexOf(rssiDsMap[mac]);
      if (idx !== -1) chartRssi.data.datasets.splice(idx, 1);
      delete rssiDsMap[mac];
    }
  }

  /* Update / add datasets in-place */
  let ci = 0;
  for (const mac of macs) {
    if (!rssiDsMap[mac]) {
      const color = chartColors[ci++ % chartColors.length];
      const ds = {
        label: mac.length > 12 ? mac.slice(-12) : mac,
        data: [],
        backgroundColor: color,
        borderColor: color,
        pointRadius: 2,
        pointHoverRadius: 5,
      };
      chartRssi.data.datasets.push(ds);
      rssiDsMap[mac] = ds;
    }
    const ds = rssiDsMap[mac];
    const pts = history.filter(h => h.mac === mac).map(h => ({ x: h.ts, y: h.rssi }));
    ds.data.splice(0, ds.data.length, ...pts);
  }

  /* Fix X axis window: last 120s */
  chartRssi.options.scales.x.min = now - 120;
  chartRssi.options.scales.x.max = now;
  chartRssi.update("none");

  /* Rate */
  const rate = state.rateHistory;
  chartRate.data.labels = rate.map(r => fmtTime(r.ts));
  chartRate.data.datasets[0].data = rate.map(r => r.count);
  chartRate.update("none");
}

/* ---- Map ---- */

function initMap() {
  const el = document.getElementById("map");
  if (!el) return;
  state.map = L.map("map", { attributionControl: false }).setView([45, 10], 3);
  L.tileLayer("https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png", {
    maxZoom: 18,
    opacity: 0.7,
  }).addTo(state.map);
}

function updateMap() {
  if (!state.map) return;
  const now = Date.now() / 1000;
  const newMacs = new Set();

  for (const [mac, dev] of Object.entries(state.devices)) {
    if (dev.lat === null || dev.lon === null) continue;
    if (now - dev.lastSeen > 60) continue;  /* stale */
    newMacs.add(mac);

    if (!state.mapMarkers[mac]) {
      const icon = L.circleMarker([dev.lat, dev.lon], {
        radius: 8,
        color: "#58a6ff",
        fillColor: "#58a6ff",
        fillOpacity: 0.5,
        weight: 2,
      });
      const label = `<b>${esc(dev.basicId || mac)}</b><br>${mac}<br>${dev.lat.toFixed(5)}, ${dev.lon.toFixed(5)}`;
      icon.bindTooltip(label);
      icon.addTo(state.map);
      state.mapMarkers[mac] = icon;
    } else {
      state.mapMarkers[mac].setLatLng([dev.lat, dev.lon]);
    }
  }

  /* remove stale */
  for (const mac of Object.keys(state.mapMarkers)) {
    if (!newMacs.has(mac)) {
      state.map.removeLayer(state.mapMarkers[mac]);
      delete state.mapMarkers[mac];
    }
  }

  if (Object.keys(state.mapMarkers).length > 0) {
    const group = L.featureGroup(Object.values(state.mapMarkers));
    state.map.fitBounds(group.getBounds().pad(0.1), { maxZoom: 15 });
  }
}

function clearMapMarkers() {
  for (const mac of Object.keys(state.mapMarkers)) {
    state.map.removeLayer(state.mapMarkers[mac]);
  }
  state.mapMarkers = {};
}

/* ---- Tabs ---- */
document.querySelectorAll(".tab").forEach(tab => {
  tab.addEventListener("click", () => {
    document.querySelectorAll(".tab").forEach(t => t.classList.remove("active"));
    document.querySelectorAll(".tab-content").forEach(t => t.classList.remove("active"));
    tab.classList.add("active");
    const target = document.getElementById("tab-" + tab.dataset.tab);
    if (target) target.classList.add("active");
    state.tab = tab.dataset.tab;
    if (state.tab === "map") {
      setTimeout(() => state.map && state.map.invalidateSize(), 50);
      updateMap();
    }
  });
});

/* ---- Helpers ---- */
function esc(s) {
  if (!s) return "";
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

/* ---- Global ---- */
function resetStats() {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send(JSON.stringify({ command: "reset" }));
  }
}

/* ---- Main loop ---- */
function tick() {
  renderDevices();
  renderLog();
  renderStats();
  updateCharts();
  if (state.tab === "map") updateMap();
}

/* ---- Init ---- */
function init() {
  connectWs();
  initCharts();
  initMap();
  setInterval(tick, 1000);
}

/* ---- Dark mode ---- */
function toggleDark() {
  const html = document.documentElement;
  const isDark = html.getAttribute("data-theme") === "dark";
  html.setAttribute("data-theme", isDark ? "light" : "dark");
  localStorage.setItem("rid-theme", isDark ? "light" : "dark");
}
(function initTheme() {
  const saved = localStorage.getItem("rid-theme");
  if (saved === "dark" || (!saved && window.matchMedia("(prefers-color-scheme: dark)").matches)) {
    document.documentElement.setAttribute("data-theme", "dark");
  }
})();

document.addEventListener("DOMContentLoaded", init);
