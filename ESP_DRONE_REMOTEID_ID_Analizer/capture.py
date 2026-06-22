"""WiFi Remote ID beacon capture via Scapy (with monitor mode helper)."""

from __future__ import annotations
import time
import threading
import logging
from typing import Optional, Callable

from ESP_DRONE_REMOTEID_ANALIZER.decoder import extract_odid_from_beacon, format_summary

logger = logging.getLogger(__name__)

try:
    from scapy.all import sniff, Dot11, Dot11Beacon, Dot11Elt, conf
    SCAPY_AVAILABLE = True
except ImportError:
    SCAPY_AVAILABLE = False


MONITOR_MODE_HELP = {
    "linux": (
        "Set interface to monitor mode:\n"
        "  sudo ip link set <iface> down\n"
        "  sudo iw dev <iface> set type monitor\n"
        "  sudo ip link set <iface> up"
    ),
    "darwin": (
        "Set interface to monitor mode (macOS):\n"
        "  sudo ifconfig <iface> down\n"
        "  sudo ifconfig <iface> up"
    ),
    "win32": (
        "On Windows monitor mode requires:\n"
        "  - Npcap installed with '802.11 monitor mode' option\n"
        "  - A compatible WiFi adapter\n"
        "  OR use a regular interface (beacon frames still work)\n"
        "  See: https://npcap.com/guide/npcap-tutorial.html"
    ),
}


class RIDCapture:
    def __init__(self, iface: Optional[str] = None, channel: int = 6):
        self.iface = iface
        self.channel = channel
        self._running = False
        self._thread: Optional[threading.Thread] = None
        self._callback: Optional[Callable] = None
        self._sniffer = None
        self._pcap_writer = None
        self._pcap_file: Optional[str] = None

    @property
    def is_supported(self) -> bool:
        return SCAPY_AVAILABLE

    def set_callback(self, cb: Callable):
        self._callback = cb

    def set_pcap_output(self, path: str):
        self._pcap_file = path

    def start(self):
        if not SCAPY_AVAILABLE:
            logger.error("Scapy not installed. Run: pip install scapy")
            return
        if self._running:
            return
        self._running = True
        self._thread = threading.Thread(target=self._capture_loop, daemon=True)
        self._thread.start()
        logger.info(f"Capture started on iface={self.iface} ch={self.channel}")

    def stop(self):
        self._running = False
        if self._thread:
            self._thread.join(timeout=3)
        if self._pcap_writer:
            self._pcap_writer.close()
        logger.info("Capture stopped")

    def _process_packet(self, pkt):
        if not self._running:
            return
        try:
            if not pkt.haslayer(Dot11) or not pkt.haslayer(Dot11Beacon):
                return

            rssi = None
            if hasattr(pkt, "dBm_AntSignal"):
                rssi = pkt.dBm_AntSignal
            elif pkt.notdecoded and len(pkt.notdecoded) >= 4:
                try:
                    rssi = -256 + pkt.notdecoded[-4]
                except (ValueError, TypeError):
                    pass

            src_mac = pkt.addr2 if pkt.addr2 else "?"

            beacon = pkt[Dot11Beacon]
            frame_body = bytes(beacon)

            odid_msgs = extract_odid_from_beacon(frame_body)
            if not odid_msgs:
                return

            ts = time.time()
            summary = format_summary(odid_msgs)

            data = {
                "timestamp": ts,
                "source_mac": src_mac,
                "rssi": rssi,
                "channel": self.channel,
                "summary": summary,
                "messages": odid_msgs,
            }

            if self._callback:
                try:
                    self._callback(data)
                except Exception as e:
                    logger.error(f"Callback error: {e}")

        except Exception:
            pass

    def _capture_loop(self):
        kwargs = {"prn": self._process_packet, "store": 0}
        if self.iface:
            kwargs["iface"] = self.iface
        beacon_filter = "type mgt subtype beacon"
        kwargs["filter"] = beacon_filter

        try:
            self._sniffer = sniff(**kwargs)
        except PermissionError as e:
            logger.error(f"Permission denied: {e}. Try running as admin/root.")
        except Exception as e:
            logger.error(f"Capture error: {e}")


def list_interfaces() -> list[str]:
    if not SCAPY_AVAILABLE:
        return []
    try:
        from scapy.all import get_if_list
        return get_if_list()
    except Exception:
        return []


def check_monitor_mode(iface: str) -> bool:
    import platform
    if platform.system().lower() == "linux":
        try:
            import subprocess
            r = subprocess.run(
                ["iw", "dev", iface, "info"],
                capture_output=True, text=True, timeout=3
            )
            return "type monitor" in r.stdout
        except Exception:
            return False
    elif platform.system().lower() == "win32":
        return False
    return False


def print_interface_help(iface: str):
    import platform
    os_name = platform.system().lower()
    help_text = MONITOR_MODE_HELP.get(os_name, "See your OS documentation for monitor mode setup.")
    print(f"\nMonitor mode for '{iface}':")
    print(help_text)
    print()
