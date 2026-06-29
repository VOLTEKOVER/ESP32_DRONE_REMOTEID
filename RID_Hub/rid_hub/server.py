"""WebSocket server — bridge between capture engine and Web UI.

Supports multi-channel scanning, session recording, CSV/KML export,
drone track trails, and full command dispatch.
"""

from __future__ import annotations
import json
import time
import csv
import io
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
    # Stub so the module can be imported without simple_websocket_server
    WebSocket = object
    WebSocketServer = object


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
    location_trail: list = field(default_factory=list)
    self_id: str = ""

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
            "location_trail": self.location_trail[-10:],
            "self_id": self.self_id,
        }

    def to_detail_dict(self) -> dict:
        return {
            "mac": self.mac,
            "basic_id": self.basic_id,
            "operator_id": self.operator_id,
            "ua_type": self.ua_type,
            "self_id": self.self_id,
            "packet_count": self.packet_count,
            "rssi_avg": round(self.avg_rssi, 1) if self.rssi_samples else None,
            "rssi_last": self.rssi_samples[-1] if self.rssi_samples else None,
            "rssi_min": min(self.rssi_samples) if self.rssi_samples else None,
            "rssi_max": max(self.rssi_samples) if self.rssi_samples else None,
            "rssi_samples": self.rssi_samples[-100:],
            "first_seen": self.first_seen,
            "last_seen": self.last_seen,
            "last_location": self.last_location,
            "last_system": self.last_system,
            "location_trail": self.location_trail,
            "messages_seen": sorted(self.messages_seen),
            "packet_count": self.packet_count,
        }


