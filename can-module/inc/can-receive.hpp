#ifndef CAN_RECEIVE_HPP
#define CAN_RECEIVE_HPP

#include "can-setup.hpp"
#include <atomic>
#include <memory>
#include <thread>

class CanReceive
{
public:
    CanReceive() = default;
    CanReceive(std::shared_ptr<CanSetup> canSetup) : m_canSetup(canSetup) {}
    ~CanReceive();

    void receiveCanData();
    void startReceiveThread();
    void stopReceiveThread();

private:
    std::shared_ptr<CanSetup> m_canSetup;
    std::atomic<bool> m_shouldRun{false};
    std::thread m_receiveThread;
};

#endif // CAN_RECEIVE_HPP