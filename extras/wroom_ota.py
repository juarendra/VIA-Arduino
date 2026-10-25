#!/usr/bin/env python3
"""Provision WROOM Wi-Fi over serial; upload application images over authenticated OTA."""
import argparse
import ctypes
from ctypes import wintypes
import hashlib
import importlib.util
import ipaddress
import json
import os
from pathlib import Path
import re
import secrets
import sys
import time

DEFAULT_CREDENTIAL = "AirVIA/WROOM/24-6F-28-A0-DB-34"


def credential(target, password=None):
    """Use the current Windows user's Credential Manager, never a repository file."""
    if sys.platform != "win32":
        raise RuntimeError("Set AIRVIA_OTA_PASSWORD on non-Windows hosts")

    class Credential(ctypes.Structure):
        _fields_ = [
            ("Flags", wintypes.DWORD), ("Type", wintypes.DWORD),
            ("TargetName", wintypes.LPWSTR), ("Comment", wintypes.LPWSTR),
            ("LastWritten", wintypes.FILETIME), ("CredentialBlobSize", wintypes.DWORD),
            ("CredentialBlob", ctypes.POINTER(ctypes.c_ubyte)),
            ("Persist", wintypes.DWORD), ("AttributeCount", wintypes.DWORD),
            ("Attributes", ctypes.c_void_p), ("TargetAlias", wintypes.LPWSTR),
            ("UserName", wintypes.LPWSTR),
        ]

    api = ctypes.WinDLL("advapi32", use_last_error=True)
    api.CredWriteW.argtypes = [ctypes.POINTER(Credential), wintypes.DWORD]
    api.CredWriteW.restype = wintypes.BOOL
    api.CredReadW.argtypes = [wintypes.LPCWSTR, wintypes.DWORD, wintypes.DWORD,
                              ctypes.POINTER(ctypes.POINTER(Credential))]
    api.CredReadW.restype = wintypes.BOOL
    api.CredFree.argtypes = [ctypes.c_void_p]
    if password is not None:
        blob = (ctypes.c_ubyte * len(password.encode("utf-8"))).from_buffer_copy(password.encode("utf-8"))
        record = Credential()
        record.Type, record.Persist = 1, 2
        record.TargetName, record.UserName = target, "AirVIA OTA"
        record.CredentialBlobSize, record.CredentialBlob = len(blob), blob
        if not api.CredWriteW(ctypes.byref(record), 0):
            raise RuntimeError("Cannot save OTA credential in Windows Credential Manager")
        return password
    record = ctypes.POINTER(Credential)()
    if not api.CredReadW(target, 1, 0, ctypes.byref(record)):
        if ctypes.get_last_error() == 1168:
            return None
        raise RuntimeError("Cannot read Windows OTA credential")
    try:
        return ctypes.string_at(record.contents.CredentialBlob,
                                record.contents.CredentialBlobSize).decode("utf-8")
    finally:
        api.CredFree(record)


def setup_command(value, ota_password):
    if not isinstance(value, dict) or set(value) != {"ssid", "password"}:
        raise ValueError("Expected JSON object with ssid and password")
    fields = [value["ssid"], value["password"]]
    if any(not isinstance(field, str) or any(ord(c) < 32 or ord(c) == 127 for c in field)
           for field in fields):
        raise ValueError("Invalid Wi-Fi field")
    ssid, password = (field.encode("utf-8") for field in fields)
    if not 1 <= len(ssid) <= 32 or not 8 <= len(password) <= 64:
        raise ValueError("Wi-Fi fields exceed supported byte lengths")
    if len(password) == 64 and not re.fullmatch(b"[0-9a-fA-F]{64}", password):
        raise ValueError("A 64-byte Wi-Fi password must be a hexadecimal PSK")
    if not isinstance(ota_password, str) or len(ota_password) < 16:
        raise ValueError("OTA password must have at least 16 characters")
    digest = hashlib.md5(ota_password.encode("utf-8")).hexdigest().encode("ascii")
    return b"OTA_SETUP\t" + ssid + b"\t" + password + b"\t" + digest + b"\n"


