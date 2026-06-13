"""RSSI-based distance estimation."""

from __future__ import annotations
import math
from typing import Optional

# Path loss exponent for 2.4 GHz
# Free space: 2.0 | Indoor LOS: 2.0-2.5 | Indoor obstructed: 3.0-4.0
# Outdoor: 2.5-3.5 | Outdoor urban: 3.0-5.0
PATH_LOSS_EXPONENT = 2.5
REF_RSSI_DBM_1M = -40  # Typical RSSI at 1 meter for WiFi


def estimate_distance(
    rssi_dbm: float,
    ref_rssi: float = REF_RSSI_DBM_1M,
    n: float = PATH_LOSS_EXPONENT,
) -> float:
    if rssi_dbm > ref_rssi:
        return 1.0
    return 10 ** ((ref_rssi - rssi_dbm) / (10 * n))


def estimate_distance_range(rssi_dbm: float) -> tuple[float, float]:
    d_nominal = estimate_distance(rssi_dbm)
    d_optimistic = estimate_distance(rssi_dbm, n=2.0)
    d_pessimistic = estimate_distance(rssi_dbm, n=4.0)
    return round(min(d_optimistic, d_pessimistic), 1), round(max(d_optimistic, d_pessimistic), 1)


class RangeEstimator:
    def __init__(self, ref_rssi: float = REF_RSSI_DBM_1M, n: float = PATH_LOSS_EXPONENT):
        self.ref_rssi = ref_rssi
        self.n = n

    def estimate(self, rssi_dbm: float) -> float:
        return round(estimate_distance(rssi_dbm, self.ref_rssi, self.n), 1)

    def estimate_bounds(self, rssi_dbm: float) -> dict:
        lo, hi = estimate_distance_range(rssi_dbm)
        return {
            "rssi_dbm": rssi_dbm,
            "distance_nominal": self.estimate(rssi_dbm),
            "distance_min": lo,
            "distance_max": hi,
            "unit": "m",
        }


RSSI_QUALITY = [
    (-30, "Amazing", "rssi-ok"),
    (-50, "Excellent", "rssi-ok"),
    (-65, "Good", "rssi-warn"),
    (-80, "Fair", "rssi-warn"),
    (-90, "Poor", "rssi-bad"),
    (-100, "Unusable", "rssi-bad"),
]


def rssi_quality(rssi_dbm: float) -> tuple[str, str]:
    for threshold, label, css_class in RSSI_QUALITY:
        if rssi_dbm >= threshold:
            return label, css_class
    return "No signal", "rssi-bad"
