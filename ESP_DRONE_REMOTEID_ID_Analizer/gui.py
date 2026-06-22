"""pywebview native window — wraps the Web UI in a desktop app."""

from __future__ import annotations
import os
import sys
import json
import time
import threading
import logging
from pathlib import Path
from http.server import HTTPServer, SimpleHTTPRequestHandler

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger("rid_gui")


WEB_DIR = Path(__file__).parent / "web"


def run_backend(ws_port: int, capture_iface: str, capture_channel: int):
    from ESP_DRONE_REMOTEID_ANALIZER.server import BroadcastServer
    from ESP_DRONE_REMOTEID_ANALIZER.capture import RIDCapture, list_interfaces, check_monitor_mode, print_interface_help

    server = BroadcastServer(port=ws_port)
    server.start()
    time.sleep(0.3)

    capturer = RIDCapture(iface=capture_iface or None, channel=capture_channel)
    if not capturer.is_supported:
        logger.warning("Scapy not available. Install: pip install scapy")

    capturer.set_callback(server.on_capture)
    capturer.start()

    return server, capturer


def start_http_server(web_dir: Path, port: int):
    os.chdir(str(web_dir))
    server = HTTPServer(("127.0.0.1", port), SimpleHTTPRequestHandler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server


def main():
    import argparse

    parser = argparse.ArgumentParser(description="ESP DRONE REMOTEID ANALIZER")
    parser.add_argument("--port", type=int, default=8765, help="WebSocket port (default: 8765)")
    parser.add_argument("--http-port", type=int, default=8080, help="HTTP server port (default: 8080)")
    parser.add_argument("--iface", default=None, help="WiFi interface for capture")
    parser.add_argument("--channel", type=int, default=6, help="WiFi channel (default: 6)")
    parser.add_argument("--no-gui", action="store_true", help="Run without GUI (headless)")
    parser.add_argument("--list-ifaces", action="store_true", help="List available WiFi interfaces and exit")
    parser.add_argument("--export", default=None, help="Export directory for PCAP files")
    args = parser.parse_args()

    if args.list_ifaces:
        from ESP_DRONE_REMOTEID_ANALIZER.capture import list_interfaces
        ifaces = list_interfaces()
        print("Available interfaces:")
        for i in ifaces:
            print(f"  {i}")
        sys.exit(0)

    try:
        import webview
        PYWEBVIEW_OK = True
    except ImportError:
        PYWEBVIEW_OK = False
        if not args.no_gui:
            print("pywebview not installed. Run: pip install pywebview\nFalling back to headless mode.")
            args.no_gui = True

    httpd = start_http_server(WEB_DIR, args.http_port)
    logger.info("HTTP server on http://127.0.0.1:%d", args.http_port)

    server, capturer = run_backend(args.port, args.iface, args.channel)
    time.sleep(0.5)

    if args.no_gui:
        logger.info("Running in headless mode. WebSocket on ws://127.0.0.1:%d", args.port)
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            pass
    else:
        window = webview.create_window(
            title="ESP DRONE REMOTEID ANALIZER",
            url=f"http://127.0.0.1:{args.http_port}/index.html",
            width=1280,
            height=860,
            resizable=True,
            text_select=True,
        )
        webview.start(private_mode=True)

    capturer.stop()
    server.stop()
    httpd.shutdown()


if __name__ == "__main__":
    main()
