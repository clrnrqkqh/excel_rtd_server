#include <iostream>
#include <chrono>

#include "../aw/datetime.h"
#include "../aw/udp.h"
#include "../UDPServer/udpdata.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::cout << "enter <num>" << std::endl;
        exit(1);
    }
    int32_t numMessages = std::atoi(argv[1]);
    aw::UDPSender udp("", "239.9.61.1", 5000);
    auto rt = udp.start();
    std::cout << "udp.start: " << rt << std::endl;
    for (int32_t i = 0; i < numMessages; i++) {
        UDPData d("IBM", 100.01 + i, 2000.02 + i, std::chrono::system_clock::now());
        std::cout << udp.send(reinterpret_cast<const char*>(&d), sizeof(UDPData)) << std::endl;
    }
    std::cout << "exit" << std::endl;
    udp.stop();
    exit(0);
}