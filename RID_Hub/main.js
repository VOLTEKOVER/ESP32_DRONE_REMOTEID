'use strict';
const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
const https = require('https');
const { autoUpdater } = require('electron-updater');
const { Tracker } = require('./src/tracker');
const { WiFiCapture, BLECapture, SerialCapture } = require('./src/capture');
const { extractOdidFromBeacon, formatSummary, decodeOdidMessage } = require('./src/decoder');

let mainWindow = null;
const tracker = new Tracker();
const wifiCapture = new WiFiCapture();
const bleCapture = new BLECapture();
const serialCapture = new SerialCapture();

let captureSources = [];

function emitToRenderer(channel, data) {
  if (mainWindow && !mainWindow.isDestroyed()) {
    mainWindow.webContents.send(channel, data);
  }
}

function onCaptureData(data) {
  const clean = tracker.onCapture(data);
  const snap = tracker.getSnapshot();
  const devices = snap.devices.map(d => ({
    mac: d.mac,
    basicId: d.basic_id,
    operatorId: d.operator_id,
    uaType: d.ua_type,
    packetsCount: d.packet_count,
    rssiAvg: d.rssi_avg,
    rssiLast: d.rssi_last,
    rssiMin: d.rssi_min,
    rssiMax: d.rssi_max,
    firstSeen: d.first_seen * 1000,
    lastSeen: d.last_seen * 1000,
    lat: d.last_location?.latitude || 0,
    lon: d.last_location?.longitude || 0,
    altitude: d.last_location?.altitude_geodetic ?? null,
    altitudePressure: d.last_location?.altitude_pressure ?? null,
    height: d.last_location?.height ?? null,
    speed: d.last_location?.speed_horizontal ?? null,
    verticalSpeed: d.last_location?.speed_vertical ?? null,
    direction: d.last_location?.direction ?? null,
    status: d.last_location?.status ?? null,
    selfId: d.self_id,
    trail: d.location_trail.map(p => ({
      lat: p.lat, lon: p.lon, timestamp: p.ts * 1000, rssi: null,
    })),
    seenMessages: d.messages_seen || [],
  }));
  emitToRenderer('rid-packet', {
    raw: clean,
    snapshot: devices,
    totalPackets: snap.stats.total_packets,
    sessionPackets: snap.stats.session_packets,
  });
}

wifiCapture.onPacket = onCaptureData;
bleCapture.onPacket = onCaptureData;
serialCapture.onPacket = onCaptureData;

