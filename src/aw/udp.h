// udp.h
// cross platform implementation (windows and mac, should work in linux)
// UDPServer
//	IUDPListener
//	UDPChannel
// UDPSender

#pragma once

#ifdef _WIN64
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <MSWSock.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <fcntl.h>
#include <unistd.h>
#endif

#include <string>
#include <vector>
#include <assert.h>
#include <thread>
#include <atomic>

namespace aw
{
#ifdef _WIN64
#define CLOSE_SOCKET closesocket
#else
#define SOCKET int
#define INVALID_SOCKET -1
#define CLOSE_SOCKET close
#endif
	// I is for interface
	class IUDPListener
	{
	public:
		virtual auto onData(const char* data, size_t size) -> void = 0; // "empty function"
	};

	namespace internal_only
	{
		// specifies UDP channel (intf is interface NIC (empty is all NIC), addrGroup is multicast address,
		// port is the port, listener is consumer callback pointer)
		// ex: 127.0.0.1, 233.61.2.0, 5000, ptr
		struct UDPChannel
		{
			UDPChannel(const std::string& intf, const std::string& addrGroup, int port, IUDPListener* listener)
				: m_intf(intf), m_addrGroup(addrGroup), m_port(port), m_listener(listener)
			{
				m_imreq = {}; // equivalent new way of memset(&m_imreq, 0, sizeof(ip_mreq)) for any data structure
				inet_pton(AF_INET, m_addrGroup.c_str(), &m_imreq.imr_multiaddr.s_addr);
				intf.empty() ? m_imreq.imr_interface.s_addr = INADDR_ANY : inet_pton(AF_INET, intf.c_str(), &m_imreq.imr_interface.s_addr);
			}

			std::string m_intf;
			std::string m_addrGroup;
			int m_port;
			struct ip_mreq m_imreq;
			SOCKET m_sock = INVALID_SOCKET;

			IUDPListener* m_listener = nullptr;
		};

#ifdef _WIN64
		// WSA is Windows Socket (for Windows, need to initialize Windows Socket Library)
		// need to call WSAStartup/WSACleanup pair
		class WSA
		{
		public:
			// allocate in constructor (always paired since creating object guarantees calling destructor)
			WSA()
			{
				WSADATA wsaData;
				int rt = WSAStartup(WINSOCK_VERSION, &wsaData);
				assert(rt == 0);
			}
			// deallocate in destructor
			~WSA()
			{
				WSACleanup();
			}
		};
#endif
	}

	class UDPServer
	{
	public:
		UDPServer()
		{
#ifdef _WIN64
			static internal_only::WSA __wsa; // must be called once, avoid using this ever again
#endif
		}
		~UDPServer() {}

		auto addChannel(const std::string& intf, const std::string& addrGroup, int port, IUDPListener* listener) -> void
		{
			m_channels.push_back(internal_only::UDPChannel(intf, addrGroup, port, listener));
		}

		void dropChannels() 
		{//do nothing
		}

