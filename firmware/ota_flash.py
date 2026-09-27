#!/usr/bin/env python3
"""
tinyTouch Local OTA Flasher
Streams built firmware binary to tinyTouch device via Serial OTA protocol.
No physical buttons or ROM bootloader required.
"""
import sys
import os
import time
import base64
import hashlib
import secrets

try:
    import serial
except ImportError:
    print("Error: pyserial is required. Run with ESP-IDF python:")
    print("  ~/.espressif/python_env/idf5.3_py3.9_env/bin/python ota_flash.py")
    sys.exit(1)

CHUNK_SIZE = 3072
WRITE_WINDOW = 8

def read_line(ser, timeout=5.0):
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = ser.readline()
        if line:
            text = line.decode("utf-8", "replace").strip()
            if text:
                return text
    return None

def send_and_wait(ser, cmd, timeout=5.0):
    ser.write((cmd + "\n").encode("ascii"))
    ser.flush()
    while True:
        line = read_line(ser, timeout=timeout)
        if line is None:
            raise TimeoutError(f"Timeout waiting for response to: {cmd}")
        print(f"  <-- {line}")
        if line.startswith("OK ") or line.startswith("ERR "):
            return line

def main():
    port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem21103"
    bin_path = sys.argv[2] if len(sys.argv) > 2 else "build/tiny_touch_unified.bin"

    if not os.path.exists(bin_path):
        print(f"Error: binary file not found: {bin_path}")
        sys.exit(1)

    with open(bin_path, "rb") as f:
        image = f.read()

    digest = hashlib.sha256(image).hexdigest()
    size = len(image)
    print(f"=== tinyTouch Serial OTA Flasher ===")
    print(f"Port   : {port}")
    print(f"Binary : {bin_path} ({size} bytes)")
    print(f"SHA256 : {digest}")

    print(f"\n[1/5] Opening serial port {port}...")
    try:
        ser = serial.Serial(port, 115200, timeout=0.25, write_timeout=5)
    except Exception as e:
        print(f"Error opening port: {e}")
        print("Note: Make sure Web Controller or other apps disconnected from the port.")
        sys.exit(1)

    time.sleep(0.5)
    ser.reset_input_buffer()

    print("\n[2/5] Checking device status...")
    ser.write(b"STATUS\n")
    ser.flush()
    time.sleep(0.3)
    while True:
        line = read_line(ser, timeout=1.0)
        if not line: break
        print(f"  status: {line}")

    print("\n[3/5] Authorizing...")
    try:
        # Abort any dangling OTA first
        ser.write(b"OTA ABORT\n")
        ser.flush()
        time.sleep(0.2)
        while read_line(ser, timeout=0.5): pass

        # Send AUTH
        ser.write(b"AUTH\n")
        ser.flush()
        auth_ok = False
        deadline = time.time() + 15.0
        while time.time() < deadline:
            line = read_line(ser, timeout=2.0)
            if not line: continue
            print(f"  auth: {line}")
            if "EVENT TOUCH" in line:
                print("  👉 Chạm ngón tay vào cảm biến vân tay để xác nhận!...")
            if line.startswith("OK AUTH"):
                auth_ok = True
                break
            if line.startswith("ERR "):
                print(f"  Auth error: {line}")
                break

        if not auth_ok:
            print("Warning: AUTH did not return OK, attempting OTA anyway...")
    except Exception as e:
        print(f"Auth notice: {e}")

    token = secrets.token_hex(16)
    print(f"\n[4/5] Starting OTA session (token={token})...")
    begin_cmd = f"OTA BEGIN {token} {size} {digest}"
    begin_res = send_and_wait(ser, begin_cmd, timeout=8.0)
    if not begin_res.startswith("OK OTA BEGIN"):
        print(f"Failed to begin OTA: {begin_res}")
        ser.close()
        sys.exit(1)

    print("\n[5/5] Streaming firmware chunks...")
    starts = list(range(0, size, CHUNK_SIZE))
    total_chunks = len(starts)

    for i in range(0, total_chunks, WRITE_WINDOW):
        window_starts = starts[i:i + WRITE_WINDOW]
        commands = []
        for start in window_starts:
            chunk = image[start:start + CHUNK_SIZE]
            b64 = base64.b64encode(chunk).decode("ascii")
            cmd = f"OTA WRITE {token} {start} {b64}"
            ser.write((cmd + "\n").encode("ascii"))
            commands.append(cmd)
        ser.flush()

        # Wait for ACKs
        for cmd in commands:
            while True:
                line = read_line(ser, timeout=8.0)
                if not line: raise TimeoutError("Timeout waiting for chunk ACK")
                if line.startswith("OK OTA WRITE") or line.startswith("ERR "):
                    break
            if line.startswith("ERR "):
                print(f"Chunk write error: {line}")
                ser.close()
                sys.exit(1)

        pct = min(100, (window_starts[-1] + CHUNK_SIZE) * 100 // size)
        print(f"  Progress: {pct}% ({min(size, window_starts[-1] + CHUNK_SIZE)}/{size} bytes)")

    print("\nCommiting OTA...")
    commit_res = send_and_wait(ser, f"OTA COMMIT {token}", timeout=10.0)
    if commit_res.startswith("OK OTA STAGED"):
        print("\n✅ THÀNH CÔNG! Firmware đã nạp vào phân vùng OTA.")
        print("👉 Rút USB thiết bị ra và cắm lại để boot vào firmware mới!")
    else:
        print(f"Commit error: {commit_res}")

    ser.close()

if __name__ == "__main__":
    main()
