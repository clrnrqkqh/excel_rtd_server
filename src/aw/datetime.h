// datetime.h
// some date time related utility using chrono

#pragma once
#pragma warning(disable: 4996)

#include <chrono>
#include <string>
#include <ctime>
#include <iomanip> // put_time for formatting
#include <sstream>

namespace aw
{
	inline auto get_time_point_from_mks_from_epoch(uint64_t mks) -> std::chrono::system_clock::time_point
	{
		using d_mks = std::chrono::duration<uint64_t, std::ratio<1, 1000000>>;
		d_mks mks_since_epoch(mks);
		auto since_epoch = std::chrono::duration_cast<std::chrono::system_clock::duration>(mks_since_epoch);
		return std::chrono::system_clock::time_point(since_epoch);
	}
}

// stream function allows std::cout << and any other streaming function to take this data type
namespace aw_stream
{
	template<typename Stream>
	inline Stream& operator <<(Stream& stream, const std::chrono::system_clock::time_point& tp)
	{
		auto in_time_t = std::chrono::system_clock::to_time_t(tp);
		auto* timeinfo = std::localtime(&in_time_t);
		std::chrono::microseconds mks = std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch());
		std::string mks_str = std::to_string(mks.count() % 1000000);
		stream << std::put_time(timeinfo, "%Y-%m-%d %H:%M:%S.");
		if (mks_str.length() < 6)
			stream << std::string(6 - mks_str.length(), '0');
		stream << mks_str;
		return stream;
	}
}
