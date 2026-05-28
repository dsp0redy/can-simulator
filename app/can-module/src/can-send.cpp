#include "can-send.hpp"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <linux/can.h>
#include <linux/can/j1939.h>
#include <linux/can/raw.h>
#include <thread>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
uint32_t BuildCanId(const J1939TxConfig& config, uint8_t sourceAddress)
{
    const uint32_t pgn = config.pgn & J1939_PGN_MAX;
    const uint8_t pf = static_cast<uint8_t>((pgn >> 8) & 0xFF);
    const uint8_t dp = static_cast<uint8_t>((pgn >> 16) & 0x01);
    const uint8_t ps = (pf < 240) ? config.addr : static_cast<uint8_t>(pgn & 0xFF);

    return CAN_EFF_FLAG |
           (static_cast<uint32_t>(config.priority & 0x07) << 26) |
           (static_cast<uint32_t>(dp) << 24) |
           (static_cast<uint32_t>(pf) << 16) |
           (static_cast<uint32_t>(ps) << 8) |
           sourceAddress;
}

ssize_t SendRawFallback(int socketFd, const uint8_t* data, size_t size, const J1939TxConfig& config)
{
    if (size > CAN_MAX_DLEN)
        throw std::runtime_error("Raw CAN fallback supports up to 8 bytes only");

    struct sockaddr_can boundAddr{};
    socklen_t boundLen = sizeof(boundAddr);
    if (getsockname(socketFd, reinterpret_cast<struct sockaddr*>(&boundAddr), &boundLen) < 0)
        throw std::runtime_error(std::string("Failed to query bound J1939 socket: ") + std::strerror(errno));

    const uint8_t sourceAddress = boundAddr.can_addr.j1939.addr;
    const int rawFd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (rawFd < 0)
        throw std::runtime_error(std::string("Failed to create raw CAN fallback socket: ") + std::strerror(errno));

    struct sockaddr_can rawBind{};
    rawBind.can_family = AF_CAN;
    rawBind.can_ifindex = boundAddr.can_ifindex;
    if (bind(rawFd, reinterpret_cast<struct sockaddr*>(&rawBind), sizeof(rawBind)) < 0)
    {
        close(rawFd);
        throw std::runtime_error(std::string("Failed to bind raw CAN fallback socket: ") + std::strerror(errno));
    }

    struct can_frame frame{};
    frame.can_id = BuildCanId(config, sourceAddress);
    frame.len = static_cast<__u8>(size);
    std::memcpy(frame.data, data, size);

    const ssize_t written = write(rawFd, &frame, sizeof(frame));
    const int savedErrno = errno;
    close(rawFd);

    if (written < 0)
    {
        errno = savedErrno;
        throw std::runtime_error(std::string("Failed to send raw CAN fallback frame: ") + std::strerror(errno));
    }

    return static_cast<ssize_t>(size);
}

