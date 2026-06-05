// ==================== GLOBAL VARIABLES ====================
let device = null;
let transport = null;
let esploader = null;

// ==================== INITIALIZATION ====================
document.addEventListener('DOMContentLoaded', () => {
    initializeApp();
});

function initializeApp() {
    log('Applicazione ESP32 Drone ID avviata', 'success');
    
    if (!navigator.serial) {
        showStatus('Web Serial API non disponibile. Usa Chrome, Edge o Opera', 'warning');
        log('Web Serial API non disponibile', 'warning');
        document.getElementById('connectBtn').disabled = true;
    } else {
        loadPorts();
    }
    
    initializeTabs();
    initializeFileInput();
    loadSavedConfigFromStorage();
}

// ==================== TAB MANAGEMENT ====================
function initializeTabs() {
    const tabButtons = document.querySelectorAll('.tab-btn');
    
    tabButtons.forEach(button => {
        button.addEventListener('click', (e) => {
            const tabName = button.getAttribute('data-tab');
            switchTab(tabName, button);
        });
    });
}

function switchTab(tabName, button) {
    const tabsContainer = button.closest('.card') || button.closest('.section');
    const allTabs = tabsContainer.querySelectorAll('.tab-btn');
    const allContents = tabsContainer.querySelectorAll('.tab-content');
    
    allTabs.forEach(tab => tab.classList.remove('active'));
    allContents.forEach(content => content.classList.remove('active'));
    
    button.classList.add('active');
    
    const content = tabsContainer.querySelector('#' + tabName + '-content');
    if (content) {
        content.classList.add('active');
    }
}

// ==================== CONFIGURATION MANAGEMENT ====================
function getConfig() {
    const config = {
        type: 'OpenDroneID',
        timestamp: new Date().toISOString(),
        version: '1.0',
        openid: {
            uasId: document.getElementById('uasId').value || '',
            operatorId: document.getElementById('operatorId').value || '',
            serialNumber: document.getElementById('serialNumber').value || '',
            manufacturerName: document.getElementById('manufacturerName').value || '',
            modelName: document.getElementById('modelName').value || '',
            protocols: {
                ble: document.getElementById('ble').checked,
                wifiBeacon: document.getElementById('wifiBeacon').checked,
                wifiNan: document.getElementById('wifiNan').checked
            }
        }
    };
    return config;
}

function setConfig(config) {
    if (!config || !config.openid) return;
    
    try {
        document.getElementById('uasId').value = config.openid.uasId || '';
        document.getElementById('operatorId').value = config.openid.operatorId || '';
        document.getElementById('serialNumber').value = config.openid.serialNumber || '';
        document.getElementById('manufacturerName').value = config.openid.manufacturerName || '';
        document.getElementById('modelName').value = config.openid.modelName || '';
        
        if (config.openid.protocols) {
            document.getElementById('ble').checked = config.openid.protocols.ble || false;
            document.getElementById('wifiBeacon').checked = config.openid.protocols.wifiBeacon || false;
            document.getElementById('wifiNan').checked = config.openid.protocols.wifiNan || false;
        }
    } catch (error) {
        console.error('Errore:', error);
        showStatus('Errore nel caricamento', 'error');
    }
}

function saveConfig() {
    const config = getConfig();
    
    if (!validateConfig(config)) {
        showStatus('Compila almeno un campo', 'warning');
        return;
    }
    
    const json = JSON.stringify(config, null, 2);
    const blob = new Blob([json], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    const timestamp = new Date().toISOString().split('T')[0];
    a.download = 'esp32-drone-config-' + timestamp + '.json';
    a.click();
    URL.revokeObjectURL(url);
    
    localStorage.setItem('esp32DroneConfig', json);
    showConfigPreview(config);
    
    showStatus('Configurazione salvata!', 'success');
    log('Configurazione salvata', 'success');
}

function loadConfig() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.json';
    input.onchange = (e) => {
        const file = e.target.files[0];
        if (!file) return;
        
        const reader = new FileReader();
        reader.onload = (event) => {
            try {
                const config = JSON.parse(event.target.result);
                setConfig(config);
                localStorage.setItem('esp32DroneConfig', event.target.result);
                showStatus('Configurazione caricata!', 'success');
                log('Configurazione caricata', 'success');
                showConfigPreview(config);
            } catch (err) {
                showStatus('Errore: file JSON non valido', 'error');
                log('Errore: ' + err.message, 'error');
            }
        };
        reader.readAsText(file);
    };
    input.click();
}

