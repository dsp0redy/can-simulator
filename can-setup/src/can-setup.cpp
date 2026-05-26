#include "can-setup.hpp"
#include <cerrno>
#include <cstring>
#include <iostream>
#include <linux/can/j1939.h>
#include <linux/net_tstamp.h>

#ifndef SOF_TIMESTAMPING_OPT_CMSG
#define SOF_TIMESTAMPING_OPT_CMSG 0
#endif

#ifndef SOF_TIMESTAMPING_TX_ACK
#define SOF_TIMESTAMPING_TX_ACK 0
#endif

#ifndef SOF_TIMESTAMPING_TX_SCHED
#define SOF_TIMESTAMPING_TX_SCHED 0
#endif

#ifndef SOF_TIMESTAMPING_OPT_ID
#define SOF_TIMESTAMPING_OPT_ID 0
#endif

#ifndef SOF_TIMESTAMPING_RX_SOFTWARE
#define SOF_TIMESTAMPING_RX_SOFTWARE 0
#endif

bool CanSetup::setUp()
{
    constexpr uint8_t kLocalSourceAddress = 0x80;

    // reference https://docs.kernel.org/networking/j1939.html
    // Create a J1939 SocketCAN datagram socket
    m_socketId = socket(PF_CAN, SOCK_DGRAM, CAN_J1939);
    if (m_socketId < 0)
    {
        std::cerr << "Error opening socket" << std::endl;
        return false;
    }

    int sock_opt = SOF_TIMESTAMPING_OPT_CMSG |
                   SOF_TIMESTAMPING_TX_ACK |
                   SOF_TIMESTAMPING_TX_SCHED |
                   SOF_TIMESTAMPING_OPT_ID |
                   SOF_TIMESTAMPING_RX_SOFTWARE;

    if (setsockopt(m_socketId, SOL_SOCKET, SO_TIMESTAMPING, &sock_opt, sizeof(sock_opt)) < 0)
    {
        std::cerr << "Error enabling SO_TIMESTAMPING" << std::endl;
        close(m_socketId);
        m_socketId = -1;
        return false;
    }

    int canBroadcast = 1;
    if (setsockopt(m_socketId, SOL_SOCKET, SO_BROADCAST, &canBroadcast, sizeof(canBroadcast)) < 0)
    {
        std::cerr << "Error enabling SO_BROADCAST (errno=" << errno << ")" << std::endl;
        close(m_socketId);
        m_socketId = -1;
        return false;
    }

    // Bind the socket to the interface
    std::memset(&m_address, 0, sizeof(sockaddr_can));
    m_address.can_family = AF_CAN;
    m_address.can_ifindex = if_nametoindex("vcan0");
    if (m_address.can_ifindex == 0)
    {
        std::cerr << "Error finding CAN interface vcan0" << std::endl;
        close(m_socketId);
        m_socketId = -1;
        return false;
    }

    // J1939 TX requires a concrete local source address.
    m_address.can_addr.j1939.addr = kLocalSourceAddress;

    // Accept ALL PGNs
    m_address.can_addr.j1939.pgn = J1939_NO_PGN;

    // Accept ALL names
    m_address.can_addr.j1939.name = J1939_NO_NAME;

    // Bind the socket to the interface
    if (bind(m_socketId, (struct sockaddr *)&m_address, sizeof(m_address)) < 0)
    {
        std::cerr << "Error in binding socket (errno=" << errno << ")" << std::endl;
        close(m_socketId);
        m_socketId = -1;
        return false;
    }

    return true;
}

int CanSetup::getSocketID() const
{
    return m_socketId;
}