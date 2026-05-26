#ifndef CAN_SETUP_HPP
#define CAN_SETUP_HPP

#include <unistd.h>

// Core Networking Headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>

// Linux CAN Specific Headers
#include <linux/can.h>
#include <linux/can/raw.h>

class CanSetup
{
public:
    CanSetup() = default;
    ~CanSetup()
    {
        if (m_socketId >= 0)
        {
            close(m_socketId);
        }
    }

    bool setUp();
    int getSocketID() const;

private:
    int m_socketId = -1;
    struct sockaddr_can m_address{};
    // struct ifreq m_interface{};
};

#endif // CAN_SETUP_HPP
