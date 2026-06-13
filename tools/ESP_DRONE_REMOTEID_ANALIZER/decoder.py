"""Pure Python Open Drone ID (ASTM F3411-22a) packet decoder."""

from __future__ import annotations
import struct
from dataclasses import dataclass, field
from typing import Optional


# Message type constants
ODID_MESSAGE_BASIC_ID = 0x00
ODID_MESSAGE_LOCATION = 0x01
ODID_MESSAGE_AUTH = 0x02
ODID_MESSAGE_SELF_ID = 0x03
ODID_MESSAGE_SYSTEM = 0x04
ODID_MESSAGE_OPERATOR_ID = 0x05
ODID_MESSAGE_PACK = 0xFF

# ID types
ID_TYPE_NONE = 0
ID_TYPE_SERIAL = 1
ID_TYPE_CAA = 2
ID_TYPE_UTM = 3
ID_TYPE_SESSION = 4

ID_TYPE_NAMES = {
    ID_TYPE_NONE: "None",
    ID_TYPE_SERIAL: "Serial Number (ANSI/CTA-2063)",
    ID_TYPE_CAA: "CAA Registration",
    ID_TYPE_UTM: "UTM UUID",
    ID_TYPE_SESSION: "Session ID",
}

# UA types
UA_TYPE_NONE = 0
UA_TYPE_AEROPLANE = 1
UA_TYPE_HELICOPTER = 2
UA_TYPE_GYROPLANE = 3
UA_TYPE_HYBRID_LIFT = 4
UA_TYPE_ORITHOPTER = 5
UA_TYPE_FIXED_WING = 6
UA_TYPE_ROTORCRAFT = 7
UA_TYPE_VTOL = 8
UA_TYPE_OTHER = 15

UA_TYPE_NAMES = {
    UA_TYPE_NONE: "None / Not specified",
    UA_TYPE_AEROPLANE: "Aeroplane",
    UA_TYPE_HELICOPTER: "Helicopter",
    UA_TYPE_GYROPLANE: "Gyroplane",
    UA_TYPE_HYBRID_LIFT: "Hybrid Lift / Multirotor",
    UA_TYPE_ORITHOPTER: "Ornithopter",
    UA_TYPE_FIXED_WING: "Fixed Wing",
    UA_TYPE_ROTORCRAFT: "Rotorcraft",
    UA_TYPE_VTOL: "VTOL",
    UA_TYPE_OTHER: "Other",
}

# Status codes
ODID_STATUS_UNDECLARED = 0
ODID_STATUS_GROUND = 1
ODID_STATUS_AIRBORNE = 2
ODID_STATUS_EMERGENCY = 3
ODID_STATUS_REMOTE_ID_SYSTEM_FAILURE = 4

STATUS_NAMES = {
    ODID_STATUS_UNDECLARED: "Undeclared",
    ODID_STATUS_GROUND: "On Ground",
    ODID_STATUS_AIRBORNE: "Airborne",
    ODID_STATUS_EMERGENCY: "Emergency",
    ODID_STATUS_REMOTE_ID_SYSTEM_FAILURE: "System Failure",
}

# Description types
DESC_TYPE_NONE = 0
DESC_TYPE_TEXT = 1
DESC_TYPE_EMERGENCY = 2
DESC_TYPE_EXTENDED_STATUS = 3

DESC_TYPE_NAMES = {
    DESC_TYPE_NONE: "None",
    DESC_TYPE_TEXT: "Free Text",
    DESC_TYPE_EMERGENCY: "Emergency",
    DESC_TYPE_EXTENDED_STATUS: "Extended Status",
}

# Auth types / pages
AUTH_PAGE_NONE = 0x00
AUTH_PAGE_DRIP = 0x02


@dataclass
class BasicIDMessage:
    id_type: int = 0
    ua_type: int = 0
    uas_id: str = ""

    def to_dict(self) -> dict:
        return {
            "type": "Basic ID",
            "id_type": self.id_type,
            "id_type_name": ID_TYPE_NAMES.get(self.id_type, f"Unknown ({self.id_type})"),
            "ua_type": self.ua_type,
            "ua_type_name": UA_TYPE_NAMES.get(self.ua_type, f"Unknown ({self.ua_type})"),
            "uas_id": self.uas_id,
        }


@dataclass
class LocationMessage:
    status: int = 0
    direction: float = 0.0
    speed_horizontal: float = 0.0
    speed_vertical: float = 0.0
    latitude: float = 0.0
    longitude: float = 0.0
    altitude_pressure: float = 0.0
    altitude_geodetic: float = 0.0
    height: float = 0.0
    timestamp: float = 0.0

    def to_dict(self) -> dict:
        return {
            "type": "Location",
            "status": self.status,
            "status_name": STATUS_NAMES.get(self.status, f"Unknown ({self.status})"),
            "direction": self.direction,
            "speed_horizontal": self.speed_horizontal,
            "speed_vertical": self.speed_vertical,
            "latitude": self.latitude,
            "longitude": self.longitude,
            "altitude_pressure": self.altitude_pressure,
            "altitude_geodetic": self.altitude_geodetic,
            "height": self.height,
            "timestamp": self.timestamp,
        }