function clearConfig() {
    if (!confirm('Resettare tutti i campi?')) {
        return;
    }
    
    document.getElementById('uasId').value = '';
    document.getElementById('operatorId').value = '';
    document.getElementById('serialNumber').value = '';
    document.getElementById('manufacturerName').value = '';
    document.getElementById('modelName').value = '';
    document.getElementById('ble').checked = false;
    document.getElementById('wifiBeacon').checked = false;
    document.getElementById('wifiNan').checked = false;
    
    localStorage.removeItem('esp32DroneConfig');
    hideConfigPreview();
    
    showStatus('Configurazione resettata', 'success');
    log('Configurazione resettata', 'info');
}

function validateConfig(config) {
    if (!config.openid) return false;
    const openid = config.openid;
    return openid.uasId || openid.operatorId || openid.serialNumber || 
           openid.manufacturerName || openid.modelName;
}

function showConfigPreview(config) {
    const preview = document.getElementById('configPreview');
    const content = document.getElementById('configPreviewContent');
    
    content.textContent = JSON.stringify(config, null, 2);
    preview.style.display = 'block';
}

function hideConfigPreview() {
    document.getElementById('configPreview').style.display = 'none';
}

function loadSavedConfigFromStorage() {
    const saved = localStorage.getItem('esp32DroneConfig');
    if (saved) {
        try {
            const config = JSON.parse(saved);
            setConfig(config);
            log('Configurazione precedente ripristinata', 'info');
        } catch (error) {
            console.error('Errore:', error);
        }
    }
}

// ==================== PORT MANAGEMENT ====================
async function refreshPorts() {
    try {
        log('Ricerca porte...', 'info');
        const newPort = await navigator.serial.requestPort();
        await loadPorts();
        showStatus('Porta aggiunta!', 'success');
        log('Porta aggiunta', 'success');
    } catch (err) {
        if (err.name !== 'NotFoundError') {
            log('Errore: ' + err.message, 'error');
            showStatus('Errore: ' + err.message, 'error');
        }
    }
}

async function loadPorts() {
    try {
        const ports = await navigator.serial.getPorts();
        const select = document.getElementById('portSelect');
        
        select.innerHTML = '<option value="">-- Seleziona porta --</option>';
        
        if (ports.length === 0) {
            log('Nessuna porta trovata', 'warning');
            return;
        }

        ports.forEach((port, index) => {
            const option = document.createElement('option');
            const info = port.getInfo();
            option.value = index;
            option.textContent = 'Porta ' + (index + 1) + ' (VID: ' + (info.usbVendorId || 'N/A') + ')';
            select.appendChild(option);
        });

        log(ports.length + ' porta/e trovate', 'success');
    } catch (err) {
        log('Errore porte: ' + err.message, 'error');
    }
}

