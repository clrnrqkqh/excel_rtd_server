// logger.h
// Logger class implementation with circular queue
// circular queue (pro: lock free / con: size limited to template parameter)
// best way to implement circular queue is with size power of 2 because modulo can be substituted with bit mask
// between each flush, queue cannot overflow more than size (if overflow, will just ignore new log attempts)
// can log any object that has:
// for any data structure that sizeof works with, it doesn't do copy at all, it copies data structure (ptr, sizeof) into std::string
// inside the message format call, we typecast it back to a data strcture (no copy constructor which could be expensive)
// just reinterpreting the same block of binary data
// any data structure includes double, int, uint64_t etc.
// LogMsgStr used to handle exceptions to sizeof (char*, const char*, std:;string)
// std::enable_if_t and std:is_same<T, const char*> used to control template specialization
// - stream operator (for flushing)
#include <string>
#include <chrono>
#include <array>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>

#include "datetime.h"

#pragma once

namespace aw {
	using namespace aw_stream;
	namespace private_internal	// just internal impl, nobody should use it directly
	{
		// used to ensure power of two, throw compiler error otherwise
		// struct is_power_of_two verifies N at compiler time, throws error if not power of two
		// use this to check if a constant (value known at compiler time) meets certain conditions
		template<int N>
		struct is_power_of_two
		{
			enum { val = N && !(N & (N - 1)) }; // N is non zero and N&(N-1) is 0
			static_assert(val, "power of two required"); // compiler error if N is not power of 2
		};

		class LogMsgBase
		{
		public:
			virtual ~LogMsgBase() {} // destructor
			virtual auto message()->std::string const = 0; // virtual because polymorphism required in subclass
			auto timestamp() -> std::chrono::system_clock::time_point { return m_timestamp; } // record initial timestamp when data comes in before formatting
		private:
			std::chrono::system_clock::time_point m_timestamp = std::chrono::system_clock::now();
		};

		class LogMsgStr : public LogMsgBase // extend LogMsgBase
		{
		public:
			LogMsgStr(const char* str, size_t size) // get message actual type and record it
				: m_str(str, size)
			{}
			LogMsgStr(const std::string& str)
				: m_str(str)
			{}
			virtual auto message() -> std::string const // overriding base class's message function, type as string
			{
				return m_str;
			}
			virtual ~LogMsgStr() {} // destructor
		protected:
			std::string m_str;
		};

		template<typename LogMessageType> // wrapper class for unidentified types
		class LogMsg : public LogMsgStr // extend LogMsgBase
		{
		public:
			LogMsg(const LogMessageType& realmsg, size_t msgSize) // get message actual type and record it
				: LogMsgStr(reinterpret_cast<const char*>(&realmsg), msgSize)
			{}
			virtual auto message() -> std::string const // overriding base class's message function, type as string
			{
				auto* message = reinterpret_cast<const LogMessageType*>(m_str.c_str());
				std::stringstream ss;
				ss << static_cast<const LogMessageType&>(*message);
				return ss.str();
			}
			virtual ~LogMsg() {} // destructor
		};

		template<typename size_t Size = 1024> // size must be power of two, checked by is_power_of_two struct
		class LoggerImpl
		{
		public:
			LoggerImpl() {}

			auto init(const std::string& log_suffix, const std::string& log_folder, const bool create_flush) -> bool
			{
				if (m_init)
					return true;
				m_init = true;
				m_log_folder = log_folder;
				m_log_suffix = log_suffix;
//#if __cplusplus > 201700L
				std::filesystem::current_path(std::filesystem::current_path());
				std::filesystem::create_directory(m_log_folder);
//#endif
				// lambda way to create threads passing class member functions with parameters
				if (create_flush) {
					m_flush_thread = std::thread([=] { flush_loop(); }); // dedicate one thread to repeatedly flush
				}
				m_create_thread = create_flush;
				return true;
			}

			auto size() -> uint64_t const { return m_write - m_read; } // size from circular queue implementation
			auto capacity() -> size_t const { return m_capacity_mask; } // queue can hold Size-1 elements
			auto read_position() -> uint64_t const { return m_read; } // read position index
			auto write_position() -> uint64_t const { return m_write; } // write position index

