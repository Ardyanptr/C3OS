import socket
import math

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('0.0.0.0', 12345))

def get_block_at(x, y):
    # World Generation super besar pake math
    # surfaceY akan berubah-ubah sesuai koordinat X
    surface_y = 32 + int(math.sin(x * 0.1) * 5 + math.cos(x * 0.05) * 8)
    
    if y < surface_y: return "0" # Udara
    if y == surface_y: return "2" # Grass
    if y < surface_y + 4: return "1" # Dirt
    return "3" # Stone

print("Server Minecraft C3 Online!")

while True:
    data, addr = sock.recvfrom(1024)
    print(f"Ada paket masuk dari {addr}: {data}")

    msg = data.decode(errors='ignore')

    if "MC_C3_PING" in msg:
        sock.sendto(b"MC_C3_PONG|Python_Server", (addr[0], 12346))
    
    elif msg.startswith("POS:"):
        # ESP32 kirim "POS:100,32"
        try:
            _, coords = msg.split(":")
            px, py = map(float, coords.split(","))
            
            # Ambil area sekitar player (18x10 blok)
            view_w, view_h = 18, 10
            map_data = ""
            
            start_x = int(px) - 9
            start_y = int(py) - 4
            
            for y in range(start_y, start_y + view_h):
                for x in range(start_x, start_x + view_w):
                    map_data += get_block_at(x, y)
            
            response = f"MAP:{map_data}"
            sock.sendto(response.encode(), (addr[0], 12346))
        except:
            pass