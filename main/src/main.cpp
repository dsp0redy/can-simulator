#include <iostream>
#include <thread>
#include <chrono>
#include <random>
#include <vector>
#include "can-setup.hpp"
#include "can-receive.hpp"
#include "can-send.hpp"

int main()
{
    std::cout << "CAN Simulator" << std::endl;

    auto canSetup = std::make_shared<CanSetup>();
    if (!canSetup->setUp())
    {
        std::cerr << "Error in socket setup" << std::endl;
        return -1;
    }

    auto canReceive = std::make_shared<CanReceive>(canSetup);

    canReceive->startReceiveThread();

    auto canSend = std::make_shared<CanSend>(canSetup);

    // Sample PGNs to exercise the J1939 path before DBC integration.
    std::vector<uint32_t> pgnList = {0x00F004, 0x00FEEE, 0x00FEF2};
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> byteDist(0, 255);

    for (uint32_t pgn : pgnList)
    {
        std::vector<uint8_t> payload(8);
        for (uint8_t &b : payload)
        {
            b = static_cast<uint8_t>(byteDist(rng));
        }

        if (!canSend->sendPgnData(pgn, payload))
        {
            std::cerr << "Failed to send PGN 0x" << std::hex << pgn << std::dec << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    canReceive->stopReceiveThread();
    
    return 0;
}