#include "can-receive.hpp"

#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/j1939.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>

J1939RxMessage CanReceive::Receive(int socketFd, size_t maxPayloadSize) const
{
    if (socketFd < 0)
        throw std::invalid_argument("Invalid socket fd");

    if (maxPayloadSize == 0)
        throw std::invalid_argument("maxPayloadSize must be greater than 0");

    J1939RxMessage message;
    message.payload.resize(maxPayloadSize);

    struct sockaddr_can srcAddr{};
    socklen_t srcAddrLen = sizeof(srcAddr);

    const ssize_t recvBytes = recvfrom(socketFd,
                                       message.payload.data(),
                                       message.payload.size(),
                                       0,
                                       reinterpret_cast<struct sockaddr*>(&srcAddr),
                                       &srcAddrLen);
    if (recvBytes < 0)
        throw std::runtime_error(std::string("Failed to receive J1939 message: ") + std::strerror(errno));

    message.payload.resize(static_cast<size_t>(recvBytes));
    message.name = srcAddr.can_addr.j1939.name;
    message.pgn = srcAddr.can_addr.j1939.pgn;
    message.addr = srcAddr.can_addr.j1939.addr;

    return message;
}
