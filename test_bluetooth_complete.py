#!/usr/bin/env python3
"""
Complete Bluetooth Test for HeartSync
Tests all aspects of Bluetooth functionality
"""

import asyncio
import sys

try:
    import bleak
    print("✅ Bleak (Bluetooth LE) library is available")
    BLUETOOTH_AVAILABLE = True
except ImportError as e:
    print(f"❌ Bleak not available: {e}")
    BLUETOOTH_AVAILABLE = False
    sys.exit(1)


async def test_bluetooth_adapter():
    """Test if Bluetooth adapter is available and working"""
    print("\n🔍 Testing Bluetooth Adapter...")
    try:
        scanner = bleak.BleakScanner()
        print("✅ Bluetooth adapter is accessible")
        return True
    except Exception as e:
        print(f"❌ Bluetooth adapter error: {e}")
        return False


async def test_device_scanning():
    """Test Bluetooth LE device scanning"""
    print("\n📡 Testing Device Scanning...")
    try:
        print("Scanning for Bluetooth LE devices (10 second timeout)...")
        devices = await bleak.BleakScanner.discover(timeout=10.0)
        
        print(f"✅ Scan completed successfully!")
        print(f"📱 Found {len(devices)} Bluetooth LE devices:")
        
        hr_devices = []
        for i, device in enumerate(devices, 1):
            name = device.name or "Unknown Device"
            print(f"  {i:2d}. {name} ({device.address})")
            
            # Check for heart rate related devices
            hr_keywords = ["hr", "heart", "polar", "wahoo", "garmin", "tickr", "cardio", "apple", "watch", "coospo", "hw700", "hw"]
            if any(keyword in name.lower() for keyword in hr_keywords):
                hr_devices.append(device)
                print(f"      💓 Potential heart rate device detected!")
        
        print(f"\n💓 Found {len(hr_devices)} potential heart rate devices")
        return devices
        
    except Exception as e:
        print(f"❌ Scanning failed: {e}")
        import traceback
        traceback.print_exc()
        return []


async def test_hr_service_discovery(device):
    """Test heart rate service discovery on a device"""
    print(f"\n🔧 Testing HR Service Discovery on {device.name}...")
    try:
        client = bleak.BleakClient(device.address)
        await client.connect()
        
        if client.is_connected:
            print("✅ Connected successfully")
            
            # Get all services
            services = await client.get_services()
            print(f"📋 Found {len(services.services)} services:")
            
            hr_service_found = False
            hr_characteristic_found = False
            
            for service in services.services:
                print(f"  Service: {service.uuid}")
                
                # Check for Heart Rate Service
                if "180d" in str(service.uuid).lower():
                    hr_service_found = True
                    print("    💓 Heart Rate Service found!")
                    
                    for char in service.characteristics:
                        print(f"    Characteristic: {char.uuid}")
                        if "2a37" in str(char.uuid).lower():
                            hr_characteristic_found = True
                            print("      💓 Heart Rate Measurement characteristic found!")
            
            await client.disconnect()
            
            if hr_service_found and hr_characteristic_found:
                print("✅ Heart Rate service and characteristics available")
                return True
            else:
                print("⚠️  Heart Rate service not found on this device")
                return False
                
        else:
            print("❌ Failed to connect")
            return False
            
    except Exception as e:
        print(f"❌ Service discovery failed: {e}")
        return False


async def main():
    """Run complete Bluetooth test suite"""
    print("🧪 HeartSync Bluetooth Complete Test Suite")
    print("=" * 50)
    
    # Test 1: Bluetooth adapter
    adapter_ok = await test_bluetooth_adapter()
    if not adapter_ok:
        print("\n❌ Bluetooth adapter test failed. Cannot continue.")
        return
    
    # Test 2: Device scanning
    devices = await test_device_scanning()
    if not devices:
        print("\n⚠️  No devices found. This could be normal if no BLE devices are nearby.")
        return
    
    # Test 3: Try to find and test a heart rate device
    hr_devices = []
    for device in devices:
        if device.name:
            hr_keywords = ["hr", "heart", "polar", "wahoo", "garmin", "tickr", "cardio", "apple", "watch", "coospo", "hw700", "hw"]
            if any(keyword in device.name.lower() for keyword in hr_keywords):
                hr_devices.append(device)
    
    if hr_devices:
        print(f"\n🧪 Testing first potential HR device: {hr_devices[0].name}")
        hr_service_ok = await test_hr_service_discovery(hr_devices[0])
        if hr_service_ok:
            print("\n✅ Complete Bluetooth test PASSED!")
            print("💓 HeartSync should be able to connect to heart rate devices")
        else:
            print("\n⚠️  Device doesn't have HR service, but scanning works")
    else:
        print("\n✅ Bluetooth scanning works, but no HR devices found nearby")
        print("💡 To test fully, turn on a Bluetooth heart rate monitor")
    
    print("\n🎉 Bluetooth functionality test complete!")


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        print("\n\n⏹️  Test interrupted by user")
    except Exception as e:
        print(f"\n❌ Test failed with error: {e}")
        import traceback
        traceback.print_exc()
