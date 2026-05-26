#ifndef CAN_SEND_HPP
#define CAN_SEND_HPP

#include "can-setup.hpp"
#include <linux/can/j1939.h>
#include <memory>
#include <vector>

class CanSend
{
public:
    CanSend() = default;
    CanSend(std::shared_ptr<CanSetup> canSetup) : m_canSetup(canSetup) {}
    ~CanSend() = default;

    bool sendPgnData(uint32_t pgn, const std::vector<uint8_t> &payload,
                     uint8_t destinationAddr = J1939_NO_ADDR,
                     uint64_t destinationName = J1939_NO_NAME);

private:
    std::shared_ptr<CanSetup> m_canSetup;
};

#endif // CAN_SEND_HPP