import asyncio
import json
import os
import threading
from http.server import HTTPServer, BaseHTTPRequestHandler
from bleak import BleakClient, BleakScanner

# ============================================================
# CONFIGURATION
# ============================================================

# HC-10 module Identifier so that the website can direct commands to it
HC10_SERVICE_UUID = "0000ffe0-0000-1000-8000-00805f9b34fb"
DATA_CHAR_UUID    = "0000ffe1-0000-1000-8000-00805f9b34fb"

# The Hc10 address, found from the bluetooth subdivision in the device manager
KNOWN_ADDRESSES = [
    "34:03:de:34:c7:9e",
]

# this file to save the last working address for the HC-10 module, so that
# when restarted, the last working address is captured
CACHE_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), ".hc10_address_cache")

# Website local host address
FLASK_URL   = "http://localhost:5000/from-arduino"
BRIDGE_PORT = 5001


_lock = threading.Lock()
_ble_client = None
_event_loop = None


def get_client_and_loop():
    with _lock:
        return _ble_client, _event_loop


def set_client_and_loop(client, loop):
    global _ble_client, _event_loop
    with _lock:
        _ble_client = client
        _event_loop = loop


# Reusing the chached address to prevent
# wasting time in rescanning all available addresses each we wanna connwct

def load_cached_address():
    try:
        if os.path.exists(CACHE_FILE):
            with open(CACHE_FILE, "r") as f:
                addr = f.read().strip()
                if addr:
                    return addr
    except Exception:
        pass
    return None


def save_cached_address(addr):
    try:
        with open(CACHE_FILE, "w") as f:
            f.write(addr)
    except Exception:
        pass


# Trying to connect to the Hc-10 module using different methods

async def try_connect_and_verify(address, label=""):
    """Connect to a BLE address and check if it has the HC-10 FFE1 characteristic."""
    try:
        client = BleakClient(address, timeout=5.0)
        await client.connect()
        if client.is_connected:
            for svc in client.services:
                for ch in svc.characteristics:
                    if ch.uuid.lower() == DATA_CHAR_UUID:
                        print(f"    HC-10 confirmed at {address} {label}")
                        return client
           
            await client.disconnect()
    except Exception:
        pass
    return None


async def find_hc10():
    """
    Find the HC-10 module using three strategies:
      1. Try cached address from last successful run
      2. Try known addresses (Device Manager MAC)
      3. Scan — match by service UUID in advertisement data
      4. Scan — brute-force connect to each device and check for FFE1
    """

    # Method 1: Use the cached address of previous connection
    cached = load_cached_address()
    if cached:
        print(f"\n[1/4] Trying cached address: {cached}")
        client = await try_connect_and_verify(cached, "(cached)")
        if client:
            return client, cached
        print(f"    Cached address didn't work. Continuing...")

    # Method 2: trying out the already known address from the device manager
    print(f"\n[2/4] Trying known addresses...")
    for addr in KNOWN_ADDRESSES:
        if addr == cached:
            continue  
        print(f"    Trying {addr}...")
        client = await try_connect_and_verify(addr, "(known)")
        if client:
            return client, addr

    # Method 3: Scan for all available addresses and try them out
    print(f"\n[3/4] Scanning for BLE devices (10 seconds)...")
    discovered = await BleakScanner.discover(timeout=10.0, return_adv=True)
    print(f"    Found {len(discovered)} device(s)")

    for address, (device, adv_data) in discovered.items():
        service_uuids = [str(u).lower() for u in (adv_data.service_uuids or [])]
        name = device.name or "(no name)"

        
        if HC10_SERVICE_UUID in service_uuids:
            print(f"    Matched by service UUID: {address} ({name})")
            client = await try_connect_and_verify(address, "(UUID match)")
            if client:
                return client, address

        
        if device.name and any(tag in device.name.upper() for tag in ["HMSOFT", "HM-10", "HC-10", "HC10", "BT05"]):
            print(f"    Matched by name: {address} ({name})")
            client = await try_connect_and_verify(address, "(name match)")
            if client:
                return client, address

    # Method 4: Try every available bluetooth device in neighbourhood
    print(f"\n[4/4] Checking each device for HC-10 characteristic...")
    print(f"    This may take a while ({len(discovered)} devices)...\n")

    for i, (address, (device, adv_data)) in enumerate(discovered.items()):
        name = device.name or "?"
        print(f"    [{i+1}/{len(discovered)}] {address} ({name})...", end=" ", flush=True)
        client = await try_connect_and_verify(address)
        if client:
            print("HC-10!")
            return client, address
        print("no")

    return None, None