			template<typename LogMessageType>
			std::enable_if_t<std::is_same<LogMessageType, const char*>::value, bool>
				log(LogMessageType& msg, size_t)
			{
				auto lmsg = std::make_unique<LogMsgStr>(msg, strlen(msg));
				if (size() > m_threshold) { // avoid multiple threads all writing when not enough space
					// ex: 5 threads, 3 spaces left, all 5 write => write over
					m_num_missed++;
					return false; // if m_write has run more than Size away from read, will be overwriting messages not yet flushed
				}
				uint64_t position = m_write.fetch_add(1); // adds 1 to m_write, returns m_write's previous value
				// (THREAD SAFE, ensures threads write to different indices and update m_write correctly)
				m_messages[position % m_capacity_mask] = std::move(lmsg);
				// write LogMsg object into m_messages circular queue
				return true;
			}
			template<typename LogMessageType>
			std::enable_if_t<std::is_same<LogMessageType, char*>::value, bool>
				log(LogMessageType& msg, size_t)
			{
				auto lmsg = std::make_unique<LogMsgStr>(msg, strlen(msg));
				if (size() > m_threshold) { // avoid multiple threads all writing when not enough space
					// ex: 5 threads, 3 spaces left, all 5 write => write over
					m_num_missed++;
					return false; // if m_write has run more than Size away from read, will be overwriting messages not yet flushed
				}
				uint64_t position = m_write.fetch_add(1); // adds 1 to m_write, returns m_write's previous value
				// (THREAD SAFE, ensures threads write to different indices and update m_write correctly)
				m_messages[position % m_capacity_mask] = std::move(lmsg);
				// write LogMsg object into m_messages circular queue
				return true;
			}

			template<typename LogMessageType>
			std::enable_if_t<std::is_same<LogMessageType, std::string>::value, bool>
				log(LogMessageType& msg, size_t)
			{
				auto lmsg = std::make_unique<LogMsgStr>(msg);
				if (size() > m_threshold) { // avoid multiple threads all writing when not enough space
					// ex: 5 threads, 3 spaces left, all 5 write => write over
					m_num_missed++;
					return false; // if m_write has run more than Size away from read, will be overwriting messages not yet flushed
				}
				uint64_t position = m_write.fetch_add(1); // adds 1 to m_write, returns m_write's previous value
				// (THREAD SAFE, ensures threads write to different indices and update m_write correctly)
				m_messages[position % m_capacity_mask] = std::move(lmsg);
				// write LogMsg object into m_messages circular queue
				return true;
			}
			template<typename LogMessageType>
			std::enable_if_t<!std::is_same<LogMessageType, std::string>::value && 
				!std::is_same<LogMessageType, const char*>::value &&
				!std::is_same<LogMessageType, char*>::value, bool>
				log(LogMessageType& msg, size_t msgSize)
			{
				auto lmsg = std::make_unique<LogMsg<LogMessageType>>(msg, msgSize);
				if (size() > m_threshold) { // avoid multiple threads all writing when not enough space
					// ex: 5 threads, 3 spaces left, all 5 write => write over
					m_num_missed++;
					return false; // if m_write has run more than Size away from read, will be overwriting messages not yet flushed
				}
				uint64_t position = m_write.fetch_add(1); // adds 1 to m_write, returns m_write's previous value
				// (THREAD SAFE, ensures threads write to different indices and update m_write correctly)
				m_messages[position % m_capacity_mask] = std::move(lmsg);
				// write LogMsg object into m_messages circular queue
				return true;
			}

			auto get_num_missed() -> uint64_t {
				return m_num_missed;
			}

			auto stop() -> void // stop flush thread loop
			{
				m_shutdown = true;
				m_flush_thread.join(); // join waits for thread to finish, m_shutdown ends flush_loop
				do_flush(); // empty queue
				m_init = false;
			}

			auto flush() -> void // calls do_flush, allows users to call do_flush
			{
				// if logger created thread for flushing and another thread calls flush, do not flush
				if (!m_create_thread && m_thread_on) {
					return;
				}
				do_flush();
			}

