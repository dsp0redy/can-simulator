#include "can-send.hpp"
#include "timestamp-utils.hpp"
#include <atomic>
#include <cerrno>
#include <iostream>
#include <net/if.h>

bool CanSend::sendPgnData(uint32_t pgn, const std::vector<uint8_t> &payload,
                          uint8_t destinationAddr, uint64_t destinationName)
{
    constexpr uint32_t kJ1939PgnMask = 0x3FFFFU;

    if (!m_canSetup)
    {
        std::cerr << "CAN setup is not initialized" << std::endl;
        return false;
    }

    int socketId = m_canSetup->getSocketID();
    if (socketId < 0)
    {
        std::cerr << "Invalid socket" << std::endl;
        return false;
    }
    if ((pgn & ~kJ1939PgnMask) != 0)
    {
        std::cerr << "Invalid PGN (must be 18-bit): 0x" << std::hex << pgn << std::dec << std::endl;
        return false;
    }
    if (payload.empty())
    {
        std::cerr << "Payload cannot be empty" << std::endl;
        return false;
    }

    struct sockaddr_can destination{};
    destination.can_family = AF_CAN;
    static std::atomic<unsigned int> vcan0IfIndex{0};
    unsigned int resolvedIfIndex = vcan0IfIndex.load(std::memory_order_relaxed);
    if (resolvedIfIndex == 0)
    {
        resolvedIfIndex = if_nametoindex("vcan0");
        if (resolvedIfIndex != 0)
        {
            vcan0IfIndex.store(resolvedIfIndex, std::memory_order_relaxed);
        }
    }
    destination.can_ifindex = static_cast<int>(resolvedIfIndex);
    if (destination.can_ifindex == 0)
    {
        std::cerr << "Error finding CAN interface vcan0" << std::endl;
        return false;
    }
    destination.can_addr.j1939.addr = destinationAddr;
    destination.can_addr.j1939.pgn = pgn;
    destination.can_addr.j1939.name = destinationName;

    struct iovec iov{};
    iov.iov_base = const_cast<uint8_t *>(payload.data());
    iov.iov_len = payload.size();

    struct msghdr msg{};
    msg.msg_name = &destination;
    msg.msg_namelen = sizeof(destination);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    ssize_t bytes = sendmsg(socketId, &msg, 0);
    if (bytes == static_cast<ssize_t>(payload.size()))
    {
        std::cout << "Sent J1939 PGN 0x" << std::hex << pgn << std::dec
                  << " Payload Length: " << payload.size() << std::endl;
    }
    else
    {
        std::cerr << "Error writing to socket (errno=" << errno << ")" << std::endl;
        return false;
    }

    // TX timestamping events are delivered through the socket error queue.
    char errData[256]{};
    char control[512]{};
    struct iovec errIov{};
    errIov.iov_base = errData;
    errIov.iov_len = sizeof(errData);

    struct msghdr errMsg{};
    errMsg.msg_iov = &errIov;
    errMsg.msg_iovlen = 1;
    errMsg.msg_control = control;
    errMsg.msg_controllen = sizeof(control);

    ssize_t errBytes = recvmsg(socketId, &errMsg, MSG_ERRQUEUE | MSG_DONTWAIT);
    if (errBytes >= 0)
    {
        printTimestampFromMsg(std::cout, errMsg, "TX timestamp: ", "\n");
    }
    else if (errno != EAGAIN && errno != EWOULDBLOCK)
    {
        std::cerr << "Error reading TX timestamp from error queue (errno=" << errno << ")" << std::endl;
    }

    return true;
}