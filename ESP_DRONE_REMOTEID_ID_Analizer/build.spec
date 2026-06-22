# -*- mode: python -*-
"""PyInstaller build spec for RID Analyzer (Windows exe)."""

import sys
import os
from pathlib import Path

ROOT = Path(__file__).parent

a = Analysis(
    [str(ROOT / "gui.py")],
    pathex=[str(ROOT)],
    binaries=[],
    datas=[
        (str(ROOT / "web" / "index.html"), "web"),
        (str(ROOT / "web" / "app.js"), "web"),
        (str(ROOT / "web" / "style.css"), "web"),
    ],
    hiddenimports=[
        "scapy.all",
        "simple_websocket_server",
        "webview",
    ],
    hookspath=[],
    runtime_hooks=[],
    excludes=[],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=None,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=None)

exe = EXE(
    pyz,
    a.scripts,
    a.binaries,
    a.zipfiles,
    a.datas,
    [],
    name="ESP_DRONE_REMOTEID_ANALIZER",
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    upx_exclude=[],
    runtime_tmpdir=None,
    console=False,
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    icon=str(ROOT / "web" / "icon.ico") if (ROOT / "web" / "icon.ico").exists() else None,
)
