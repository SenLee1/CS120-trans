#  import socket
#  import sys
#  import struct
#  from scapy.all import *
#
#  # === 配置 ===
#  LISTEN_IP = "127.0.0.1"
#  LISTEN_PORT = 9003     # 接收来自 Node 2 C++ 的请求
#  CPP_IP = "127.0.0.1"
#  CPP_RECV_PORT = 9004   # 发回给 Node 2 C++ 的结果
#
#  # 真实世界的根服务器 (Root Hints)
#  ROOT_SERVERS = ["198.41.0.4", "199.9.14.201", "192.33.4.12", "199.7.91.13"]
#
#  def recursive_lookup(qname, qtype):
#      """
#      模拟递归查询过程：Root -> TLD -> Authoritative
#      """
#      # Target Name Server: Whom to ask now
#      target_ns = ROOT_SERVERS[0]
#      print(f"[*] Starting recursive lookup for {qname}")
#
#      for i in range(15): # 防止无限循环
#          try:
#              print(f"\t[Step {i}] Asking {target_ns} about {qname}...")
#
#              # 构造 DNS 查询包
#              query = DNS(rd=1, qd=DNSQR(qname=qname, qtype=qtype))
#              # 这里的目的端口是 53 (标准 DNS)
#              request_pkt = IP(dst=target_ns)/UDP(sport=RandShort(), dport=53)/query
#
#              # 发送并等待回复 (超时 2s)
#              reply_pkt = sr1(request_pkt, verbose=0, timeout=2)
#
#              if reply_pkt is None:
#                  print(f"\t[!] Timeout from {target_ns}, trying next root...")
#                  # 简单的轮询容错
#                  if len(ROOT_SERVERS) > 0:
#                      idx = (ROOT_SERVERS.index(target_ns) + 1) % len(ROOT_SERVERS) if target_ns in ROOT_SERVERS else 0
#                      target_ns = ROOT_SERVERS[idx]
#                  continue
#
#              if not reply_pkt.haslayer(DNS):
#                  continue
#
#              dns_layer = reply_pkt[DNS]
#
#              # 1. 找到了答案 (Answer Section)
#              if dns_layer.an is not None:
#                  print(f"\t[SUCCESS] Got Answer!")
#                  return dns_layer
#
#              # 2. 没找到，但有 Authority (NS) 和 Additional (AR)
#              found_next_ns = False
#              if dns_layer.ar is not None:
#                  for x in dns_layer.ar:
#                      # Type 1 = A Record (IPv4)
#                      if x.type == 1:
#                          target_ns = x.rdata
#                          found_next_ns = True
#                          break
#
#              if not found_next_ns:
#                   print("\t[!] No Glue Record. Falling back to 8.8.8.8 for simulation.")
#                   target_ns = "8.8.8.8"
#
#          except Exception as e:
#              print(f"\t[Error] {e}")
#              break
#
#      return None
#
#  def server_thread():
#      sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
#      sock.bind((LISTEN_IP, LISTEN_PORT))
#      print(f"[*] Recursive DNS Server listening on {LISTEN_IP}:{LISTEN_PORT}")
#
#      out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
#
#      while True:
#          try:
#              data, addr = sock.recvfrom(4096)
#
#              # 1. 解析 C++ 发来的原始 DNS 包
#              try:
#                  dns_req = DNS(data)
#                  qname = dns_req.qd.qname.decode('utf-8')
#                  qtype = dns_req.qd.qtype
#                  print(f"[RECV] Query for {qname} (Type {qtype})")
#              except:
#                  print("[RECV] Invalid DNS packet")
#                  continue
#
#              # 2. 执行递归查询
#              dns_resp_layer = recursive_lookup(qname, qtype)
#
#              if dns_resp_layer:
#                  # 3. 构造回复包
#                  dns_resp_layer.id = dns_req.id
#                  dns_resp_layer.qr = 1 # Response
#                  dns_resp_layer.ra = 1 # Recursion Available
#
#                  # === 【新增修复】 ===
#                  # 移除权威区和附加区，确保包的末尾就是 IP 地址，配合 Node 1 的简化解析
#                  dns_resp_layer.ns = None
#                  dns_resp_layer.ar = None
#                  # ===================
#
#                  resp_bytes = bytes(dns_resp_layer)
#
#                  # 4. 发回给 C++
#                  out_sock.sendto(resp_bytes, (CPP_IP, CPP_RECV_PORT))
#                  print(f"[SEND] Answer sent back to C++")
#
#          except Exception as e:
#              print(f"[Loop Error] {e}")
#
#  if __name__ == "__main__":
#      server_thread()
import socket
import sys
import struct
from scapy.all import *

