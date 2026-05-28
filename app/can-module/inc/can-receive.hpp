#ifndef CAN_RECEIVE_HPP
#define CAN_RECEIVE_HPP

#include <cstddef>
#include <cstdint>
#include <vector>

struct J1939RxMessage
{
    uint64_t name{0};
    uint32_t pgn{0};
    uint8_t addr{0xFF};
    std::vector<uint8_t> payload;
};

class CanReceive
{
public:
    J1939RxMessage Receive(int socketFd, size_t maxPayloadSize = 1785) const;
};

#endif // CAN_RECEIVE_HPP