# Using the HC-10 module to forward rduino data to the website

import requests

def forward_to_website(decoded: str):
    try:
        requests.post(FLASK_URL, json={"data": decoded}, timeout=1)
    except Exception:
        pass


async def notification_handler(sender, data):
    try:
        decoded = data.decode('utf-8', errors='ignore').strip()
        if decoded:
            print(f"[RX] {decoded}")
            threading.Thread(target=forward_to_website, args=(decoded,), daemon=True).start()
    except Exception as e:
        print(f"[Notification error] {e}")


# HC-10 main operation loop


async def ble_loop():
    loop = asyncio.get_running_loop()

    while True:
        client, address = await find_hc10()

        if not client:
            print("\n" + "=" * 55)
            print("  COULD NOT FIND HC-10")
            print("=" * 55)
            print("  Checklist:")
            print("  1. Is the car powered ON? (HC-10 LED should blink)")
            print("  2. REMOVE HC-10 from Windows Bluetooth settings:")
            print("     Settings > Bluetooth > HMSOFT > Remove device")
            print("     (Windows holds the connection and blocks us)")
            print("  3. Make sure no phone/tablet is connected to it")
            print("=" * 55)
            print("\n  Retrying in 15 seconds...\n")
            await asyncio.sleep(15)
            continue

        # Saving the address that worked so that we can use it next time
        save_cached_address(address)

        try:
            set_client_and_loop(client, loop)
            await client.start_notify(DATA_CHAR_UUID, notification_handler)

            print("\n" + "=" * 55)
            print(f"  HC-10 CONNECTED — {address}")
            print(f"  Bridge API: http://localhost:{BRIDGE_PORT}")
            print(f"  Website:    http://localhost:5000")
            print("=" * 55 + "\n")
            print("Listening for Arduino data...\n")

            while client.is_connected:
                await asyncio.sleep(1)

            print("HC-10 disconnected.")

        except Exception as e:
            print(f"BLE error: {e}")
        finally:
            set_client_and_loop(None, None)
            try:
                await client.disconnect()
            except Exception:
                pass

        print("Retrying in 5 seconds...")
        await asyncio.sleep(5)


# Recieving commands from the website to be forwarded to the arduino

class BridgeHandler(BaseHTTPRequestHandler):

    def do_POST(self):
        if self.path == '/send':
            try:
                length = int(self.headers.get('Content-Length', 0))
                body = self.rfile.read(length)
                data = json.loads(body.decode('utf-8'))
                cmd = data.get('cmd', '').strip()

                if not cmd:
                    self._respond(400, {"status": "error", "error": "Empty command"})
                    return

                client, loop = get_client_and_loop()

                if client and client.is_connected and loop:
                    future = asyncio.run_coroutine_threadsafe(
                        client.write_gatt_char(DATA_CHAR_UUID, (cmd + "\n").encode('utf-8')),
                        loop
                    )
                    future.result(timeout=3)
                    print(f"[TX] {cmd}")
                    self._respond(200, {"status": "ok"})
                else:
                    self._respond(503, {"status": "error", "error": "HC-10 not connected"})

            except Exception as e:
                print(f"[Send error] {e}")
                self._respond(500, {"status": "error", "error": str(e)})
        else:
            self._respond(404, {"status": "error", "error": "Not found"})

    def do_GET(self):
        if self.path == '/status':
            client, _ = get_client_and_loop()
            connected = bool(client and client.is_connected)
            self._respond(200, {"status": "ok", "connected": connected})
        else:
            self._respond(404, {"status": "error", "error": "Not found"})

    def _respond(self, code, payload):
        body = json.dumps(payload).encode('utf-8')
        self.send_response(code)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', len(body))
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        pass


def run_http_server():
    server = HTTPServer(('0.0.0.0', BRIDGE_PORT), BridgeHandler)
    server.serve_forever()



if __name__ == "__main__":
    print("=" * 55)
    print("  HC-10 BLE Bridge (Auto-Discovery)")
    print("=" * 55)
    print(f"  Bridge port : {BRIDGE_PORT}")
    print(f"  Website URL : {FLASK_URL}")
    cached = load_cached_address()
    if cached:
        print(f"  Cached addr : {cached}")
    print("=" * 55)

    
    http_thread = threading.Thread(target=run_http_server, daemon=True)
    http_thread.start()
    print(f"\nBridge API running on http://localhost:{BRIDGE_PORT}")

    try:
        asyncio.run(ble_loop())
    except KeyboardInterrupt:
        print("\nShutting down.")
s
