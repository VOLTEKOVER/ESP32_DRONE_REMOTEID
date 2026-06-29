'use strict';
const { app, BrowserWindow, ipcMain, dialog } = require('electron');
const path = require('path');
const fs = require('fs');
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
  emitToRenderer('rid-packet', clean);
}

wifiCapture.onPacket = onCaptureData;
bleCapture.onPacket = onCaptureData;
serialCapture.onPacket = onCaptureData;

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280, height: 860, minWidth: 800, minHeight: 600,
    title: 'RID Hub',
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  mainWindow.loadFile(path.join(__dirname, 'renderer', 'index.html'));
  mainWindow.on('closed', () => { mainWindow = null; });
}

app.whenReady().then(() => {
  createWindow();
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
