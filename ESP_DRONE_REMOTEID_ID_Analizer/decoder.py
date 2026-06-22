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
ODID_MESSAGE_PACK = 0x0F

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


def decode_basic_id(data: bytes, offset: int = 0) -> BasicIDMessage:
    msg = BasicIDMessage()
    if len(data) - offset < 22:
        return msg
    b1 = data[offset]
    msg.id_type = (b1 >> 4) & 0x0F
    msg.ua_type = b1 & 0x0F
    raw_id = data[offset + 1 : offset + 21]
    msg.uas_id = raw_id.split(b"\x00")[0].decode("utf-8", errors="replace")
    return msg


def decode_location(data: bytes, offset: int = 0) -> LocationMessage:
    msg = LocationMessage()
    if len(data) - offset < 24:
        return msg

    b1 = data[offset]
    status = b1 >> 4
    reserved1 = (b1 >> 3) & 0x01
    height_type = (b1 >> 2) & 0x01
    ew_direction = (b1 >> 1) & 0x01
    speed_mult = b1 & 0x01

    direction_val = data[offset + 1]
    speed_h = data[offset + 2]
    speed_v = struct.unpack_from("<b", data, offset + 3)[0]
    lat_raw = struct.unpack_from("<i", data, offset + 4)[0]
    lon_raw = struct.unpack_from("<i", data, offset + 8)[0]
    alt_baro = struct.unpack_from("<H", data, offset + 12)[0]
    alt_geo = struct.unpack_from("<H", data, offset + 14)[0]
    height_raw = struct.unpack_from("<H", data, offset + 16)[0]
    b19 = data[offset + 18]
    horiz_acc = b19 & 0x0F
    vert_acc = b19 >> 4
    b20 = data[offset + 19]
    speed_acc = b20 & 0x0F
    baro_acc = b20 >> 4
    ts_raw = struct.unpack_from("<H", data, offset + 20)[0]

    if ew_direction == 1:
        direction_val += 180
    direction = float(direction_val)

    if speed_mult == 0:
        speed_horizontal = speed_h * 0.25
    else:
        speed_horizontal = 63.75 + speed_h * 0.75

    speed_vertical = speed_v * 0.5
    latitude = lat_raw / 1e7
    longitude = lon_raw / 1e7
    altitude_pressure = alt_baro * 0.5 - 1000.0
    altitude_geodetic = alt_geo * 0.5 - 1000.0
    height = height_raw * 0.5 - 1000.0
    timestamp = ts_raw / 10.0 if ts_raw != 0xFFFF else -1

    msg.status = status
    msg.direction = direction
    msg.speed_horizontal = speed_horizontal
    msg.speed_vertical = speed_vertical
    msg.latitude = latitude
    msg.longitude = longitude
    msg.altitude_pressure = altitude_pressure
    msg.altitude_geodetic = altitude_geodetic
    msg.height = height
    msg.timestamp = timestamp
    return msg


def decode_system(data: bytes, offset: int = 0) -> SystemMessage:
    msg = SystemMessage()
    if len(data) - offset < 24:
        return msg

    b1 = data[offset]
    reserved = (b1 >> 6) & 0x03
    classification_type = (b1 >> 3) & 0x07
    operator_loc_type = b1 & 0x03

    op_lat = struct.unpack_from("<i", data, offset + 1)[0]
    op_lon = struct.unpack_from("<i", data, offset + 5)[0]
    area_count = struct.unpack_from("<H", data, offset + 9)[0]
    area_radius = data[offset + 11]
    area_ceiling = struct.unpack_from("<H", data, offset + 12)[0]
    area_floor = struct.unpack_from("<H", data, offset + 14)[0]
    b17 = data[offset + 16]
    class_eu = b17 & 0x0F
    category_eu = b17 >> 4
    op_alt_geo = struct.unpack_from("<H", data, offset + 17)[0]
    ts_raw = struct.unpack_from("<I", data, offset + 19)[0]

    msg.operator_lat = op_lat / 1e7
    msg.operator_lon = op_lon / 1e7
    msg.area_count = area_count
    msg.area_radius = area_radius * 10
    msg.operator_alt_geo = op_alt_geo * 0.5 - 1000.0
    msg.timestamp = ts_raw
    return msg


