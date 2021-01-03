#pragma once

#include "../aw/datetime.h"

constexpr double SCALE = 1000000000;

using namespace aw_stream;

#pragma pack(1)
struct EnhancedUDPData {
    struct Field {
        char m_topic[3]; // track data topic
        int8_t m_type; // 1: int64_t, 2: double
        int64_t m_val; // data is always represented as int64_t
    };

    template<typename Stream>
    friend Stream& operator <<(Stream& stream, const EnhancedUDPData& data)
    {
        stream << "{symbol<" << std::string(data.m_symbol) << ">, # of fields<"
            << data.m_num_fields << ">: ";
        for (int32_t i = 0; i < data.m_num_fields; i++) {
            stream << "field[" << i << "] topic<" << std::string(data.m_fields[i].m_topic, sizeof(data.m_fields[i].m_topic)) << "> value<" << data.m_fields[i].m_val << ">";
        }
        return stream;
    }

    char m_symbol[24];
    uint64_t m_timestamp; // timestamp, microseconds from epoch
    uint16_t m_num_fields; // how many fields stored
    Field m_fields[1]; // indeterminate number of fields (will be stored right after first one in sequential memory)
    // sizeof(EnhancedUDPData) may return incorrect size since it only accounts for first field (out of possibly more) 
};
#pragma pack()