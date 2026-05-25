#include "can-send.hpp"
#include <cstring>
#include <iostream>

bool CanSend::sendCanData(const struct can_frame *canFrame)
{
    int socketId = m_canSetup->getSocketID();

    int bytes = write(socketId, canFrame, sizeof(struct can_frame));
    if (sizeof(struct can_frame) == bytes)
        std::cout << "Sent CAN Frame! ID: 0x" << std::hex << canFrame->can_id << " Data Length: " << (int)canFrame->can_dlc << std::endl;
    else
    {
        std::cerr << "Error writing to socket" << std::endl;
        return false;
    }
    return true;
}

bool CanSend::createFrameAndSend(const uint32_t canId, const uint8_t dlc, const std::vector<uint8_t> &canData)
{
    // Linux CAN frame layout reference:
    // struct can_frame {
    //     canid_t can_id;
    //     union {
    //         __u8 len;
    //         __u8 can_dlc; // deprecated
    //     };
    //     __u8 __pad;
    //     __u8 __res0;
    //     __u8 len8_dlc;
    //     __u8 data[8] __attribute__((aligned(8)));
    // };

    struct can_frame canFrame;
    std::memset(&canFrame, 0, sizeof(struct can_frame));
    if (dlc != canData.size() || dlc > 8)
    {
        std::cerr << "Error in data length" << std::endl;
        return false;
    }
    canFrame.can_id = canId;
    canFrame.can_dlc = dlc;
    for (size_t it = 0; it < static_cast<size_t>(dlc); it++)
    {
        canFrame.data[it] = canData[it];
    }

    if (!sendCanData(&canFrame))
        return false;

    return true;
}