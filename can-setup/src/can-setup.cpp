#include "can-setup.hpp"
#include <cstring>
#include <sys/ioctl.h>
#include <iostream>

bool CanSetup::setUp()
{
    // reference https://docs.kernel.org/networking/can.html
    // Create a SocketCAN raw socket
    m_socketId = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (m_socketId < 0)
    {
        std::cerr << "Error opening socket" << std::endl;
        return false;
    }

    // Specify Interface
    strcpy(m_interface.ifr_name, "vcan0");
    if (ioctl(m_socketId, SIOCGIFINDEX, &m_interface) < 0)
    {
        std::cerr << "Error getting interface index for vcan0" << std::endl;
        close(m_socketId);
        m_socketId = -1;
        return false;
    }

    m_addrress.can_family = AF_CAN;
    m_addrress.can_ifindex = m_interface.ifr_ifindex;

    // Bind the socket to the interface
    if(bind(m_socketId, (struct sockaddr *)&m_addrress, sizeof(m_addrress)) < 0){
        std::cerr << "Error in binding socket" << std::endl;
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