#ifndef CAN_SEND_HPP
#define CAN_SEND_HPP

#include <cstddef>
#include <cstdint>
#include <sys/types.h>
#include <vector>

struct J1939TxConfig
{
    uint64_t name{0};
    uint32_t pgn{0};
    uint8_t addr{0xFF};
    uint8_t priority{6};
};

class CanSend
{
public:
    ssize_t Send(int socketFd, const uint8_t* data, size_t size, const J1939TxConfig& config) const;
    ssize_t Send(int socketFd, const std::vector<uint8_t>& data, const J1939TxConfig& config) const;
};

#endif // CAN_SEND_HPP