// ==================== DEVICE CONNECTION ====================
async function connectDevice() {
    const connectBtn = document.getElementById('connectBtn');
    const flashBtn = document.getElementById('flashBtn');
    
    try {
        const select = document.getElementById('portSelect');
        if (!select.value) {
            showStatus('Seleziona una porta', 'warning');
            return;
        }

        const ports = await navigator.serial.getPorts();
        const port = ports[parseInt(select.value)];
        
        if (!port) {
            showStatus('Porta non valida', 'error');
            return;
        }
        
        const baudRate = parseInt(document.getElementById('baudRate').value);
        
        log('Connessione...', 'info');
        showStatus('Connessione a ESP32...', 'info');
        
        connectBtn.disabled = true;
        connectBtn.innerHTML = 'Connessione...';

        await port.open({ baudRate: baudRate });
        
        const { ESPLoader, Transport } = window;
        transport = new Transport(port);
        esploader = new ESPLoader({
            transport: transport,
            baudrate: baudRate,
            terminal: {
                clean() {},
                writeLine(data) { log(data, 'info'); },
                write(data) { log(data, 'info'); }
            }
        });

        await esploader.main_fn();
        const chipName = await esploader.chip_name();
        const macAddr = await esploader.read_mac();
        
        log('Connesso a ' + chipName, 'success');
        log('MAC: ' + macAddr, 'success');
        
        const deviceInfo = document.getElementById('deviceInfo');
        deviceInfo.innerHTML = '<strong>Chip:</strong> ' + chipName + '<br><strong>MAC:</strong> ' + macAddr + '<br><strong>Baud:</strong> ' + baudRate;
        deviceInfo.style.display = 'block';
        
        connectBtn.innerHTML = 'Connesso';
        flashBtn.disabled = false;
        device = esploader;
        
        showStatus('Dispositivo connesso!', 'success');
    } catch (err) {
        log('Errore connessione: ' + err.message, 'error');
        showStatus('Errore: ' + err.message, 'error');
        
        connectBtn.disabled = false;
        connectBtn.innerHTML = 'Connetti ESP32';
    }
}

// ==================== FIRMWARE FLASHING ====================
function initializeFileInput() {
    const fileInput = document.getElementById('firmwareFile');
    const fileNameDisplay = document.getElementById('fileName');
    
    fileInput.addEventListener('change', (e) => {
        const file = e.target.files[0];
        if (file) {
            fileNameDisplay.textContent = file.name + ' (' + formatBytes(file.size) + ')';
            fileNameDisplay.style.fontStyle = 'normal';
            fileNameDisplay.style.color = 'var(--primary)';
            log('File: ' + file.name, 'success');
        } else {
            fileNameDisplay.textContent = 'Nessun file selezionato';
            fileNameDisplay.style.fontStyle = 'italic';
            fileNameDisplay.style.color = 'var(--text-muted)';
        }
    });
}

async function flashFirmware() {
    const flashBtn = document.getElementById('flashBtn');
    
    try {
        if (!device) {
            showStatus('Connetti ESP32 prima', 'error');
            return;
        }

        const fileInput = document.getElementById('firmwareFile');
        if (!fileInput.files.length) {
            showStatus('Seleziona un file firmware', 'error');
            return;
        }

        const file = fileInput.files[0];
        const firmware = await file.arrayBuffer();
        const addressStr = document.getElementById('flashAddress').value;
        const eraseFlash = document.getElementById('eraseFlash').checked;

        let flashAddress;
        try {
            flashAddress = parseInt(addressStr, 16);
        } catch (err) {
            showStatus('Indirizzo non valido', 'error');
            return;
        }

        log('===== FLASHING =====', 'info');
        log('File: ' + file.name + ' (' + formatBytes(file.size) + ')', 'info');
        log('Indirizzo: ' + addressStr, 'info');
        
        showStatus('Flashing...', 'info');
        showProgress(0, 'Preparazione...');
        
        flashBtn.disabled = true;
        flashBtn.innerHTML = 'Flashing...';

        if (eraseFlash) {
            log('Cancellazione flash...', 'warning');
            showProgress(10);
            await device.erase_flash();
            log('Flash cancellata', 'success');
            showProgress(30);
        } else {
            showProgress(30);
        }

        log('Scrittura firmware...', 'info');
        showProgress(40);

        const fileArray = [{
            data: new Uint8Array(firmware),
            address: flashAddress
        }];

        await device.write_flash({
            fileArray: fileArray,
            flash_size: 'keep',
            compress: true,
            reportProgress: (fileIndex, written, total) => {
                const percent = 40 + Math.floor((written / total) * 50);
                showProgress(percent);
            }
        });

        log('Firmware scritto', 'success');
        showProgress(95);

        await device.hard_reset();
        
        showProgress(100);
        log('===== SUCCESSO! =====', 'success');

        showStatus('Firmware flashato con successo!', 'success');
        
        setTimeout(() => {
            hideProgress();
        }, 3000);

    } catch (err) {
        log('ERRORE: ' + err.message, 'error');
        showStatus('Errore: ' + err.message, 'error');
        hideProgress();
    } finally {
        flashBtn.disabled = false;
        flashBtn.innerHTML = 'Flash Firmware';
    }
}

