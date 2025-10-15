#!/usr/bin/env python3
"""
UDS Smoke Test Tool for HeartSync Bridge
Sends length-prefixed JSON messages to the Bridge socket for testing.

Usage:
    python3 uds_send.py ready
    python3 uds_send.py perm AUTHORIZED
    python3 uds_send.py device
    python3 uds_send.py connected
    python3 uds_send.py hr 72
    python3 uds_send.py disconnected
"""

import socket
import json
import struct
import sys
import time
import os
from pathlib import Path

# Socket path (same as client & bridge)
SOCKET_PATH = os.path.expanduser("~/Library/Application Support/HeartSync/bridge.sock")

def send_message(sock, message_dict):
    """Send a length-prefixed JSON message to the socket."""
    # Serialize to JSON
    json_str = json.dumps(message_dict)
    json_bytes = json_str.encode('utf-8')
    
    # Create 4-byte big-endian length prefix
    length = len(json_bytes)
    if length > 65536:
        print(f"ERROR: Message too large ({length} bytes, max 64KB)", file=sys.stderr)
        return False
    
    length_prefix = struct.pack('>I', length)  # Big-endian uint32
    
    # Send length + payload
    try:
        sock.sendall(length_prefix)
        sock.sendall(json_bytes)
        print(f"✓ Sent: {json_str}")
        return True
    except Exception as e:
        print(f"ERROR sending message: {e}", file=sys.stderr)
        return False

def receive_message(sock, timeout=2.0):
    """Receive and parse a length-prefixed JSON message (if any)."""
    sock.settimeout(timeout)
    try:
        # Read 4-byte length prefix
        length_data = sock.recv(4)
        if not length_data or len(length_data) < 4:
            return None
        
        length = struct.unpack('>I', length_data)[0]
        if length > 65536:
            print(f"ERROR: Invalid message length {length}", file=sys.stderr)
            return None
        
        # Read payload
        payload = b''
        while len(payload) < length:
            chunk = sock.recv(length - len(payload))
            if not chunk:
                break
            payload += chunk
        
        if len(payload) != length:
            print(f"ERROR: Incomplete payload (got {len(payload)}, expected {length})", file=sys.stderr)
            return None
        
        # Parse JSON
        message = json.loads(payload.decode('utf-8'))
        print(f"✓ Received: {json.dumps(message)}")
        return message
        
    except socket.timeout:
        return None
    except Exception as e:
        print(f"ERROR receiving message: {e}", file=sys.stderr)
        return None

def connect_to_bridge():
    """Connect to the Bridge UDS socket."""
    if not Path(SOCKET_PATH).exists():
        print(f"ERROR: Socket not found at {SOCKET_PATH}", file=sys.stderr)
        print("Make sure HeartSync Bridge is running.", file=sys.stderr)
        return None
    
    try:
        sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        sock.connect(SOCKET_PATH)
        print(f"✓ Connected to {SOCKET_PATH}")
        return sock
    except Exception as e:
        print(f"ERROR connecting to socket: {e}", file=sys.stderr)
        return None

def cmd_ready(sock):
    """Send ready message."""
    return send_message(sock, {"type": "ready", "version": 1})

def cmd_permission(sock, state):
    """Send permission state."""
    state_lower = state.lower()
    if state_lower not in ["authorized", "denied", "unknown", "requesting"]:
        print(f"ERROR: Invalid permission state '{state}' (use AUTHORIZED, DENIED, UNKNOWN, or REQUESTING)", file=sys.stderr)
        return False
    return send_message(sock, {"type": "permission", "state": state_lower})

def cmd_device(sock):
    """Send fake device_found message."""
    return send_message(sock, {
        "type": "device_found",
        "id": "AA:BB:CC:DD:EE:FF",
        "rssi": -60,
        "name": "Polar H10 1234"
    })

def cmd_connected(sock):
    """Send connected event."""
    return send_message(sock, {
        "type": "connected",
        "id": "AA:BB:CC:DD:EE:FF"
    })

def cmd_hr(sock, bpm):
    """Send heart rate data."""
    try:
        bpm_int = int(bpm)
        if not (30 <= bpm_int <= 220):
            print(f"WARNING: BPM {bpm_int} outside normal range (30-220)", file=sys.stderr)
    except ValueError:
        print(f"ERROR: Invalid BPM '{bpm}' (must be integer)", file=sys.stderr)
        return False
    
    return send_message(sock, {
        "type": "hr_data",
        "bpm": bpm_int,
        "ts": time.time()
    })

def cmd_disconnected(sock):
    """Send disconnected event."""
    return send_message(sock, {
        "type": "disconnected",
        "reason": "debug"
    })

def main():
    if len(sys.argv) < 2:
        print(__doc__)
        sys.exit(1)
    
    command = sys.argv[1].lower()
    
    # Connect to Bridge
    sock = connect_to_bridge()
    if not sock:
        sys.exit(1)
    
    try:
        # Execute command
        success = False
        if command == "ready":
            success = cmd_ready(sock)
        elif command == "perm":
            if len(sys.argv) < 3:
                print("ERROR: 'perm' requires state argument (AUTHORIZED|DENIED|UNKNOWN|REQUESTING)", file=sys.stderr)
                sys.exit(1)
            success = cmd_permission(sock, sys.argv[2])
        elif command == "device":
            success = cmd_device(sock)
        elif command == "connected":
            success = cmd_connected(sock)
        elif command == "hr":
            if len(sys.argv) < 3:
                print("ERROR: 'hr' requires BPM argument (e.g., 'hr 72')", file=sys.stderr)
                sys.exit(1)
            success = cmd_hr(sock, sys.argv[2])
        elif command == "disconnected":
            success = cmd_disconnected(sock)
        else:
            print(f"ERROR: Unknown command '{command}'", file=sys.stderr)
            print(__doc__)
            sys.exit(1)
        
        if not success:
            sys.exit(1)
        
        # Wait briefly for response
        time.sleep(0.1)
        response = receive_message(sock, timeout=0.5)
        if response:
            print(f"✓ Bridge acknowledged")
        
    finally:
        sock.close()
        print("✓ Disconnected")
    
    sys.exit(0)

if __name__ == "__main__":
    main()
