"""Packet timing analysis tools."""

from __future__ import annotations
import time
from collections import defaultdict


class TimingAnalyzer:
    def __init__(self, window_sec: float = 30.0):
        self.window = window_sec
        self._last_seen: dict[str, float] = {}
        self._intervals: dict[str, list[float]] = defaultdict(list)
        self._packet_times: list[float] = []

    def record(self, mac: str, ts: float):
        self._packet_times.append(ts)
        if len(self._packet_times) > 10000:
            self._packet_times = self._packet_times[-5000:]

        if mac in self._last_seen:
            interval = ts - self._last_seen[mac]
            self._intervals[mac].append(interval)
            cutoff = ts - self.window
            self._intervals[mac] = [i for i in self._intervals[mac] if i >= cutoff]

        self._last_seen[mac] = ts

    def get_stats(self, mac: str) -> dict:
        intervals = self._intervals.get(mac, [])
        if not intervals:
            return {"mac": mac, "count": 0, "avg_interval": None, "rate_hz": None}
        avg_int = sum(intervals) / len(intervals)
        return {
            "mac": mac,
            "count": len(intervals),
            "avg_interval": round(avg_int, 3),
            "rate_hz": round(1.0 / avg_int, 2) if avg_int > 0 else 0,
            "min_interval": round(min(intervals), 3),
            "max_interval": round(max(intervals), 3),
        }

    def get_all_stats(self) -> list[dict]:
        macs = sorted(self._intervals.keys())
        return [self.get_stats(m) for m in macs]

    def get_global_rate(self, duration: float = 10.0) -> float:
        now = time.time()
        cutoff = now - duration
        recent = [t for t in self._packet_times if t >= cutoff]
        if not recent:
            return 0.0
        return len(recent) / duration

    def reset(self):
        self._last_seen.clear()
        self._intervals.clear()
        self._packet_times.clear()
