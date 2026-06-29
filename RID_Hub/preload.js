'use strict';
const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('RID', {
  getSnapshot: () => ipcRenderer.invoke('get-snapshot'),
  getDeviceDetail: (mac) => ipcRenderer.invoke('get-device-detail', mac),
  resetStats: () => ipcRenderer.invoke('reset-stats'),
  startRecording: () => ipcRenderer.invoke('start-recording'),
  stopRecording: () => ipcRenderer.invoke('stop-recording'),
  getSession: () => ipcRenderer.invoke('get-session'),
  exportCSV: () => ipcRenderer.invoke('export-csv'),
  exportKML: () => ipcRenderer.invoke('export-kml'),
  saveFile: (data, name, filter) => ipcRenderer.invoke('save-file', data, name, filter),
  listPorts: () => ipcRenderer.invoke('list-ports'),
  connectSerial: (path, baud) => ipcRenderer.invoke('connect-serial', path, baud),
  disconnectSerial: () => ipcRenderer.invoke('disconnect-serial'),
  startWiFi: (iface) => ipcRenderer.invoke('start-wifi-capture', iface),
  stopWiFi: () => ipcRenderer.invoke('stop-wifi-capture'),
  startBLE: () => ipcRenderer.invoke('start-ble-capture'),
  stopBLE: () => ipcRenderer.invoke('stop-ble-capture'),
  importPcap: () => ipcRenderer.invoke('import-pcap'),
  onPacket: (cb) => { ipcRenderer.on('rid-packet', (_, data) => cb(data)); },
  onPcapDone: (cb) => { ipcRenderer.on('rid-pcap-done', () => cb()); },
});