@dataclass
class SystemMessage:
    operator_lat: float = 0.0
    operator_lon: float = 0.0
    area_count: int = 0
    area_radius: int = 0
    operator_alt_geo: float = 0.0
    timestamp: int = 0

    def to_dict(self) -> dict:
        return {
            "type": "System",
            "operator_latitude": self.operator_lat,
            "operator_longitude": self.operator_lon,
            "area_count": self.area_count,
            "area_radius": self.area_radius,
            "operator_altitude_geodetic": self.operator_alt_geo,
            "timestamp": self.timestamp,
        }


@dataclass
class OperatorIDMessage:
    operator_id: str = ""

    def to_dict(self) -> dict:
        return {
            "type": "Operator ID",
            "operator_id": self.operator_id,
        }


@dataclass
class SelfIDMessage:
    desc_type: int = 0
    description: str = ""

    def to_dict(self) -> dict:
        return {
            "type": "Self ID",
            "desc_type": self.desc_type,
            "desc_type_name": DESC_TYPE_NAMES.get(self.desc_type, f"Unknown ({self.desc_type})"),
            "description": self.description,
        }


@dataclass
class AuthMessage:
    auth_type: int = 0
    auth_page: int = 0
    auth_data: bytes = field(default_factory=bytes)

    def to_dict(self) -> dict:
        return {
            "type": "Authentication",
            "auth_type": self.auth_type,
            "auth_page": self.auth_page,
            "auth_data_hex": self.auth_data.hex(),
            "auth_data_len": len(self.auth_data),
        }


@dataclass
class MessagePack:
    messages: list = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "type": "Message Pack",
            "count": len(self.messages),
            "messages": [m.to_dict() for m in self.messages],
        }


ODID_MESSAGE_HEADER = struct.Struct("<BB")
SYSTEM_FIXED = struct.Struct("<iiHHhI")
BASIC_ID_FIXED = struct.Struct("<BB20s")
OPERATOR_ID_FIXED = struct.Struct("<20s")
SELF_ID_FIXED = struct.Struct("<23s")


def decode_basic_id(data: bytes, offset: int = 0) -> BasicIDMessage:
    msg = BasicIDMessage()
    if len(data) - offset < 22:
        return msg
    raw = BASIC_ID_FIXED.unpack_from(data, offset)
    msg.id_type = raw[0]
    msg.ua_type = raw[1]
    raw_id = raw[2]
    msg.uas_id = raw_id.split(b"\x00")[0].decode("utf-8", errors="replace")
    return msg


def decode_location(data: bytes, offset: int = 0) -> LocationMessage:
    msg = LocationMessage()
    if len(data) - offset < 26:
        return msg
    msg.status = struct.unpack_from("<B", data, offset)[0]; offset += 1
    _reserved = struct.unpack_from("<B", data, offset)[0]; offset += 1  # noqa
    msg.direction = struct.unpack_from("<H", data, offset)[0] / 100.0; offset += 2
    msg.speed_horizontal = struct.unpack_from("<H", data, offset)[0] * 0.25; offset += 2
    msg.speed_vertical = struct.unpack_from("<h", data, offset)[0] * 0.25; offset += 2
    msg.latitude = struct.unpack_from("<i", data, offset)[0] / 1e7; offset += 4
    msg.longitude = struct.unpack_from("<i", data, offset)[0] / 1e7; offset += 4
    msg.altitude_pressure = struct.unpack_from("<h", data, offset)[0] / 2.0 - 1000.0; offset += 2
    msg.altitude_geodetic = struct.unpack_from("<h", data, offset)[0] / 2.0 - 1000.0; offset += 2
    msg.height = struct.unpack_from("<h", data, offset)[0] / 2.0 - 1000.0; offset += 2
    msg.timestamp = struct.unpack_from("<I", data, offset)[0] / 10.0
    return msg


def decode_system(data: bytes, offset: int = 0) -> SystemMessage:
    msg = SystemMessage()
    if len(data) - offset < SYSTEM_FIXED.size:
        return msg
    raw = SYSTEM_FIXED.unpack_from(data, offset)
    msg.operator_lat = raw[0] / 1e7
    msg.operator_lon = raw[1] / 1e7
    msg.area_count = raw[2]
    msg.area_radius = raw[3]
    msg.operator_alt_geo = raw[4] / 2.0 - 1000.0
    msg.timestamp = raw[5]
    return msg


def decode_operator_id(data: bytes, offset: int = 0) -> OperatorIDMessage:
    msg = OperatorIDMessage()
    if len(data) - offset < 20:
        return msg
    raw_id = data[offset : offset + 20]
    msg.operator_id = raw_id.split(b"\x00")[0].decode("utf-8", errors="replace")
    return msg