# === 配置 ===
LISTEN_IP = "127.0.0.1"
LISTEN_PORT = 9003     
CPP_IP = "127.0.0.1"
# CPP_IP = "10.20.98.47"
CPP_RECV_PORT = 9004   

# 根服务器
ROOT_SERVERS = ["10.15.44.11", "1.1.1.1"]

def recursive_lookup(qname, qtype):
    # ... (这部分 recursive_lookup 函数保持不变，直接用你原来的即可) ...
    # 为了节省篇幅，这里省略 recursive_lookup 的代码
    # 请保留你之前能跑通的那版 recursive_lookup 逻辑
    target_ns = ROOT_SERVERS[0]
    print(f"[*] Starting recursive lookup for {qname}")
    
    for i in range(15):
        try:
            print(f"\t[Step {i}] Asking {target_ns} about {qname}...")
            query = DNS(rd=1, qd=DNSQR(qname=qname, qtype=qtype))
            request_pkt = IP(dst=target_ns)/UDP(sport=RandShort(), dport=53)/query
            reply_pkt = sr1(request_pkt, verbose=0, timeout=2)
            
            if reply_pkt is None:
                if len(ROOT_SERVERS) > 0:
                    idx = (ROOT_SERVERS.index(target_ns) + 1) % len(ROOT_SERVERS) if target_ns in ROOT_SERVERS else 0
                    target_ns = ROOT_SERVERS[idx]
                continue

            if not reply_pkt.haslayer(DNS): continue
            dns_layer = reply_pkt[DNS]
            
            # 1. Got Answer
            if dns_layer.an is not None:
                print(f"\t[SUCCESS] Got Answer!")
                return dns_layer 
            
            # 2. Iteration
            found_next_ns = False
            if dns_layer.ar is not None:
                for x in dns_layer.ar:
                    if x.type == 1: 
                        target_ns = x.rdata
                        found_next_ns = True
                        break
            if not found_next_ns:
                 print("\t[!] No Glue Record. Fallback to 8.8.8.8")
                 target_ns = "8.8.8.8"

        except Exception as e:
            print(f"\t[Error] {e}")
            break
    return None

def server_thread():
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((LISTEN_IP, LISTEN_PORT))
    print(f"[*] Recursive DNS Server listening on {LISTEN_IP}:{LISTEN_PORT}")
    
    out_sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    while True:
        try:
            data, addr = sock.recvfrom(4096)
            
            try:
                dns_req = DNS(data)
                qname = dns_req.qd.qname.decode('utf-8')
                qtype = dns_req.qd.qtype
                print(f"[RECV] Query for {qname} (Type {qtype})")
            except:
                print("[RECV] Invalid DNS packet")
                continue
            
            # 执行查询
            found_dns_layer = recursive_lookup(qname, qtype)
            
            if found_dns_layer and found_dns_layer.an:
                # === 关键修改：提取 IP 并手动重组包 ===
                
                # 1. 提取第一个 A 记录的 IP 地址
                final_ip = None
                # 遍历所有答案，找到第一个 Type A (IP地址)
                # Scapy 的 an 字段是一个链表结构，需要循环遍历
                current_rr = found_dns_layer.an
                while current_rr:
                    if current_rr.type == 1: # Type A
                        final_ip = current_rr.rdata
                        break
                    current_rr = current_rr.payload
                
                if final_ip:
                    print(f"\t[DEBUG] Extracted IP: {final_ip}")
                    
                    # 2. 重新构造一个干净的 DNS 回复包
                    # 只包含 Header, Question, 和一个 Answer
                    clean_resp = DNS(
                        id=dns_req.id,   # 保持 ID 一致
                        qr=1,            # Response
                        ra=1,            # Recursion Available
                        qd=dns_req.qd,   # 原始问题
                        an=DNSRR(rrname=qname, type='A', ttl=60, rdata=final_ip) # 构造标准 Answer
                    )
                    
                    resp_bytes = bytes(clean_resp)
                    try:
                        sent_len = out_sock.sendto(resp_bytes, (CPP_IP, CPP_RECV_PORT))
                        print(f"[DEBUG] socket.sendto executed. Bytes sent: {sent_len} -> {CPP_IP}:{CPP_RECV_PORT}")
                    except Exception as e:
                        print(f"[ERROR] Send failed: {e}")
                    # out_sock.sendto(resp_bytes, (CPP_IP, CPP_RECV_PORT))
                    print(f"[SEND] Clean Answer sent to C++ (Len: {len(resp_bytes)})")
                else:
                    print("\t[ERROR] Answer received but no Type A IP found (maybe CNAME only?)")

            else:
                print("\t[FAILED] Lookup failed or no answer found")
                
        except Exception as e:
            print(f"[Loop Error] {e}")

if __name__ == "__main__":
    server_thread()
