#ifndef CAN_SEND_HPP
#define CAN_SEND_HPP

#include "can-setup.hpp"
#include <memory>
#include <vector>

class CanSend
{
public:
    CanSend() = default;
    CanSend(std::shared_ptr<CanSetup> canSetup) : m_canSetup(canSetup) {}
    ~CanSend() = default;

    bool sendCanData(const struct can_frame *canFrame);
    bool createFrameAndSend(const uint32_t canId, const uint8_t dlc, const std::vector<uint8_t> &canData);
private:
    std::shared_ptr<CanSetup> m_canSetup;
};

#endif // CAN_SEND_HPP