def decode_self_id(data: bytes, offset: int = 0) -> SelfIDMessage:
    msg = SelfIDMessage()
    if len(data) - offset < 23:
        return msg
    msg.desc_type = data[offset]
    raw_desc = data[offset + 1 : offset + 23]
    msg.description = raw_desc.split(b"\x00")[0].decode("utf-8", errors="replace")
    return msg


def decode_auth(data: bytes, offset: int = 0) -> AuthMessage:
    msg = AuthMessage()
    if len(data) - offset < 2:
        return msg
    msg.auth_type = data[offset]
    msg.auth_page = data[offset + 1]
    msg.auth_data = data[offset + 2:]
    return msg


DECODERS = {
    ODID_MESSAGE_BASIC_ID: decode_basic_id,
    ODID_MESSAGE_LOCATION: decode_location,
    ODID_MESSAGE_SYSTEM: decode_system,
    ODID_MESSAGE_OPERATOR_ID: decode_operator_id,
    ODID_MESSAGE_SELF_ID: decode_self_id,
    ODID_MESSAGE_AUTH: decode_auth,
}

MESSAGE_TYPE_NAMES = {
    ODID_MESSAGE_BASIC_ID: "Basic ID",
    ODID_MESSAGE_LOCATION: "Location/Vector",
    ODID_MESSAGE_SYSTEM: "System",
    ODID_MESSAGE_OPERATOR_ID: "Operator ID",
    ODID_MESSAGE_SELF_ID: "Self ID",
    ODID_MESSAGE_AUTH: "Authentication",
    ODID_MESSAGE_PACK: "Message Pack",
}


def decode_odid_message(data: bytes) -> dict:
    if len(data) < 2:
        return {"type": "Error", "error": "Too short"}
    msg_type = data[0]
    _protocol_version = data[1]

    decoded = None
    decoder = DECODERS.get(msg_type)
    if decoder:
        decoded = decoder(data, 2)

    name = MESSAGE_TYPE_NAMES.get(msg_type, f"Unknown (0x{msg_type:02x})")

    result = {
        "message_type": msg_type,
        "protocol_version": _protocol_version,
        "message_name": name,
    }
    if decoded:
        result["decoded"] = decoded.to_dict()
    else:
        result["raw_hex"] = data.hex()
    return result


def decode_beacon_payload(payload: bytes) -> list[dict]:
    results = []
    offset = 0
    while offset < len(payload):
        if offset + 2 > len(payload):
            break
        msg_type = payload[offset]
        msg_len = payload[offset + 1]

        if offset + 2 + msg_len > len(payload):
            break

        msg_data = payload[offset : offset + 2 + msg_len]
        decoded = decode_odid_message(msg_data)
        results.append(decoded)
        offset += 2 + msg_len
    return results


# WiFi Vendor IE (OUI 0xFA0BBC) for ODID Beacon frames
ODID_WIFI_OUI = bytes([0xFA, 0x0B, 0xBC])
ODID_WIFI_OUI_WID = 0x0D


def extract_odid_from_beacon(frame_body: bytes) -> list[dict]:
    offset = 0
    while offset < len(frame_body):
        if offset + 2 > len(frame_body):
            break
        elem_id = frame_body[offset]
        elem_len = frame_body[offset + 1]
        if offset + 2 + elem_len > len(frame_body):
            break
        elem_data = frame_body[offset + 2 : offset + 2 + elem_len]

        if elem_id == 0xDD and elem_len >= 5:
            oui = elem_data[:3]
            oui_wid = elem_data[3]
            if oui == ODID_WIFI_OUI and oui_wid == ODID_WIFI_OUI_WID:
                odid_payload = elem_data[4:]
                return decode_beacon_payload(odid_payload)
        offset += 2 + elem_len
    return []


def format_summary(decoded_list: list[dict]) -> str:
    parts = []
    for d in decoded_list:
        dec = d.get("decoded", {})
        if dec.get("type") == "Basic ID":
            parts.append(f"ID:{dec.get('uas_id', '?')}({dec.get('id_type_name','?')})")
        elif dec.get("type") == "Location":
            parts.append(f"GPS:{dec.get('latitude',0):.5f},{dec.get('longitude',0):.5f}")
        elif dec.get("type") == "System":
            parts.append(f"OpPos:{dec.get('operator_latitude',0):.4f},{dec.get('operator_longitude',0):.4f}")
        elif dec.get("type") == "Operator ID":
            parts.append(f"Op:{dec.get('operator_id','?')}")
        elif dec.get("type") == "Self ID":
            parts.append(f"Desc:{dec.get('description','?')[:20]}")
        elif dec.get("type") == "Message Pack":
            parts.append(f"Pack({dec.get('count',0)}msgs)")
        else:
            parts.append(d.get("message_name", "?"))
    return " | ".join(parts)
