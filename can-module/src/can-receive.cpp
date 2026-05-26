#include "can-receive.hpp"
#include "timestamp-utils.hpp"
#include <cerrno>
#include <chrono>
#include <iostream>
#include <linux/can/j1939.h>

CanReceive::~CanReceive()
{
    stopReceiveThread();
}

void CanReceive::receiveCanData()
{
    int socketId = m_canSetup->getSocketID();
    if (socketId < 0)
    {
        std::cerr << "Invalid socket" << std::endl;
        return;
    }

    char control[512]{};
    uint8_t payload[256]{};
    struct sockaddr_can source{};

    struct iovec iov{};
    iov.iov_base = payload;
    iov.iov_len = sizeof(payload);

    struct msghdr msg{};
    msg.msg_name = &source;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;

    while (m_shouldRun)
    {
        // recvmsg updates these lengths; reset before each call.
        msg.msg_namelen = sizeof(source);
        msg.msg_controllen = sizeof(control);

        ssize_t bytes = recvmsg(socketId, &msg, MSG_DONTWAIT);

        if (bytes < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                continue;
            }
            if (errno == EINTR)
            {
                continue;
            }
            std::cerr << "Error reading from J1939 socket (errno=" << errno << ")" << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        if (bytes == 0)
        {
            continue;
        }

        if ((msg.msg_flags & MSG_TRUNC) != 0)
        {
            std::cerr << "Received payload was truncated" << std::endl;
        }
        if ((msg.msg_flags & MSG_CTRUNC) != 0)
        {
            std::cerr << "Received ancillary data was truncated" << std::endl;
        }

        std::cout << "Received J1939 message PGN: 0x" << std::hex
                  << source.can_addr.j1939.pgn
                  << " SRC: 0x" << static_cast<int>(source.can_addr.j1939.addr)
                  << " Data: ";
        for (ssize_t i = 0; i < bytes; i++)
        {
            std::cout << std::hex << static_cast<int>(payload[i]) << " ";
        }
        printTimestampFromMsg(std::cout, msg, " RX timestamp: ");
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