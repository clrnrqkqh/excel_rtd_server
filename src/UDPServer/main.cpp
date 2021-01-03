#include <iostream>
#include <chrono>

#include "../aw/datetime.h"
#include "../aw/udp.h"
#include "udpdata.h"

struct Listener : public aw::IUDPListener
{
    auto onData(const char* data, size_t size) -> void override
    {
        assert(size == sizeof(UDPData));
        // static_cast is for types known at compile time
        // dynamic_cast is for inheritance up/down casting
        // reinterpret_cast pointer from one type to pointer of another type
        const auto* myData = reinterpret_cast<const UDPData*>(data);
        std::cout << reinterpret_cast<const UDPData&>(*myData) << std::endl;
    }
};

struct EnhancedListener : public aw::IUDPListener
{
	// implement IUDPListener interface
	auto onData(const char* data, size_t size) -> void override
	{
		// assert(size == sizeof(EnhancedUDPData)); // sender may not fill all fields, may send less than 368 bytes
		std::cout << "DATA RECEIVED" << std::endl;
		const auto* myData = reinterpret_cast<const EnhancedUDPData*>(data);
		/*
		#char m_symbol[24];
		uint64_t m_timestamp; // timestamp, microseconds from epoch
		#uint16_t m_num_fields; // how many fields stored <= 20
		#Field m_fields[20]; // can store up to 20 fields
		#char m_filler[4]; // align 8 bytes*/
		std::string symbol(myData->m_symbol);
		std::vector<std::pair<std::string, int64_t>> topic_var;
		uint16_t num_fields(myData->m_num_fields);
		for (int32_t i = 0; i < num_fields; i++) {
			std::string topic(myData->m_fields[i].m_topic);
			int64_t val = myData->m_fields[i].m_val;
			topic_var.push_back(std::make_pair(topic, val));
		}
		std::string tms; // implement timestamp later
		std::cout << "SYMBOL<" << symbol << "> topic_var: ";
		for (int32_t i = 0; i < topic_var.size(); i++) {
			std::cout << "topic[" << i << "]<" << topic_var[i].first << "> var[" << i << "]<" << topic_var[i].second << ">";
		}
		std::cout << std::endl;
	}
};

int main(int argc, char** argv)
{
    std::cout << "size of Enhanced UDP Data: " << sizeof(EnhancedUDPData) << std::endl;
    aw::UDPServer udp;
    EnhancedListener listener;
    udp.addChannel("", "239.9.61.1", 5000, &listener);
    auto rt = udp.start();
    std::cout << "udp.start: " << rt << std::endl;
    std::cin.get();
    std::cout << "exit" << std::endl;
    udp.stop();
    exit(0);
}