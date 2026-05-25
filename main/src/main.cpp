#include <iostream>
#include <thread>
#include <chrono>
#include "can-setup.hpp"
#include "can-receive.hpp"
#include "can-send.hpp"

int main(int argc, char *argv[])
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
    // frame_to_send.can_id = 0x5A1; // CAN ID (Hex)
    // frame_to_send.can_dlc = 4;    // Data length (4 bytes)
    // frame_to_send.data[0] = 0xAA;
    // frame_to_send.data[1] = 0xBB;
    // frame_to_send.data[2] = 0xCC;
    // frame_to_send.data[3] = 0xDD;
    std::vector<uint8_t> arr = {0XAA, 0XBB, 0XCC, 0XDD};
    uint32_t id = 0X5A1;
    uint8_t dlc = 4;
    canSend->createFrameAndSend(id, dlc, arr);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    canSend->createFrameAndSend(id, dlc, arr);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    canSend->createFrameAndSend(id, dlc, arr);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    canSend->createFrameAndSend(id, dlc, arr);
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // canReceive->stopReceiveThread();
    
    return 0;
}