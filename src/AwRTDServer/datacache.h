#pragma once

#include <comdef.h>
#include <string>
#include <unordered_map>
#include <mutex>

#include "../aw/udp.h"
#include "udpdata.h"
#include "configuration.h"

struct Cell
{
	Cell() { VariantInit(&m_var); VariantInit(&m_topic_id); m_topic_id.vt = VT_EMPTY; }
	auto update(const VARIANT& var) -> void
	{
		AW_LOG("Cell update: topic<" << m_topic << "> topic_id<" << m_topic_id.lVal);
		m_var = var;
		m_changed = true;
	}

	auto ready() -> bool const
	{
		return (m_topic_id.vt != VT_EMPTY && m_changed);
	}

	auto get(std::vector<std::pair<VARIANT, VARIANT>>& data) -> void
	{
		data.push_back(std::make_pair(m_topic_id, m_var));
		m_changed = false;
	}

	auto set_topic(const std::string& topic, LONG topic_id) -> void
	{
		m_topic = topic;
		m_topic_id.vt = VT_I4;
		m_topic_id.lVal = topic_id;
	}

	bool m_changed = false;
	VARIANT m_var;
	VARIANT m_topic_id;
	std::string m_topic;
};

struct SymbolData
{
	SymbolData() {}
	
	SymbolData(const std::string& symbol_name) : m_symbol_name(symbol_name)
	{}

	// from excel side
	auto add(const std::string& topic, LONG topic_id) -> void
	{
		Cell& cell(m_fields[topic]);
		cell.m_topic = topic;
		cell.m_topic_id.vt = VT_I4;
		cell.m_topic_id.lVal = topic_id;
	}
	auto get(std::vector<std::pair<VARIANT, VARIANT>>& data) -> void
	{
		for (auto& it : m_fields)
		{
			if (it.second.ready())
			{
				it.second.get(data);
			}
		}
	}


	// from data source side
	auto add(const std::string& topic) -> void
	{
		Cell& cell(m_fields[topic]);
		cell.m_topic = topic;
	}
	auto update(const std::string topic, const VARIANT& var) -> void
	{
		AW_LOG("SymbolData update: symbol<" << m_symbol_name << "> topic<" << topic << "> var<" << var.vt);
		Cell& cell(m_fields[topic]);
		cell.update(var);
	}

	bool m_init = false;
	std::string m_symbol_name;
	std::unordered_map<std::string, Cell> m_fields;
};

class DataCache : public aw::IUDPListener
{
public:
	DataCache() {}

	auto start() -> bool
	{
		m_udp.addChannel(Configuration::instance().getInterface(), Configuration::instance().getMulticastGroup(), Configuration::instance().getMulticastPort(), this);
		return m_udp.start();
	}

	auto stop() -> bool
	{
		m_udp.stop();
		return true;
	}

	// access from excel side
	// all public functions should have lock
	auto add(const std::string& symbol, const std::string& topic, LONG topic_id) -> void
	{
		std::lock_guard<std::mutex> __(m_mutex);
		SymbolData& sd(m_symbols[symbol]);
		sd.m_symbol_name = symbol;
		sd.add(topic, topic_id);
	}
	auto get(std::vector<std::pair<VARIANT, VARIANT>>& data) -> void
	{
		std::lock_guard<std::mutex> __(m_mutex);
		for (auto& it : m_symbols)
		{
			it.second.get(data);
		}
	}
	// from data source side (can't update from excel)
	auto update(const std::string& symbol, const std::string& topic, const VARIANT& var) -> void
	{
		std::lock_guard<std::mutex> __(m_mutex);
		SymbolData& sd(add_no_lock(symbol, topic)); // sd cannot exist by itself
		sd.update(topic, var);
	}
	// from data source side (can't update from excel) for lists
	auto update(const std::string& symbol, const std::vector<std::pair<std::string, VARIANT>>& topic_var) -> void
	{
		std::lock_guard<std::mutex> __(m_mutex);
		for (uint32_t i = 0; i < topic_var.size(); i++) {
			SymbolData& sd(add_no_lock(symbol, topic_var[i].first)); // sd cannot exist by itself
			sd.update(topic_var[i].first, topic_var[i].second);
		}
	}

	// implement IUDPListener interface
	auto onData(const char* data, size_t size) -> void override
	{
		// assert(size == sizeof(EnhancedUDPData)); // sender may not fill all fields, may send less than 368 bytes
		AW_LOG("Data received");
		const auto* myData = reinterpret_cast<const EnhancedUDPData*>(data);
		aw::logger::log(*myData, size);
		/*
		#char m_symbol[24];
		uint64_t m_timestamp; // timestamp, microseconds from epoch
		#uint16_t m_num_fields; // how many fields stored <= 20
		#Field m_fields[20]; // can store up to 20 fields
		#char m_filler[4]; // align 8 bytes*/
		std::string symbol(myData->m_symbol);
		std::vector<std::pair<std::string, VARIANT>> topic_var;
		uint16_t num_fields(myData->m_num_fields);
		for (int32_t i = 0; i < num_fields; i++) {
			std::string topic(myData->m_fields[i].m_topic, sizeof(myData->m_fields[i].m_topic));
			VARIANT var;
			VariantInit(&var);
			if (myData->m_fields[i].m_type == 1) { // int64_t
				var.vt = VT_I8;
				var.llVal = myData->m_fields[i].m_val;
			}
			else {
				var.vt = VT_R8;
				var.dblVal = static_cast<double>(myData->m_fields[i].m_val) / SCALE;
			}
			topic_var.push_back(std::make_pair(topic, var));
		}
		std::chrono::system_clock::time_point timestamp = aw::get_time_point_from_mks_from_epoch(myData->m_timestamp);
		std::stringstream tStream;
		tStream << timestamp;
		std::string tms(tStream.str());
		VARIANT var;
		VariantInit(&var);
		var.vt = VT_BSTR;
		var.bstrVal = SysAllocString(_bstr_t(tms.c_str()));
		topic_var.push_back(std::make_pair("tms", var));
		update(symbol, topic_var);
		SetEvent(Configuration::instance().getNotifyHandle());
	}
	
private:
	// access from data source, avoid double lock in data source update
	auto add_no_lock(const std::string& symbol, const std::string& topic) -> SymbolData&
	{
		SymbolData& sd(m_symbols[symbol]);
		if (sd.m_init)
			return sd;
		sd.m_symbol_name = symbol;
		sd.add(topic);
		return sd;
	}

	std::unordered_map<std::string, SymbolData> m_symbols;
	std::mutex m_mutex;
	aw::UDPServer m_udp;
};