function createWindow() {
  const isMac = process.platform === 'darwin';
  mainWindow = new BrowserWindow({
    width: 1280, height: 860, minWidth: 800, minHeight: 600,
    title: 'RID Hub',
    frame: isMac,
    titleBarStyle: isMac ? 'hidden' : 'default',
    ...(isMac ? { trafficLightPosition: { x: 12, y: 8 } } : {}),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  mainWindow.loadFile(path.join(__dirname, 'index.html'));
  mainWindow.on('closed', () => { mainWindow = null; });
  mainWindow.on('maximize', () => { mainWindow?.webContents.send('window-state', true); });
  mainWindow.on('unmaximize', () => { mainWindow?.webContents.send('window-state', false); });
}

// === UPDATER ===
autoUpdater.autoDownload = false;
autoUpdater.autoInstallOnAppQuit = true;

function checkForUpdates() {
  if (app.isPackaged) {
    autoUpdater.checkForUpdates().catch(() => {});
  } else {
    // Dev mode: check GitHub API
    const repo = 'VOLTEKOVER/ESP_DRONE_REMOTEID';
    https.get(`https://api.github.com/repos/${repo}/releases/latest`, {
      headers: { 'User-Agent': 'RID-Hub' },
    }, (res) => {
      let body = '';
      res.on('data', d => body += d);
      res.on('end', () => {
        try {
          const rel = JSON.parse(body);
          const tag = rel.tag_name || '';
          const current = app.getVersion();
          const hasUpdate = tag.replace(/^v/i,'') !== current;
          mainWindow?.webContents.send('update-checked', {
            hasUpdate, current, latest: tag,
            url: rel.html_url, body: rel.body,
          });
        } catch (_) {}
      });
    }).on('error', () => {});
  }
}

autoUpdater.on('checking-for-update', () => {
  mainWindow?.webContents.send('update-status', 'checking');
});
autoUpdater.on('update-available', (info) => {
  mainWindow?.webContents.send('update-available', {
    version: info.version, url: info.files?.[0]?.url,
  });
});
autoUpdater.on('update-not-available', () => {
  mainWindow?.webContents.send('update-status', 'up-to-date');
});
autoUpdater.on('download-progress', (p) => {
  mainWindow?.webContents.send('update-progress', {
    percent: Math.round(p.percent),
    bytesPerSecond: p.bytesPerSecond,
    transferred: p.transferred,
    total: p.total,
  });
});
autoUpdater.on('update-downloaded', () => {
  mainWindow?.webContents.send('update-status', 'downloaded');
});
autoUpdater.on('error', (err) => {
  mainWindow?.webContents.send('update-error', err.message);
});

app.whenReady().then(() => {
  createWindow();
  checkForUpdates();
  app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on('window-all-closed', () => {
  stopAllCapture();
  if (process.platform !== 'darwin') app.quit();
});

app.on('before-quit', () => { stopAllCapture(); });

function stopAllCapture() {
  wifiCapture.stop();
  bleCapture.stop();
  serialCapture.stop();
  captureSources = [];
}

ipcMain.handle('get-snapshot', () => tracker.getSnapshot());
ipcMain.handle('get-device-detail', (_, mac) => tracker.getDeviceDetail(mac));
ipcMain.handle('reset-stats', () => { tracker.reset(); return true; });

ipcMain.handle('start-recording', () => { tracker.startRecording(); return true; });
ipcMain.handle('stop-recording', () => { tracker.stopRecording(); return true; });
ipcMain.handle('get-session', () => tracker.sessionPackets);

ipcMain.handle('export-csv', () => tracker.generateCSV());
ipcMain.handle('export-kml', () => tracker.generateKML());

ipcMain.handle('list-ports', async () => {
  try {
    const { SerialPort } = require('serialport');
    const ports = await SerialPort.list();
    return ports.map(p => ({ path: p.path, manufacturer: p.manufacturer || '' }));
  } catch (_) { return []; }
});

ipcMain.handle('connect-serial', async (_, path, baud) => {
  serialCapture.stop();
  await serialCapture.start(path, baud || 115200);
  captureSources.push('serial');
  return true;
});

ipcMain.handle('disconnect-serial', () => {
  serialCapture.stop();
  captureSources = captureSources.filter(s => s !== 'serial');
  return true;
});

ipcMain.handle('start-wifi-capture', (_, iface) => {
  wifiCapture.start(iface);
  captureSources.push('wifi');
  return true;
});

ipcMain.handle('stop-wifi-capture', () => {
  wifiCapture.stop();
  captureSources = captureSources.filter(s => s !== 'wifi');
  return true;
});

ipcMain.handle('start-ble-capture', () => {
  bleCapture.start();
  captureSources.push('ble');
  return true;
});

ipcMain.handle('stop-ble-capture', () => {
  bleCapture.stop();
  captureSources = captureSources.filter(s => s !== 'ble');
  return true;
});

ipcMain.handle('import-pcap', async () => {
  const result = await dialog.showOpenDialog(mainWindow, {
    filters: [{ name: 'PCAP Files', extensions: ['pcap', 'pcapng'] }],
    properties: ['openFile'],
  });
  if (result.canceled || !result.filePaths.length) return;
  const filePath = result.filePaths[0];
  try {
    const pcapModule = require('pcap');
    const parser = new pcapModule.PCAPOffline(filePath);
    parser.on('packet', (raw) => {
      const msgs = extractOdidFromBeacon(Buffer.from(raw));
      if (msgs && msgs.length) {
        const data = {
          timestamp: Date.now() / 1000,
          source_mac: 'pcap',
          rssi: null,
          channel: 0,
          summary: formatSummary(msgs),
          messages: msgs,
        };
        tracker.onCapture(data);
      }
    });
    parser.on('complete', () => { emitToRenderer('rid-pcap-done', {}); });
    return filePath;
  } catch (e) {
    return { error: e.message };
  }
});

ipcMain.handle('get-version', () => app.getVersion());

ipcMain.handle('check-update', () => {
  if (app.isPackaged) {
    autoUpdater.checkForUpdates();
    return { status: 'checking' };
  } else {
    checkForUpdates();
    return { status: 'checking' };
  }
});
ipcMain.handle('download-update', () => {
  if (app.isPackaged) {
    autoUpdater.downloadUpdate();
    return { status: 'downloading' };
  }
  return { status: 'dev-mode', msg: 'Scarica manualmente da GitHub' };
});
ipcMain.handle('restart-app', () => {
  if (app.isPackaged) {
    autoUpdater.quitAndInstall();
  } else {
    app.relaunch();
    app.exit();
  }
});

ipcMain.handle('window-minimize', () => { mainWindow?.minimize(); });
ipcMain.handle('window-maximize', () => {
  if (mainWindow?.isMaximized()) mainWindow.unmaximize();
  else mainWindow?.maximize();
});
ipcMain.handle('window-close', () => mainWindow?.close());
ipcMain.handle('window-is-maximized', () => mainWindow?.isMaximized());

ipcMain.handle('save-file', async (_, data, defaultName, filter) => {
  const result = await dialog.showSaveDialog(mainWindow, {
    defaultPath: defaultName,
    filters: filter || [{ name: 'All Files', extensions: ['*'] }],
  });
  if (result.canceled || !result.filePath) return false;
  try {
    fs.writeFileSync(result.filePath, data, 'utf8');
    return true;
  } catch (e) {
    return { error: e.message };
  }
});