class RIDWebSocket(WebSocket):
    _broadcaster: Optional['BroadcastServer'] = None

    def handle(self):
        msg = getattr(self, "data", "")
        if msg and self._broadcaster:
            self._broadcaster._handle_message(msg, self)

    def connected(self):
        if self._broadcaster:
            self._broadcaster._clients.add(self)
            # send current state snapshot on connect
            snapshot = self._broadcaster._get_snapshot()
            try:
                self.send_message(json.dumps(snapshot, default=str))
            except Exception:
                pass

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

        self._session_recording = False
        self._session_packets: list = []
        self._session_start: float = 0.0

    @property
    def ws_url(self) -> str:
        return f"ws://{self.host}:{self.port}"

    def start(self):
        if not WS_AVAILABLE:
            logger.error("simple_websocket_server not installed.")
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

    def _get_snapshot(self) -> dict:
        return {
            "type": "snapshot",
            "devices": self.get_devices_snapshot(),
            "stats": self.get_stats(),
        }

    def _handle_message(self, msg: str, client: RIDWebSocket):
        try:
            data = json.loads(msg)
        except json.JSONDecodeError:
            return
        cmd = data.get("command", "")
        if cmd == "reset":
            self.reset_stats()
        elif cmd == "session_start":
            self._session_start_recording()
        elif cmd == "session_stop":
            self._session_stop_recording()
        elif cmd == "get_csv":
            csv_text = self._generate_csv()
            self._send_to(client, {"type": "csv_data", "data": csv_text})
        elif cmd == "get_kml":
            kml_text = self._generate_kml()
            self._send_to(client, {"type": "kml_data", "data": kml_text})
        elif cmd == "get_session":
            self._send_to(client, {"type": "session_data", "data": self._session_packets})
        elif cmd == "get_device_detail":
            mac = data.get("mac", "")
            with self._stats_lock:
                dev = self._devices.get(mac)
            if dev:
                self._send_to(client, {"type": "device_detail", "data": dev.to_detail_dict()})
            else:
                self._send_to(client, {"type": "device_detail", "data": None})
        elif cmd == "get_replay":
            session = data.get("session", [])
            for pkt in session:
                self._send_to(client, {"type": "replay_packet", "data": pkt})
            self._send_to(client, {"type": "replay_done"})

    def _send_to(self, client: RIDWebSocket, msg: dict):
        try:
            client.send_message(json.dumps(msg, default=str))
        except Exception:
            self._clients.discard(client)

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
                if len(dev.rssi_samples) > 500:
                    dev.rssi_samples = dev.rssi_samples[-500:]

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
                    loc = dec
                    dev.last_location = loc
                    lat = loc.get("latitude")
                    lon = loc.get("longitude")
                    if lat is not None and lon is not None:
                        trail_entry = {"lat": lat, "lon": lon, "ts": int(ts)}
                        if not dev.location_trail or dev.location_trail[-1]["lat"] != lat or dev.location_trail[-1]["lon"] != lon:
                            dev.location_trail.append(trail_entry)
                            if len(dev.location_trail) > 500:
                                dev.location_trail = dev.location_trail[-500:]
                elif t == "System":
                    dev.last_system = dec
                elif t == "Self ID":
                    desc = dec.get("description", "")
                    if desc:
                        dev.self_id = desc

        clean_data = {
            "ts": ts,
            "mac": mac,
            "rssi": rssi,
            "summary": data.get("summary", ""),
            "channel": data.get("channel", 0),
        }
        with self._history_lock:
            self._packet_history.append(clean_data)
            if len(self._packet_history) > 2000:
                self._packet_history = self._packet_history[-2000:]

        if self._session_recording:
            self._session_packets.append(clean_data)

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
            "recording": self._session_recording,
            "session_packets": len(self._session_packets),
        }

    def reset_stats(self):
        with self._stats_lock:
            self._devices.clear()
        with self._history_lock:
            self._packet_history.clear()
        self._session_packets.clear()
        self._session_recording = False
        self._session_start = 0.0
        self._broadcast({"type": "reset"})
        logger.info("Stats reset")

    def _session_start_recording(self):
        self._session_packets.clear()
        self._session_recording = True
        self._session_start = time.time()
        self._broadcast({"type": "session_status", "data": {"recording": True, "packets": 0}})
        logger.info("Session recording started")

    def _session_stop_recording(self):
        self._session_recording = False
        self._broadcast({
            "type": "session_status",
            "data": {"recording": False, "packets": len(self._session_packets)},
        })
        logger.info(f"Session recording stopped ({len(self._session_packets)} packets)")

    def _generate_csv(self) -> str:
        output = io.StringIO()
        writer = csv.writer(output)
        writer.writerow([
            "MAC", "Basic ID", "UA Type", "Operator ID",
            "Latitude", "Longitude", "RSSI Avg", "RSSI Last",
            "First Seen", "Last Seen", "Packet Count", "Self ID"
        ])
        now = time.time()
        with self._stats_lock:
            for dev in self._devices.values():
                lat = ""
                lon = ""
                if dev.last_location:
                    lat = dev.last_location.get("latitude", "")
                    lon = dev.last_location.get("longitude", "")
                writer.writerow([
                    dev.mac,
                    dev.basic_id,
                    dev.ua_type,
                    dev.operator_id,
                    lat,
                    lon,
                    round(dev.avg_rssi, 1) if dev.rssi_samples else "",
                    dev.rssi_samples[-1] if dev.rssi_samples else "",
                    time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(dev.first_seen)),
                    time.strftime("%Y-%m-%d %H:%M:%S", time.gmtime(dev.last_seen)),
                    dev.packet_count,
                    dev.self_id,
                ])
        return output.getvalue()

    def _generate_kml(self) -> str:
        kml_parts = [
            '<?xml version="1.0" encoding="UTF-8"?>',
            '<kml xmlns="http://www.opengis.net/kml/2.2">',
            '<Document>',
            '<name>RID Hub — Drone Report</name>',
        ]
        with self._stats_lock:
            for dev in self._devices.values():
                name = dev.basic_id or dev.mac
                kml_parts.append(f'<Placemark><name>{name}</name>')
                kml_parts.append(f'<description>MAC: {dev.mac}\\nOperator: {dev.operator_id}\\nType: {dev.ua_type}\\nPackets: {dev.packet_count}\\nAvg RSSI: {round(dev.avg_rssi,1) if dev.rssi_samples else "N/A"} dBm</description>')
                coords = ""
                if dev.location_trail:
                    for pt in dev.location_trail:
                        coords += f"{pt['lon']},{pt['lat']},{pt['ts']} "
                    kml_parts.append('<LineString><extrude>1</extrude><tessellate>1</tessellate>')
                    kml_parts.append(f'<coordinates>{coords.strip()}</coordinates>')
                    kml_parts.append('</LineString>')
                if dev.last_location:
                    lat = dev.last_location.get("latitude", 0)
                    lon = dev.last_location.get("longitude", 0)
                    kml_parts.append('<Point>')
                    kml_parts.append(f'<coordinates>{lon},{lat},0</coordinates>')
                    kml_parts.append('</Point>')
                kml_parts.append('</Placemark>')
        kml_parts.append('</Document>')
        kml_parts.append('</kml>')
        return "\n".join(kml_parts)
