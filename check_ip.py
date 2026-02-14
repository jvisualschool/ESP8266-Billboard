import serial
import time
import re

def find_ip():
    ports = ['/dev/cu.usbserial-210', '/dev/cu.usbserial-1420', '/dev/cu.wchusbserial1420']
    for port in ports:
        try:
            print(f"Trying to connect to {port}...")
            ser = serial.Serial(port, 74880, timeout=1)
            print(f"Connected to {port}. Waiting for IP...")
            start_time = time.time()
            while time.time() - start_time < 10:
                line = ser.readline().decode('utf-8', errors='replace').strip()
                if line:
                    print(f"DEBUG: {line}")
                    match = re.search(r'(\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3})', line)
                    if match:
                        print(f"\n[FOUND IP] -> {match.group(1)}")
                        ser.close()
                        return match.group(1)
            ser.close()
        except:
            continue
    return None

if __name__ == "__main__":
    ip = find_ip()
    if not ip:
        print("\nCould not find IP in serial logs within 10 seconds.")
