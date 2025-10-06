import socket
import time
import struct

def send_heartrate_osc(host='localhost', port=8000, bpm=75):
    """Send heart rate as OSC message to the VST3 plugin"""
    
    # Create OSC message: /heartrate <float>
    address = "/heartrate\0\0\0"  # Padded to 4-byte boundary
    typetag = ",f\0\0"  # Float type tag, padded
    value = struct.pack('>f', float(bpm))  # Big-endian float
    
    # OSC message format
    message = address.encode() + typetag.encode() + value
    
    # Send UDP packet
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        sock.sendto(message, (host, port))
        print(f"Sent heart rate: {bpm} BPM")
    finally:
        sock.close()

if __name__ == "__main__":
    print("HeartSync VST3 Test - Sending heart rate data...")
    
    # Simulate varying heart rate
    for i in range(10):
        bpm = 60 + (i * 5)  # 60 to 105 BPM
        send_heartrate_osc(bpm=bpm)
        time.sleep(1)
    
    print("Test complete!")
