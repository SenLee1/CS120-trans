import socket

# 这里的 IP 要和你 dns_server.py 里写的 CPP_IP 一模一样！
# 刚才你改成了局域网 IP，这里也得改。
LISTEN_IP = "0.0.0.0"  
LISTEN_PORT = 9004

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
try:
    sock.bind((LISTEN_IP, LISTEN_PORT))
    print(f"=== [DEBUG] Fake Node2 Listening on {LISTEN_IP}:{LISTEN_PORT} ===")
except Exception as e:
    print(f"ERROR Binding: {e}")
    exit()

while True:
    data, addr = sock.recvfrom(4096)
    print(f"\n[RECEIVED] Got packet from {addr}")
    print(f"    Length: {len(data)} bytes")
    print(f"    Hex: {data.hex()}")
    # 打印前20个字节看看是不是 DNS 包
    print(f"    Raw: {data[:20]}")