def read_status(port, timeout, require_saved=False):
    end = time.monotonic() + timeout
    saved, online = not require_saved, False
    next_status = 0
    status = {}
    while time.monotonic() < end:
        if time.monotonic() >= next_status:
            port.write(b"OTA_STATUS\n")
            next_status = time.monotonic() + 2
        line = port.readline(256).decode("ascii", "replace").strip()
        if line in ("[OTA] invalid", "[OTA] save failed"):
            raise RuntimeError("Device rejected OTA provisioning")
        if line == "[OTA] saved":
            saved = True
            print(line, flush=True)
        match = re.fullmatch(r"\[OTA\] (ip|image|slot)=([A-Za-z0-9_.:-]+)", line)
        if match:
            key, value = match.groups()
            status[key] = value
            if key == "ip":
                address = ipaddress.IPv4Address(value)
                online = not address.is_unspecified
            print(line, flush=True)
        if saved and online and "image" in status and "slot" in status:
            return
    raise RuntimeError("Device did not report an online OTA IP before timeout")


def serial_action(args):
    import serial
    command = None
    if args.command == "provision":
        print("PROVISION_READY", flush=True)
        if sys.stdin.isatty():
            from getpass import getpass
            value = {"ssid": input("Wi-Fi SSID: "), "password": getpass("Wi-Fi password: ")}
        else:
            value = json.loads(sys.stdin.readline(4096))
        # Validate before saving anything. Error messages never include field contents.
        setup_command(value, "validation-only-password")
        password = os.environ.get("AIRVIA_OTA_PASSWORD") or credential(args.credential)
        if not password:
            password = secrets.token_urlsafe(24)
        command = setup_command(value, password)
        if "AIRVIA_OTA_PASSWORD" not in os.environ:
            credential(args.credential, password)
    port = serial.Serial()
    port.port, port.baudrate, port.timeout, port.write_timeout = args.port, 115200, 0.2, 3
    port.dtr = port.rts = False
    with port:
        port.reset_input_buffer()
        if command:
            port.write(command)
            port.flush()
        read_status(port, args.timeout, require_saved=command is not None)


def upload(args):
    ipaddress.IPv4Address(args.ip)
    image, espota = Path(args.image), Path(args.espota)
    if not image.is_file() or image.suffix != ".bin":
        raise ValueError("Application .bin file is missing")
    with image.open("rb") as stream:
        if stream.read(1) != bytes([0xE9]):
            raise ValueError("Expected an application .bin, not a merged flash image")
    if not espota.is_file():
        raise ValueError("ESP32 core espota.py file is missing")
    password = os.environ.get("AIRVIA_OTA_PASSWORD") or credential(args.credential)
    if not password:
        raise RuntimeError("OTA credential missing; provision first or set AIRVIA_OTA_PASSWORD")
    spec = importlib.util.spec_from_file_location("airvia_espota", espota)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    # In-process call keeps the password out of OS command lines and process listings.
    result = module.main(["-i", args.ip, "-I", args.host_ip, "-P", "3233", "-p", "3232",
                          "-a", password, "-f", str(image.resolve()), "-t", str(args.timeout)])
    if result != 0:
        raise RuntimeError("OTA transfer failed; active firmware was not confirmed updated")
    print("OTA_UPLOAD_OK", flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    for name in ("provision", "status"):
        command = sub.add_parser(name)
        command.add_argument("--port", default="COM39")
        command.add_argument("--timeout", type=int, default=45)
        command.add_argument("--credential", default=DEFAULT_CREDENTIAL)
    command = sub.add_parser("upload")
    command.add_argument("--ip", required=True)
    command.add_argument("--image", required=True)
    command.add_argument("--espota", required=True)
    command.add_argument("--host-ip", default="0.0.0.0")
    command.add_argument("--timeout", type=int, default=15)
    command.add_argument("--credential", default=DEFAULT_CREDENTIAL)
    args = parser.parse_args()
    if args.timeout <= 0:
        parser.error("timeout must be positive")
    try:
        if args.command == "upload":
            upload(args)
        else:
            serial_action(args)
    except (ValueError, RuntimeError) as error:
        # Only our fixed, credential-free validation messages are printable.
        if isinstance(error, json.JSONDecodeError):
            print("ERROR: invalid provisioning JSON", file=sys.stderr)
        else:
            print("ERROR:", str(error), file=sys.stderr)
        return 1
    except Exception:
        print("ERROR: serial/network/credential operation failed; no secrets logged", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
