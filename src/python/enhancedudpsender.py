import socket
import struct
import time
import yfinance as yf

"""
#char m_symbol[24];
uint64_t m_timestamp; // timestamp, microseconds from epoch
#uint16_t m_num_fields; // how many fields stored <= 20
#Field m_fields[20]; // can store up to 20 fields
#char m_filler[4]; // align 8 bytes*/
"""
def generateEnhancedUDPData(symbol, bidprice, askprice, volume):
	rt = bytearray()
	symbolLen = len(symbol)
	rt += bytes(symbol, "ascii")
	for i in range(24-symbolLen):
		rt += struct.pack('b',0)
	ts = time.time() * 1000000
	rt += struct.pack('q', int(ts))
	rt += struct.pack('h', int(3))

	rt += bytes("bid", "ascii")
	rt += struct.pack('b', 2)
	bp = int(bidprice * 1000000000)
	rt += struct.pack('q', bp)
	rt += bytes("ask", "ascii")
	rt += struct.pack('b', 2)
	ap = int(askprice * 1000000000)
	rt += struct.pack('q', ap)
	rt += bytes("vol", "ascii")
	rt += struct.pack('b', 1)
	rt += struct.pack('q', volume)
	return rt

	

addr = ('239.9.61.1', 5000)
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
# set time-to-live for messages to 1 so they don't go past the local network segment
ttl = struct.pack('b', 1)
sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, ttl)

lst = ["IBM", "MSFT"]

for i in range(5):
	message = generateEnhancedUDPData('IBM', 100.01 + i, 200.02 + i, 1000000 + i)
	sock.sendto(message, addr)
time.sleep(1)

for i in range(10):
	for symbol in lst:
		ticker = yf.Ticker(symbol)
		# print(str(ticker.info))
		bid = ticker.info.get('bid', 0.0)
		ask = ticker.info.get('ask', 0.0)
		vol = ticker.info.get('volume', 0.0)
		print("%s: bid: %s ask %s volume: %s" %(symbol, bid, ask, vol))
		message = generateEnhancedUDPData(symbol, bid, ask, vol)
		sock.sendto(message, addr)
	time.sleep(1)
sock.close()