		protected:
			template<typename MsgType> // MsgType can be LogMsgStr or LogMsg as unique pointer
			auto do_log(MsgType& msg) -> bool
			{
				if (size() > m_threshold) { // avoid multiple threads all writing when not enough space
					// ex: 5 threads, 3 spaces left, all 5 write => write over
					m_num_missed++;
					return false; // if m_write has run more than Size away from read, will be overwriting messages not yet flushed
				}
				uint64_t position = m_write.fetch_add(1); // adds 1 to m_write, returns m_write's previous value
				// (THREAD SAFE, ensures threads write to different indices and update m_write correctly)
				m_messages[position % m_capacity_mask] = std::move(msg);
				// write LogMsg object into m_messages circular queue
				return true;
			}

			auto do_flush() -> void // dequeue msg from circular queue
			{
				std::string filename = createDatedFilename();
				std::ofstream file(filename, std::ios::out | std::ios::app);
				if (!file.is_open()) {
					return;
				}
				while ((m_last_write - m_read) != 0)
				{
					uint64_t r_position = m_read.fetch_add(1); // get read_position and move it up one (THREAD SAFE)
					LogMsgBase* msg = m_messages[r_position % m_capacity_mask].get(); // get message from read position
					file << msg->timestamp() << ":\t" << msg->message() << "\n";
					m_messages[r_position % m_capacity_mask] = nullptr; // call destructor of flushed LogMsg, avoids time in log function
				}
				m_last_write = m_write;
				file.close();
			}
			auto flush_loop() -> void // repeatedly call flush to move elements in queue to file
			{
				m_thread_on = true;
				while (!m_shutdown) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100)); // sleep time can be configured
					do_flush();
				}
				m_thread_on = false;
			}

			// creates dated filename including folder where LoggerImpl flushes to for today
			// ex: var/20201223.log
			auto createDatedFilename() -> std::string
			{
				auto in_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				auto* timeinfo = std::localtime(&in_time_t);
				std::stringstream ss;
				ss << m_log_folder << "/" << std::put_time(timeinfo, "%Y%m%d") << m_log_suffix << ".log";
				return ss.str();
			}

		private:
			// file io variables
			std::string m_log_suffix;
			std::string m_log_folder = "var";

			// ensure capacity as power of 2 (for circular queue modulo efficiency)
			is_power_of_two<Size> ensure_power_of_2;

			std::array<std::unique_ptr<LogMsgBase>, Size - 1> m_messages; // queue container, size-1 capacity
			size_t m_capacity_mask = Size - 1; // bit mask for for easy modulo
			std::atomic<uint64_t> m_read = 0; // atomic read and write position variables
			std::atomic<uint64_t> m_write = 0;
			uint64_t m_last_write = 0; // delayed write index to avoid read_index catching
			size_t m_threshold = m_capacity_mask > 100 ? m_capacity_mask - 100 : (Size - 1) / 2;
			std::atomic<uint64_t> m_num_missed = 0;

			std::thread m_flush_thread; // thread dedicated to flush_loop
			bool m_init = false;
			bool m_create_thread = false;
			std::atomic<bool> m_thread_on = false;
			std::atomic<bool> m_shutdown = false; // thread safe atomic keyword (instrinsic function) 
		};
	}

	template<typename size_t Size = 1024>
	class Logger // logger class, only needs one of these for entire program
	{
	public:
		auto static init(const std::string& log_suffix = "", const std::string& log_folder = "var", bool create_flush_thread = true) -> bool
		{
			// static init to ensure one Logger is made ever with io variables log_suffix and log_folder
			return getImpl().init(log_suffix, log_folder, create_flush_thread);
		}
		template<typename LogMessageType>
		auto static log(LogMessageType& msg, size_t msgSize = sizeof(LogMessageType)) -> bool
		{
			// call log on implementation of Logger with msg input
			return getImpl().log(msg, msgSize);
		}
		auto static flush() -> void
		{
			// call flush on implementation of Logger
			getImpl().flush();
		}
		auto static getImpl() -> private_internal::LoggerImpl<Size>&
		{
			static private_internal::LoggerImpl<Size> __impl; // __impl shouldn't be used by anyone else
			// return unique/static LoggerImpl (circular queue container with necessary update functions)
			return __impl;
		}
		auto static stop() -> void
		{
			// call flush on implementation of Logger
			getImpl().stop();
		}
		auto static getNumMissed() -> uint64_t
		{
			return getImpl().get_num_missed();
		}
	};

	using logger = Logger<8192>;
}