ssize_t SendTpBamFallback(int socketFd, const uint8_t* data, size_t size, const J1939TxConfig& config)
{
    if (size == 0)
        throw std::invalid_argument("TP.BAM fallback payload must not be empty");

    constexpr size_t kDtPayloadSize = 7;
    constexpr uint32_t kTpCmPgn = 0x00EC00;
    constexpr uint32_t kTpDtPgn = 0x00EB00;
    constexpr uint8_t kGlobalAddress = 0xFF;
    constexpr uint8_t kBamControl = 32;

    const size_t packetCount = (size + kDtPayloadSize - 1) / kDtPayloadSize;
    if (packetCount > 255)
        throw std::runtime_error("TP.BAM fallback supports at most 255 DT packets");
    if (size > 0xFFFF)
        throw std::runtime_error("TP.BAM fallback supports payload up to 65535 bytes");

    struct sockaddr_can boundAddr{};
    socklen_t boundLen = sizeof(boundAddr);
    if (getsockname(socketFd, reinterpret_cast<struct sockaddr*>(&boundAddr), &boundLen) < 0)
        throw std::runtime_error(std::string("Failed to query bound J1939 socket: ") + std::strerror(errno));

    const uint8_t sourceAddress = boundAddr.can_addr.j1939.addr;
    const int rawFd = socket(PF_CAN, SOCK_RAW, CAN_RAW);
    if (rawFd < 0)
        throw std::runtime_error(std::string("Failed to create raw CAN TP fallback socket: ") + std::strerror(errno));

    struct sockaddr_can rawBind{};
    rawBind.can_family = AF_CAN;
    rawBind.can_ifindex = boundAddr.can_ifindex;
    if (bind(rawFd, reinterpret_cast<struct sockaddr*>(&rawBind), sizeof(rawBind)) < 0)
    {
        const int savedErrno = errno;
        close(rawFd);
        errno = savedErrno;
        throw std::runtime_error(std::string("Failed to bind raw CAN TP fallback socket: ") + std::strerror(errno));
    }

    struct can_frame cmFrame{};
    cmFrame.can_id = BuildCanId({.name = 0, .pgn = kTpCmPgn, .addr = kGlobalAddress, .priority = config.priority},
                                sourceAddress);
    cmFrame.len = 8;
    cmFrame.data[0] = kBamControl;
    cmFrame.data[1] = static_cast<uint8_t>(size & 0xFF);
    cmFrame.data[2] = static_cast<uint8_t>((size >> 8) & 0xFF);
    cmFrame.data[3] = static_cast<uint8_t>(packetCount & 0xFF);
    cmFrame.data[4] = 0xFF;
    cmFrame.data[5] = static_cast<uint8_t>(config.pgn & 0xFF);
    cmFrame.data[6] = static_cast<uint8_t>((config.pgn >> 8) & 0xFF);
    cmFrame.data[7] = static_cast<uint8_t>((config.pgn >> 16) & 0xFF);

    if (write(rawFd, &cmFrame, sizeof(cmFrame)) < 0)
    {
        const int savedErrno = errno;
        close(rawFd);
        errno = savedErrno;
        throw std::runtime_error(std::string("Failed to send TP.BAM CM frame: ") + std::strerror(errno));
    }

    for (size_t packetIndex = 0; packetIndex < packetCount; ++packetIndex)
    {
        struct can_frame dtFrame{};
        dtFrame.can_id = BuildCanId({.name = 0, .pgn = kTpDtPgn, .addr = kGlobalAddress, .priority = config.priority},
                                    sourceAddress);
        dtFrame.len = 8;
        dtFrame.data[0] = static_cast<uint8_t>(packetIndex + 1);
        std::memset(&dtFrame.data[1], 0xFF, kDtPayloadSize);

        const size_t payloadOffset = packetIndex * kDtPayloadSize;
        const size_t remaining = size - payloadOffset;
        const size_t copyBytes = (remaining < kDtPayloadSize) ? remaining : kDtPayloadSize;
        std::memcpy(&dtFrame.data[1], data + payloadOffset, copyBytes);

        if (write(rawFd, &dtFrame, sizeof(dtFrame)) < 0)
        {
            const int savedErrno = errno;
            close(rawFd);
            errno = savedErrno;
            throw std::runtime_error(std::string("Failed to send TP.DT frame: ") + std::strerror(errno));
        }

        // Keep TP.DT pacing modest so receivers can reassemble reliably.
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    close(rawFd);
    return static_cast<ssize_t>(size);
}
} // namespace

ssize_t CanSend::Send(int socketFd, const uint8_t* data, size_t size, const J1939TxConfig& config) const
{
    if (socketFd < 0)
        throw std::invalid_argument("Invalid socket fd");

    if (data == nullptr || size == 0)
        throw std::invalid_argument("Payload must not be empty");

    struct sockaddr_can dstAddr{};
    dstAddr.can_family = AF_CAN;
    dstAddr.can_ifindex = 0;
    dstAddr.can_addr.j1939.name = config.name;
    dstAddr.can_addr.j1939.pgn = config.pgn;
    dstAddr.can_addr.j1939.addr = config.addr;

    const ssize_t sentBytes = sendto(socketFd,
                                     data,
                                     size,
                                     0,
                                     reinterpret_cast<const struct sockaddr*>(&dstAddr),
                                     sizeof(dstAddr));
    if (sentBytes < 0)
    {
        if (errno == EPROTO)
        {
            if (size > CAN_MAX_DLEN)
                return SendTpBamFallback(socketFd, data, size, config);
            return SendRawFallback(socketFd, data, size, config);
        }

        throw std::runtime_error(std::string("Failed to send J1939 message: ") + std::strerror(errno));
    }

    return sentBytes;
}

ssize_t CanSend::Send(int socketFd, const std::vector<uint8_t>& data, const J1939TxConfig& config) const
{
    return Send(socketFd, data.data(), data.size(), config);
}
