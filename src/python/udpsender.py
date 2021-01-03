import socket
import struct
import time

def generateUDPData(symbol, price, quantity):
	rt = bytearray()
	symbolLen = len(symbol)
	rt += bytes(symbol, "ascii")
	for i in range(24-symbolLen):
		rt += struct.pack('b',0)
	prc = int(price * 1000000000)
	qty = int(quantity * 1000000000)
	ts = time.time() * 1000000
	rt += struct.pack('qqq', prc, qty, int(ts))
	return rt


addr = ('239.9.61.1', 5000)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# set time-to-live for messages to 1 so they don't go past the local network segment
ttl = struct.pack('b', 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)

for i in range(5):
	message = generateUDPData('IBM', 100.01 + i, 200 + i)
	sock.sendto(message, addr)
sock.close()