#include "can-receive.hpp"
#include <cerrno>
#include <iostream>

CanReceive::~CanReceive()
{
    stopReceiveThread();
}

void CanReceive::receiveCanData()
{
    int socketId = m_canSetup->getSocketID();
    while (m_shouldRun)
    {
        int bytes = recv(socketId, &m_canFrame, sizeof(struct can_frame), MSG_DONTWAIT);

        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            std::cout << "can raw socket read" << std::endl;
            continue;
        }

        /* paranoid check ... */
        if (bytes < sizeof(struct can_frame))
        {
            std::cout << "read: incomplete CAN frame" << std::endl;
            continue;
        }

        // Print
        std::cout << "Received CAN Frame! ID: 0x" << std::hex << m_canFrame.can_id;
        std::cout << " Data: ";
        for (int i = 0; i < m_canFrame.can_dlc; i++)
        {
            std::cout << std::hex << (int)m_canFrame.data[i] << " ";
        }
        std::cout << std::dec << std::endl; // Reset to decimal format
    }
}

void CanReceive::startReceiveThread()
{
    if (m_receiveThread.joinable())
    {
        return;
    }

    m_shouldRun = true;
    m_receiveThread = std::thread(&CanReceive::receiveCanData, this);
}

void CanReceive::stopReceiveThread()
{
    m_shouldRun = false;
    if (m_receiveThread.joinable())
    {
        m_receiveThread.join();
    }
}