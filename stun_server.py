import socket
import sys
import json
import pickle

server_port = None
room_size = 0
if len(sys.argv) != 3:
  print("Usage: python3 stun.py {VSWITCH_PORT} {ROOM_SIZE}")
  sys.exit(1)
else:
  server_port = int(sys.argv[1])
  room_size = int(sys.argv[2])
  

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
server_addr = ("0.0.0.0", server_port)
sock.bind(server_addr)
print(f"[STUN] Started at {server_addr[0]}:{server_addr[1]}")

peers = []

while True:

    data, vport_addr = sock.recvfrom(1518)
    print("received connection from ", vport_addr)
    if(vport_addr not in peers):
        peers.append(vport_addr)
    if(len(peers) == room_size):
        break


for peer in peers:
   sentpeers = []
   for addr in peers:
      if(addr != vport_addr):
         sentpeers.append(addr)
   for i in range(0,3):
      sock.sendto(json.dumps(sentpeers).encode('utf-8'),peer)
