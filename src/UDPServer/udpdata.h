#pragma once

#include "../aw/datetime.h"

constexpr double SCALE = 1000000000;

using namespace aw_stream;
struct UDPData
{
    UDPData(const char* symbol, double price, double quantity, const std::chrono::system_clock::time_point& timestamp)
    {
        memset(m_symbol, 0, sizeof(m_symbol));
        memcpy(&m_symbol, symbol, min(sizeof(m_symbol), strlen(symbol)));
        m_price = static_cast<int64_t>(price * SCALE);
        m_quantity = static_cast<uint64_t>(quantity * SCALE);
        m_timestamp = std::chrono::duration_cast<std::chrono::microseconds>(timestamp.time_since_epoch()).count();
    }

	template<typename Stream>
	friend Stream& operator <<(Stream& stream, const UDPData& data)
	{
        stream << "{symbol<" << std::string(data.m_symbol) << ">, price<"
            << static_cast<double>(data.m_price) / SCALE << ">, quantity<"
            << static_cast<double>(data.m_quantity) / SCALE << ">, timestamp<"
            << aw::get_time_point_from_mks_from_epoch(data.m_timestamp) << ">}";
		return stream;
	}

    char m_symbol[24];
    int64_t m_price; // 1 billion based, divide by billion to convert to normal double
    uint64_t m_quantity; // also 1 billion based
    uint64_t m_timestamp; // microseconds from epoch time
};

#pragma pack(1)
struct EnhancedUDPData {
    struct Field {
        char m_topic[4]; // track data topic
        int64_t m_val; // data is always represented as int64_t
    };

    template<typename Stream>
    friend Stream& operator <<(Stream& stream, const EnhancedUDPData& data)
    {
        stream << "{symbol<" << std::string(data.m_symbol, 24) << ">, # of fields<"
            << data.m_num_fields << ">: ";
        for (int32_t i = 0; i < data.m_num_fields; i++) {
            stream << "field[" << i << "] topic<" << std::string(data.m_fields[i].m_topic) << "> value<" << data.m_fields[i].m_val << ">}";
        }
        return stream;
    }

    char m_symbol[24];
    uint64_t m_timestamp; // timestamp, microseconds from epoch
    uint16_t m_num_fields; // how many fields stored <= 20
    Field m_fields[20]; // can store up to 20 fields
    char m_filler[4]; // align 8 bytes
};
#pragma pack()