def decode_operator_id(data: bytes, offset: int = 0) -> OperatorIDMessage:
    msg = OperatorIDMessage()
    if len(data) - offset < 21:
        return msg
    op_id_type = data[offset]
    raw_id = data[offset + 1 : offset + 21]
    msg.operator_id = raw_id.split(b"\x00")[0].decode("utf-8", errors="replace")
    return msg


def decode_self_id(data: bytes, offset: int = 0) -> SelfIDMessage:
    msg = SelfIDMessage()
    if len(data) - offset < 24:
        return msg
    msg.desc_type = data[offset]
    raw_desc = data[offset + 1 : offset + 24]
    msg.description = raw_desc.split(b"\x00")[0].decode("utf-8", errors="replace")
    return msg


def decode_auth(data: bytes, offset: int = 0) -> AuthMessage:
    msg = AuthMessage()
    if len(data) - offset < 24:
        return msg
    b1 = data[offset]
    auth_type = b1 >> 4
    auth_page = b1 & 0x0F
    msg.auth_type = auth_type
    msg.auth_page = auth_page
    if auth_page == 0:
        msg.auth_data = data[offset + 7 : offset + 24]
    else:
        msg.auth_data = data[offset + 1 : offset + 24]
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
    if len(data) < 1:
        return {"type": "Error", "error": "Too short"}
    header_byte = data[0]
    msg_type = header_byte >> 4
    proto_version = header_byte & 0x0F

    decoded = None
    decoder = DECODERS.get(msg_type)
    if decoder:
        decoded = decoder(data, 1)

    name = MESSAGE_TYPE_NAMES.get(msg_type, f"Unknown (0x{msg_type:02x})")

    result = {
        "message_type": msg_type,
        "protocol_version": proto_version,
        "message_name": name,
    }
    if decoded:
        result["decoded"] = decoded.to_dict()
    else:
        result["raw_hex"] = data.hex()
    return result


def _is_valid_msg_header(byte_val: int) -> bool:
    msg_type = byte_val >> 4
    return msg_type <= 5 or msg_type == 0x0F


def decode_beacon_payload(payload: bytes) -> list[dict]:
    results = []
    offset = 0

    if len(payload) >= 4:
        def looks_like_pack(pos):
            return (payload[pos] >> 4) == 0x0F and (payload[pos] & 0x0F) <= 2 and payload[pos + 1] == 25 and 1 <= payload[pos + 2] <= 9
        has_pack_at_0 = looks_like_pack(0)
        has_pack_at_1 = looks_like_pack(1)
        if has_pack_at_1 and not has_pack_at_0:
            offset = 1

    while offset < len(payload):
        if offset >= len(payload):
            break

        header_byte = payload[offset]

        if not _is_valid_msg_header(header_byte):
            offset += 1
            continue

        msg_type = header_byte >> 4

        if msg_type == 0x0F:
            if offset + 3 > len(payload):
                break
            single_msg_size = payload[offset + 1]
            pack_size = payload[offset + 2]

            if pack_size == 0 or single_msg_size < 20:
                offset += 1
                continue

            pack_header_len = 3
            total_msg_bytes = pack_size * single_msg_size

            if offset + pack_header_len + total_msg_bytes > len(payload):
                tail = len(payload) - offset - pack_header_len
                pack_size = tail // single_msg_size
                total_msg_bytes = pack_size * single_msg_size

            for i in range(pack_size):
                msg_start = offset + pack_header_len + i * single_msg_size
                msg_data = payload[msg_start : msg_start + single_msg_size]
                decoded = decode_odid_message(msg_data)
                results.append(decoded)

            offset += pack_header_len + total_msg_bytes

        elif msg_type <= 5:
            msg_size = 25
            if offset + msg_size > len(payload):
                msg_data = payload[offset:]
            else:
                msg_data = payload[offset : offset + msg_size]
            decoded = decode_odid_message(msg_data)
            results.append(decoded)
            offset += len(msg_data)

        else:
            offset += 1

    return results


ODID_WIFI_OUI = bytes([0xFA, 0x0B, 0xBC])
ODID_WIFI_OUI_WID = 0x0D


def extract_odid_from_beacon(frame_body: bytes) -> list[dict]:
    results = []
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
                results.extend(decode_beacon_payload(odid_payload))
        offset += 2 + elem_len
    return results


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