// ==================== PROGRESS BAR ====================
function showProgress(percent, label) {
    const container = document.getElementById('progressContainer');
    const fill = document.getElementById('progressFill');
    
    container.style.display = 'block';
    fill.style.width = percent + '%';
    fill.textContent = percent + '%';
}

function hideProgress() {
    document.getElementById('progressContainer').style.display = 'none';
}

// ==================== STATUS & LOGGING ====================
function showStatus(message, type) {
    const statusEl = document.getElementById('statusMessage');
    statusEl.textContent = message;
    statusEl.className = 'status-message ' + type;
    statusEl.style.display = 'block';
    
    if (type === 'success') {
        setTimeout(() => {
            statusEl.style.display = 'none';
        }, 5000);
    }
}

function log(message, type) {
    type = type || 'info';
    const logContent = document.getElementById('logContent');
    const logContainer = document.getElementById('logContainer');
    const timestamp = new Date().toLocaleTimeString();
    
    const line = document.createElement('div');
    line.className = 'log-line ' + type;
    line.textContent = '[' + timestamp + '] ' + message;
    
    logContent.appendChild(line);
    logContainer.style.display = 'block';
    logContent.scrollTop = logContent.scrollHeight;
    
    console.log('[' + type + '] ' + message);
}

function clearLogs() {
    const logContent = document.getElementById('logContent');
    logContent.innerHTML = '';
    log('Log cancellato', 'info');
}

// ==================== COMPILER HELPERS ====================
function copyCode(button, text) {
    navigator.clipboard.writeText(text).then(() => {
        const originalText = button.textContent;
        button.textContent = 'Copiato!';
        
        setTimeout(() => {
            button.textContent = originalText;
        }, 2000);
        
        log('Comando copiato', 'success');
    }).catch(err => {
        log('Errore copia: ' + err.message, 'error');
    });
}

// ==================== UTILITY FUNCTIONS ====================
function formatBytes(bytes, decimals) {
    if (bytes === 0) return '0 Bytes';
    decimals = decimals || 2;
    const k = 1024;
    const dm = decimals < 0 ? 0 : decimals;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

// ==================== SMOOTH SCROLLING ====================
document.querySelectorAll('a[href^="#"]').forEach(anchor => {
    anchor.addEventListener('click', function (e) {
        e.preventDefault();
        const target = document.querySelector(this.getAttribute('href'));
        if (target) {
            target.scrollIntoView({
                behavior: 'smooth',
                block: 'start'
            });
        }
    });
});

// ==================== KEYBOARD SHORTCUTS ====================
document.addEventListener('keydown', (e) => {
    if (e.ctrlKey && e.key === 's') {
        e.preventDefault();
        saveConfig();
    }
    if (e.ctrlKey && e.key === 'l') {
        e.preventDefault();
        loadConfig();
    }
    if (e.ctrlKey && e.key === 'k') {
        e.preventDefault();
        clearLogs();
    }
});

// ==================== EXPORT ====================
window.switchTab = switchTab;
window.saveConfig = saveConfig;
window.loadConfig = loadConfig;
window.clearConfig = clearConfig;
window.hideConfigPreview = hideConfigPreview;
window.refreshPorts = refreshPorts;
window.connectDevice = connectDevice;
window.flashFirmware = flashFirmware;
window.clearLogs = clearLogs;
window.copyCode = copyCode;

console.log('%c ESP32 Drone ID ', 'background: #667eea; color: white; font-size: 16px; font-weight: bold; padding: 10px;');
console.log('Ready to configure and flash!');
