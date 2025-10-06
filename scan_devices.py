#!/usr/bin/env python3
"""
Quick Bluetooth Device Scanner
"""
import asyncio
import bleak

async def scan_all_devices():
    print("🔍 Scanning for ALL Bluetooth LE devices...")
    try:
        devices = await bleak.BleakScanner.discover(timeout=10.0)
        print(f"📱 Found {len(devices)} total devices:")
        
        for i, device in enumerate(devices):
            name = device.name or "Unknown Device"
            print(f"{i+1:2d}. {name}")
            print(f"    Address: {device.address}")
            print(f"    RSSI: {device.rssi} dBm")
            
            # Check if it might be an Apple device
            if any(keyword in name.lower() for keyword in ["apple", "watch", "iphone", "griffin"]):
                print(f"    🍎 APPLE DEVICE DETECTED!")
                
            # Check if it advertises heart rate service
            if device.metadata and "uuids" in device.metadata:
                uuids = device.metadata["uuids"]
                if "0000180d-0000-1000-8000-00805f9b34fb" in uuids:
                    print(f"    ❤️ HEART RATE SERVICE DETECTED!")
            print()
            
    except Exception as e:
        print(f"❌ Scan error: {e}")

if __name__ == "__main__":
    asyncio.run(scan_all_devices())
