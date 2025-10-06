"""
Simple BLE Scanner Test
Tests if Bluetooth scanning works at all on this system
"""

import asyncio
import sys

try:
    import bleak
    print("[OK] bleak library imported successfully")
    print(f"[INFO] bleak version: {bleak.__version__}")
except ImportError as e:
    print(f"[ERROR] Cannot import bleak: {e}")
    print("[INFO] Install with: pip install bleak")
    sys.exit(1)

async def simple_scan():
    print("[SCAN] Starting simple BLE scan...")
    print("[INFO] Make sure Bluetooth is enabled in Windows settings")
    print("[INFO] Make sure your Coospo HW700 is powered on")
    print("[WAIT] Scanning for 10 seconds...")
    print()
    
    try:
        devices = await bleak.BleakScanner.discover(timeout=10.0)
        
        print(f"\n[RESULT] Found {len(devices)} Bluetooth devices:")
        print("=" * 80)
        
        if len(devices) == 0:
            print("[WARNING] No devices found!")
            print()
            print("Troubleshooting steps:")
            print("1. Check if Bluetooth is enabled in Windows Settings")
            print("2. Check if your Coospo HW700 watch is powered on")
            print("3. Try moving the watch closer to your computer")
            print("4. Check if other apps can see Bluetooth devices")
            print("5. Try restarting Bluetooth in Windows")
            return
        
        for i, device in enumerate(devices, 1):
            name = device.name if device.name else "[No Name]"
            print(f"\n[DEVICE {i}]")
            print(f"  Name:    {name}")
            print(f"  Address: {device.address}")
            print(f"  RSSI:    {device.rssi} dBm")
            
            # Check if it might be a heart rate device
            keywords = ["hr", "heart", "coospo", "hw700", "hw", "polar", "wahoo", "garmin"]
            if any(k in name.lower() for k in keywords):
                print(f"  >>> POSSIBLE HEART RATE DEVICE <<<")
        
        print("\n" + "=" * 80)
        print(f"[DONE] Scan complete. Found {len(devices)} devices.")
        
    except Exception as e:
        print(f"\n[ERROR] Scan failed: {e}")
        import traceback
        traceback.print_exc()
        
        print("\nPossible issues:")
        print("- Bluetooth adapter not available")
        print("- Bluetooth drivers not installed")
        print("- Windows Bluetooth permissions denied")
        print("- bleak version incompatible with your system")

def main():
    print("=" * 80)
    print("HeartSync - BLE Scanner Diagnostic Tool")
    print("=" * 80)
    print()
    
    # Check Python version
    print(f"[INFO] Python version: {sys.version}")
    print()
    
    # Run scan
    asyncio.run(simple_scan())
    
    print()
    print("[INFO] If no devices were found, try:")
    print("  - Ensuring Bluetooth is enabled in Windows")
    print("  - Power cycling your Coospo HW700 watch")
    print("  - Running this script as Administrator")
    print("  - Checking Windows Privacy settings for Bluetooth")

if __name__ == "__main__":
    main()
