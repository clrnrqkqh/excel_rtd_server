#pragma once

#include <wtypes.h>

#include "../aw/logger.h"

#define AW_LOG(x) { std::stringstream ss; ss << x; std::string s(ss.str()); if (Configuration::instance().getVerbose()) aw::logger::log(s); }

class Configuration {
public:
	static Configuration& instance() {
		static Configuration __instance;
		return __instance;
	}

	static constexpr const char* command = "quote";

	auto init() -> bool
	{
		HKEY hkey;
		// registry format, HKEY_CLASSES_ROOT is one of 5 root dir that should have AwRTDServer.RTDFunctions in it after regsvr cmd
		if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT, "AwRTDServer.RTDFunctions", 0, KEY_READ, &hkey)) {
			return false;
		}
		std::string value;
		m_verbose = readRegistry(hkey, "Verbose", value) ? (value == "1" ? true : false) : true;
		m_log_dir = readRegistry(hkey, "LogDir", value) ? value : "E:\\AwRTDlog";
		m_multicast_group = readRegistry(hkey, "MulticastGroup", value) ? value : "";
		m_multicast_port = readRegistry(hkey, "MulticastPort", value) ? std::stoi(value) : 0;
		m_interface = readRegistry(hkey, "Interface", value) ? value : "";
		return true;
	}

	auto getShutdownHandle() -> HANDLE
	{
		return m_hShutdown;
	}

	auto getNotifyHandle() -> HANDLE
	{
		return m_hNotify;
	}

	auto getVerbose() -> bool
	{
		return m_verbose;
	}

	auto getLogDir() -> std::string { return m_log_dir; }

	auto getMulticastGroup() -> std::string { return m_multicast_group; }

	auto getMulticastPort() -> int { return m_multicast_port; }

	auto getInterface() -> std::string { return m_interface; }

	auto notifyCounter() -> uint64_t { return m_notify_counter; }
	auto notifyCounter(uint64_t val) -> void { m_notify_counter = val; }
	auto incrementNotify() -> uint64_t { return m_notify_counter++; }
	auto incrementRefresh() -> uint64_t { return m_refresh_counter++; }
	auto refreshCounter() -> uint64_t { return m_refresh_counter; }
	auto refreshCounter(uint64_t val) -> void { m_refresh_counter = val; }

	auto readVerbose() -> void { // update verbose if change in register
		HKEY hkey;
		// registry format, HKEY_CLASSES_ROOT is one of 5 root dir that should have AwRTDServer.RTDFunctions in it after regsvr cmd
		if (ERROR_SUCCESS != RegOpenKeyEx(HKEY_CLASSES_ROOT, "AwRTDServer.RTDFunctions", 0, KEY_READ, &hkey)) {
			return;
		}
		std::string value;
		m_verbose = readRegistry(hkey, "Verbose", value) ? (value == "1" ? true : false) : true;
	}

private:

	auto readRegistry(HKEY hkey, const char* token, std::string& value) -> bool
	{
		char buf[1024];
		DWORD bufsize = sizeof(buf);
		if (ERROR_SUCCESS != RegQueryValueEx(hkey, token, NULL, NULL, (LPBYTE)buf, &bufsize)) {
			return false;
		}
		value = buf;
		return true;
	}

	HANDLE m_hShutdown = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE m_hNotify = CreateEvent(NULL, FALSE, FALSE, NULL);
	std::string m_multicast_group;
	int m_multicast_port = 0;
	std::string m_interface;
	bool m_verbose = true;
	std::string m_log_dir = "E:\\aw\\var";
	std::atomic<uint64_t> m_notify_counter = 0;
	std::atomic<uint64_t> m_refresh_counter = 0;
};