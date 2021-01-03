// test.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <stdint.h>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include <math.h>
#include <fstream>
#include <thread>

#include <filesystem>
#include <cstdlib>

#include "../aw/logger.h"
#include "../aw/datetime.h"

class MyTest
{
public:
    static MyTest& Singleton()
    {
        static MyTest __instance;
        return __instance;
    }

    void foo()
    {
        std::cout << "foo" << std::endl;
    }
};

// COPIED FROM LOGGER IMPL
// creates dated filename including folder where LoggerImpl flushes to for today
// ex: var/20201223.log
auto createDatedFilename() -> std::string
{
	auto in_time_t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	auto* timeinfo = std::localtime(&in_time_t);
	std::stringstream ss;
	ss << "var/" << std::put_time(timeinfo, "%Y%m%d") << ".log";
	return ss.str();
}

// ThreadTest
using namespace aw_stream;

struct ThreadTestLog
{
	ThreadTestLog() = delete; // don't need default constructor
	ThreadTestLog(std::thread::id tid, uint32_t tnum, uint32_t cnt) : m_thread_id(tid), m_thread_num(tnum), m_cnt(cnt)
	{}
	
	template<typename Stream>
	friend Stream& operator <<(Stream& stream, const ThreadTestLog& data) {
		stream << "thread id: " << data.m_thread_id << ", thread #: " << data.m_thread_num << ", count: " << data.m_cnt;
		return stream;
	}

	std::thread::id m_thread_id;
	uint32_t m_thread_num;
	uint32_t m_cnt;
};

class ThreadTest {
public:
	ThreadTest(uint32_t num_threads)
		:m_num_threads(num_threads)
	{
	}

	virtual ~ThreadTest() {} // destructor since subclass exists and uses this

	auto start() -> void
	{
		for (uint32_t i = 0; i < m_num_threads; i++) {
			// lambda way to create threads passing class member functions with parameters
			auto t = std::make_unique<std::thread>([=] { action(i + 1); });
			m_threads.push_back(std::move(t)); // unique pointer has to be moved, transfer ownership from t to m_threads
			// usually use std::move when putting unique pointer into container
		}
	}
	virtual auto stop() -> void // virtual for overridden functions
	{
		for (auto& t : m_threads) {
			t->join();
		}
	}
	virtual auto action(uint32_t id) -> void
	{
		// without following lock line with mutex class variable, action print statements are printed inside one another
		// this makes it print WHOLE statements in thread order but doesn't mangle the statements with each other
		// std::lock_guard<std::mutex> __(m_mutex); // "__" means no one should ever use the variable "__"
		std::cout << std::chrono::system_clock::now() << " inside action: " << id << " thread id: " << std::this_thread::get_id() << std::endl;
	}

protected: // subclass needs to access m_mutex
	uint32_t m_num_threads;
	std::vector<std::unique_ptr<std::thread>> m_threads;
	// std::mutex m_mutex;
};

class ThreadTestLoop : public ThreadTest {
public:
	ThreadTestLoop(uint32_t num_threads) : ThreadTest(num_threads)
	{
	}

	auto stop() -> void override // virtual for overridden functions
	{
		m_shutdown = true;
		ThreadTest::stop(); // call base class implementation
	}

	auto action(uint32_t id) -> void override
	{
		uint32_t cnt = 0;
		while (!m_shutdown) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
			ThreadTestLog ttl(std::this_thread::get_id(), id, cnt++);
			aw::logger::log(ttl);
			/*
			{ // restrict lock to this scope, let things outside of this {} to be free of lock
				std::lock_guard<std::mutex> __(m_mutex); // "__" means no one should ever use the variable "__"
				std::cout << std::chrono::system_clock::now() << " inside action: " << id << " thread id: " << std::this_thread::get_id() << std::endl;
			}
			*/
		}
		m_total_cnt += cnt;
	}

	auto get_total_count() -> uint64_t
	{
		return m_total_cnt;
	}

private:
	std::atomic<bool> m_shutdown = false; // thread safe atomic keyword (instrinsic function) 
	std::atomic<uint64_t> m_total_cnt = 0;
};

int main(int argc, char** argv)
{
    MyTest::Singleton().foo();

    if (argc < 3) {
        std::cout << "enter <num threads> <run time> please" << std::endl;
        exit(1);
    }
    auto num_threads = std::atoi(argv[1]);
    auto run_time = std::atoi(argv[2]);
    std::cout << "# of threads: " << num_threads << std::endl;
    std::cout << "run time (sec): " << run_time << std::endl;

	// remove var directory and contents in it (trash log files)
	// if log file isn't deleted, will be appending to it and overcount number of messages recorded
	std::filesystem::current_path(std::filesystem::current_path());
	std::filesystem::remove_all("var");

    aw::logger::init();
	/* shows we can log const char*, char*, and integer and any other data structure
	* not counted by testLoop
	const char* s1 = "this is const char*";
	char buf[24];
	strcpy(buf, "this is char*");
	char* s2 = buf;
	uint64_t i3 = 1234567;
	aw::logger::log(s1);
	aw::logger::log(s2);
	aw::logger::log(i3);
	*/
    // ThreadTestLoop

    ThreadTestLoop testLoop(num_threads); // creates object on stack
    testLoop.start();
    std::this_thread::sleep_for(std::chrono::seconds(run_time));
    testLoop.stop();
    aw::logger::stop();

    // Read from the text file
    std::string myText;
	std::string readFile = createDatedFilename();
	std::cout << "READ FILE " << readFile << std::endl;
    std::ifstream MyReadFile(readFile);
    uint64_t num_recorded = 0;
    // Use a while loop together with the getline() function to read the file line by line
    while (getline(MyReadFile, myText)) {
        num_recorded++;
    }
    // Close the file
    MyReadFile.close();

	uint64_t num_counted = aw::logger::getNumMissed() + num_recorded;
	uint64_t num_uncounted = testLoop.get_total_count() - num_counted;
    std::cout << "number of recorded messages: " << num_recorded << std::endl;
    std::cout << "number of missed messages: " << aw::logger::getNumMissed() << std::endl;
    std::cout << "total counted: " << num_counted << std::endl;
    std::cout << "number of total messages: " << testLoop.get_total_count() << std::endl;
    std::cout << "difference is: " << num_uncounted << std::endl;

	if (num_uncounted != 0) {
		exit(-1);
	}
    exit(0);
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
