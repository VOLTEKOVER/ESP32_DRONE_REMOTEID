"""WebSocket server — bridge between capture engine and Web UI."""

from __future__ import annotations
import json
import time
import threading
import logging
from typing import Optional, Callable
from dataclasses import dataclass, field

logger = logging.getLogger(__name__)

try:
    from simple_websocket_server import WebSocket, WebSocketServer
    WS_AVAILABLE = True
except ImportError:
    WS_AVAILABLE = False


@dataclass
class RIDDevice:
    mac: str
    first_seen: float = 0.0
    last_seen: float = 0.0
    rssi_samples: list = field(default_factory=list)
    basic_id: str = ""
    operator_id: str = ""
    ua_type: int = 0
    last_location: Optional[dict] = None
    last_system: Optional[dict] = None
    packet_count: int = 0
    messages_seen: set = field(default_factory=set)

    @property
    def avg_rssi(self) -> Optional[float]:
        if not self.rssi_samples:
            return None
        return sum(self.rssi_samples) / len(self.rssi_samples)

    def to_dict(self) -> dict:
        return {
            "mac": self.mac,
            "basic_id": self.basic_id,
            "operator_id": self.operator_id,
            "ua_type": self.ua_type,
            "packet_count": self.packet_count,
            "rssi_avg": round(self.avg_rssi, 1) if self.rssi_samples else None,
            "rssi_last": self.rssi_samples[-1] if self.rssi_samples else None,
            "first_seen": self.first_seen,
            "last_seen": self.last_seen,
            "last_location": self.last_location,
            "last_system": self.last_system,
        }


class RIDWebSocket(WebSocket):
    _broadcaster: Optional['BroadcastServer'] = None

    def handle(self):
        pass

    def connected(self):
        if self._broadcaster:
            self._broadcaster._clients.add(self)

    def handle_close(self):
        if self._broadcaster:
            self._broadcaster._clients.discard(self)


class BroadcastServer:
    def __init__(self, host: str = "127.0.0.1", port: int = 8765):
        self.host = host
        self.port = port
        self._clients: set = set()
        self._server: Optional[WebSocketServer] = None
        self._thread: Optional[threading.Thread] = None
        self._devices: dict[str, RIDDevice] = {}
        self._packet_history: list = []
        self._history_lock = threading.Lock()
        self._stats_lock = threading.Lock()

    @property
    def ws_url(self) -> str:
        return f"ws://{self.host}:{self.port}"

    def start(self):
        if not WS_AVAILABLE:
            logger.error("simple_websocket_server not installed. Run: pip install simple-websocket-server")
            return
        RIDWebSocket._broadcaster = self
        self._server = WebSocketServer(self.host, self.port, RIDWebSocket)
        self._thread = threading.Thread(target=self._server.serve_forever, daemon=True)
        self._thread.start()
        logger.info(f"WebSocket server on {self.ws_url}")

    def stop(self):
        if self._server:
            self._server.close()
        logger.info("WebSocket server stopped")

    def on_capture(self, data: dict):
        mac = data.get("source_mac", "?")
        ts = data.get("timestamp", time.time())

        with self._stats_lock:
            if mac not in self._devices:
                self._devices[mac] = RIDDevice(mac=mac, first_seen=ts)
            dev = self._devices[mac]
            dev.last_seen = ts
            dev.packet_count += 1

            rssi = data.get("rssi")
            if rssi is not None:
                dev.rssi_samples.append(rssi)
                if len(dev.rssi_samples) > 200:
                    dev.rssi_samples = dev.rssi_samples[-200:]

            for msg in data.get("messages", []):
                dec = msg.get("decoded", {})
                t = dec.get("type", "?")
                dev.messages_seen.add(t)
                if t == "Basic ID":
                    uid = dec.get("uas_id", "")
                    if uid:
                        dev.basic_id = uid
                    ut = dec.get("ua_type", 0)
                    if ut:
                        dev.ua_type = ut
                elif t == "Operator ID":
                    oid = dec.get("operator_id", "")
                    if oid:
                        dev.operator_id = oid
                elif t == "Location":
                    dev.last_location = dec
                elif t == "System":
                    dev.last_system = dec

        clean_data = {
            "ts": ts,
            "mac": mac,
            "rssi": rssi,
            "summary": data.get("summary", ""),
        }
        with self._history_lock:
            self._packet_history.append(clean_data)
            if len(self._packet_history) > 1000:
                self._packet_history = self._packet_history[-1000:]

        self._broadcast({
            "type": "packet",
            "data": clean_data,
        })

    def _broadcast(self, msg: dict):
        payload = json.dumps(msg, default=str)
        dead = set()
        for client in self._clients.copy():
            try:
                client.send_message(payload)
            except Exception:
                dead.add(client)
        self._clients -= dead

    def get_devices_snapshot(self) -> list[dict]:
        with self._stats_lock:
            return [
                dev.to_dict() for dev in sorted(
                    self._devices.values(),
                    key=lambda d: d.last_seen,
                    reverse=True
                )
            ]

    def get_stats(self) -> dict:
        now = time.time()
        with self._stats_lock:
            total = len(self._devices)
            active = sum(1 for d in self._devices.values() if now - d.last_seen < 30)
            basic_ids = set(d.basic_id for d in self._devices.values() if d.basic_id)
        with self._history_lock:
            recent = len([p for p in self._packet_history if now - p["ts"] < 60])
        return {
            "total_devices": total,
            "active_devices": active,
            "unique_ids": len(basic_ids),
            "packets_last_60s": recent,
            "total_packets": len(self._packet_history),
        }

    def reset_stats(self):
        with self._stats_lock:
            self._devices.clear()
        with self._history_lock:
            self._packet_history.clear()
        self._broadcast({"type": "reset"})
        logger.info("Stats reset")
