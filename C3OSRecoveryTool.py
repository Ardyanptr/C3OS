import serial
import serial.tools.list_ports
import time
import sys
from tqdm import tqdm

print("\n" + "="*40)
print("      C3OS Recovery Tool v1.0")
print("="*40 + "\n")

def get_hw_info(port_name):
    ports = serial.tools.list_ports.comports()
    for port in ports:
        if port.device == port_name:
            return f"{port.description} [{port.hwid}]"
    return "Unknown Device"

# Input Port dengan validasi simpel
PORT = str(input("[*] Masukkan Port (contoh COM5 atau /dev/ttyUSB0): ")).upper().strip()
BAUD = 115200

def recovery_sequence():
    print(f"\n[*] Status: Waiting for connection on {PORT}...")
    
    ser = None
    while ser is None:
        try:
            ser = serial.Serial(PORT, BAUD, timeout=2)
            hw_info = get_hw_info(PORT)
            
            # Tampilkan Informasi Hardware
            print(f"\n" + "-"*30)
            print(f"[+] DEVICE DETECTED!")
            print(f"[>] Port     : {PORT}")
            print(f"[>] Hardware : {hw_info}")
            print("-"*30)
            
            # Konfirmasi User
            confirm = input(f"\n[?] Apakah anda yakin ingin me-restart hardware ini? (y/n): ").lower()
            if confirm != 'y':
                print("[!] Recovery dibatalkan oleh user.")
                ser.close()
                return

        except (serial.SerialException, FileNotFoundError):
            for char in ["/", "-", "\\", "|"]:
                sys.stdout.write(f"\r[ ] Mencari device... {char}")
                sys.stdout.flush()
                time.sleep(0.1)
            continue 

    print(f"\n[*] Memulai sequence: Attempting Soft-Ping...")
    
    # STEP 1: Soft Ping
    ser.write(b"PING\n")
    response = ser.readline().decode().strip()

    if response == "PONG":
        print("[+] System Responsive. Mengirim perintah Soft Reboot...")
        ser.write(b"REBOOT\n")
    else:
        # STEP 2: Last Resort - Hardware Reset
        print("[!] No response (Kernel Stuck). Menjalankan Hardware Reset via USB...")
        
        for i in tqdm(range(100), desc="Resetting C3OS", ascii=True, ncols=75):
            if i == 10:
                ser.setDTR(False) 
                ser.setRTS(True)
            if i == 30:
                ser.setDTR(True)  
                ser.setRTS(False)
            time.sleep(0.02)
        
    print("\n[+] ESP32-C3 berhasil direstart.")
    print("[*] Masuk ke Monitor Mode. Tekan Ctrl+C untuk keluar.")
    print("="*50 + "\n")

    # STEP 3: Monitor Mode
    try:
        ser.reset_input_buffer() 
        while True:
            if ser.in_waiting:
                line = ser.readline().decode(errors='replace').strip()
                if line:
                    timestamp = time.strftime("%H:%M:%S")
                    print(f"[{timestamp}] [ESP32]: {line}")
            time.sleep(0.001)
    except KeyboardInterrupt:
        print("\n[*] Recovery Tool ditutup.")
        ser.close()

if __name__ == "__main__":
    recovery_sequence()