		auto start() -> bool
		{
			m_shutdown = false;
			// windows: just create one socket and join multiple address group
			// linux: create multiple socket (each one binds with mcast address group)
			// alwasy create new socket
			for (auto& channel : m_channels)
			{
				SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
				if (sock == INVALID_SOCKET) { // if socket creation failed get otu
					return false;
				}
				int flag(1);
				auto rt = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)); // set socket option 1, reuse address
				// set content of struct saddr and imreq to 0
				sockaddr_in saddr = {};
				saddr.sin_family = AF_INET;
				saddr.sin_port = htons(channel.m_port);
				saddr.sin_addr.s_addr = INADDR_ANY; // always bind to any but specify intf when joining mcast group
				// bind
				if (bind(sock, (sockaddr*)&saddr, sizeof(sockaddr_in)) != 0)
				{
					return false;
				}
				rt = setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (const char*)&channel.m_imreq, sizeof(ip_mreq));
				channel.m_sock = sock;
			}
			// create thread to trigger run
			m_thread = std::thread([=] { run(); });
			return true;
		}

		auto stop() -> void
		{
			m_shutdown = true;
			m_thread.join();
		}

	protected:
		auto run() -> void
		{
			fd_set set; // file descriptor set
			size_t size(m_channels.size());
			char buf[1800]; // UDP packet size always less than 1600 bytes
			int byteCount(0);
			sockaddr_in addr;
			socklen_t addrlen(sizeof(addr));
			timeval timeout;
			int max_sock(0); // largest of created sockets
			for (auto& channel : m_channels)
			{
				if (channel.m_sock > max_sock)
				{
					max_sock = static_cast<int>(channel.m_sock);
				}
			}

			while (true)
			{
				// every time have to reinitialize set and timeout
				FD_ZERO(&set);
				for (auto& channel : m_channels)
				{
					FD_SET(channel.m_sock, &set);
				}
				timeout.tv_sec = 0;
				timeout.tv_usec = 1000;

				// return codes:
				// -1: error occurred
				// 0: timeout (no data)
				// > 0: data read to be read
				// max_sock + 1 is number of file descriptors
				// &timeout since select uses &timeout to return how long it took to return, need to reinitialize every iteration
				auto rt = select(max_sock + 1, &set, 0, 0, &timeout);
				if (m_shutdown)
				{
					dropChannels();
					for (auto& channel : m_channels)
					{
						CLOSE_SOCKET(channel.m_sock);
					}
					return;
				}

				if (rt < 0) // error occurred
					return; // maybe log error

				if (rt == 0)
					continue; // everything fine, no data

				// have data in some channel
				for (auto& channel : m_channels)
				{
					if (FD_ISSET(channel.m_sock, &set)) // something is changed
					{
						// addr is incoming address who sent it
						byteCount = recvfrom(channel.m_sock, buf, sizeof(buf), 0, (sockaddr*)&addr, &addrlen);
						if (byteCount > 0 && channel.m_listener)
						{
							channel.m_listener->onData(buf, byteCount);
						}
					}
				}
			}
		}

	private:
		std::vector<internal_only::UDPChannel> m_channels;
		std::atomic<bool> m_shutdown = false;
		std::thread m_thread;
	};

	class UDPSender
	{
	public:
		// specifies UDP channel (intf is interface NIC (empty is all NIC), addrGroup is multicast address,
		// port is the port
		UDPSender(const std::string& intf, const std::string& addrGroup, int port, int ttl = 1)
			: m_intf(intf), m_addrGroup(addrGroup), m_port(port), m_ttl(ttl)
		{
#ifdef _WIN64
			static internal_only::WSA __wsa; // must be called once, avoid using this ever again
#endif
		}

		auto start() -> bool
		{
			m_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
			if (m_sock == INVALID_SOCKET) { // if socket creation failed get otu
				return false;
			}
			int flag(1);
			auto rt = setsockopt(m_sock, SOL_SOCKET, SO_REUSEADDR, (char*)&flag, sizeof(flag)); // set socket option 1, reuse address
			rt = setsockopt(m_sock, IPPROTO_IP, IP_TTL, (char*)&m_ttl, sizeof(m_ttl));
			// setting up NIC card address
			sockaddr_in addr = {}; // this is NIC/interface card address
			addr.sin_family = AF_INET;
			addr.sin_port = htons(m_port);
			m_intf.empty() ? addr.sin_addr.s_addr = INADDR_ANY : inet_pton(AF_INET, m_intf.c_str(), &addr.sin_addr.s_addr);
			// bind our socket to NIC card
			if (bind(m_sock, (sockaddr*)&addr, sizeof(sockaddr_in)) != 0)
			{
				return false;
			}
			// setting up multicast destination address
			m_mc_addr.sin_family = AF_INET;
			m_mc_addr.sin_port = htons(m_port); // m_port is integer, convert to network byte order (big/little endianness)
			// check if m_addrGroup (multicast destination)
			assert(!m_addrGroup.empty()); // if empty, cannot send to entire world
			inet_pton(AF_INET, m_addrGroup.c_str(), &m_mc_addr.sin_addr.s_addr);
			return true;
		}

		auto send(const char* data, size_t size) -> int
		{
			return sendto(m_sock, data, static_cast<int>(size), 0, reinterpret_cast<const struct sockaddr*>(&m_mc_addr), sizeof(sockaddr_in));
		}

		auto stop() -> void
		{
			CLOSE_SOCKET(m_sock);
		}

		~UDPSender()
		{
			stop();
		}
	private:
		std::string m_intf;
		std::string m_addrGroup;
		int m_port = 0;
		int m_ttl = 1; // time to live default 1
		sockaddr_in m_mc_addr = {}; // this is multicast destination address
		SOCKET m_sock = INVALID_SOCKET;
	};
}