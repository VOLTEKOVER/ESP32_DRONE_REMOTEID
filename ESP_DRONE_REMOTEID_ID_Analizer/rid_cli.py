"""Headless CLI entries for the RID analyzer."""

from __future__ import annotations
import sys
import time
import argparse
import logging
from typing import Optional

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
)
logger = logging.getLogger("rid_cli")


def cmd_capture(args):
    """Capture and print ODID packets to stdout."""
    from ESP_DRONE_REMOTEID_ANALIZER.capture import RIDCapture, list_interfaces

    if args.list_ifaces:
        for i in list_interfaces():
            print(i)
        return

    def on_packet(data):
        ts = time.strftime("%H:%M:%S", time.localtime(data["timestamp"]))
        rssi = f"RSSI:{data['rssi']}dBm" if data.get("rssi") is not None else ""
        print(f"[{ts}] {data['source_mac']} {rssi:>10s}  {data['summary']}")

    c = RIDCapture(iface=args.iface, channel=args.channel)
    c.set_callback(on_packet)

    if args.pcap:
        c.set_pcap_output(args.pcap)

    print(f"Capturing on iface={args.iface or 'default'} ch={args.channel}")
    print("Press Ctrl+C to stop.\n")
    c.start()
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping...")
    c.stop()


def cmd_serve(args):
    """Run WebSocket server (headless)."""
    from ESP_DRONE_REMOTEID_ANALIZER.server import BroadcastServer
    from ESP_DRONE_REMOTEID_ANALIZER.capture import RIDCapture

    server = BroadcastServer(host=args.bind, port=args.port)
    server.start()

    capturer = RIDCapture(iface=args.iface, channel=args.channel)
    capturer.set_callback(server.on_capture)

    if args.pcap:
        capturer.set_pcap_output(args.pcap)

    capturer.start()
    print(f"Server on ws://{args.bind}:{args.port}")
    print(f"Capture on iface={args.iface or 'default'} ch={args.channel}")
    print("Press Ctrl+C to stop.\n")

    try:
        while True:
            time.sleep(1)
            stats = server.get_stats()
            sys.stdout.write(
                f"\r  Devices: {stats['active_devices']} active / {stats['total_devices']} total "
                f"| IDs: {stats['unique_ids']} "
                f"| Packets: {stats['packets_last_60s']}/min "
                f"| Clients: {len(server._clients)}"
            )
            sys.stdout.flush()
    except KeyboardInterrupt:
        print("\nStopping...")
    capturer.stop()
    server.stop()


def main():
    parser = argparse.ArgumentParser(description="ESP DRONE REMOTEID ANALIZER — CLI Tools")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")

    sub = parser.add_subparsers(dest="command", required=True)

    cap = sub.add_parser("capture", help="Capture ODID packets to console")
    cap.add_argument("--iface", default=None, help="WiFi interface")
    cap.add_argument("--channel", type=int, default=6, help="WiFi channel")
    cap.add_argument("--pcap", default=None, help="Save PCAP file")
    cap.add_argument("--list-ifaces", action="store_true", help="List interfaces and exit")
    cap.set_defaults(func=cmd_capture)

    srv = sub.add_parser("serve", help="Run WebSocket server")
    srv.add_argument("--iface", default=None, help="WiFi interface")
    srv.add_argument("--channel", type=int, default=6, help="WiFi channel")
    srv.add_argument("--port", type=int, default=8765, help="WebSocket port")
    srv.add_argument("--bind", default="127.0.0.1", help="Bind address")
    srv.add_argument("--pcap", default=None, help="Save PCAP file")
    srv.set_defaults(func=cmd_serve)

    args = parser.parse_args()
    if args.verbose:
        for handler in logging.root.handlers:
            handler.setLevel(logging.DEBUG)
        logging.root.setLevel(logging.DEBUG)

    args.func(args)


if __name__ == "__main__":
    main()
