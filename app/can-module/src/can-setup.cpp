#include "can-setup.hpp"
#include <cerrno>
#include <cstring>
#include <linux/can.h>
#include <linux/can/j1939.h>
#include <net/if.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

namespace
{
constexpr uint8_t kDefaultSourceAddress = 0x90;

std::runtime_error BuildError(const std::string& context)
{
    return std::runtime_error(context + ": " + std::strerror(errno));
}
} // namespace

int CanSetUp::CanSocketSetup()
{
    if (m_interface.empty())
        throw std::invalid_argument("CAN interface must not be empty");

    if (m_interface.size() >= IFNAMSIZ)
        throw std::invalid_argument("CAN interface name is too long");

    const int socketFd = socket(PF_CAN, SOCK_DGRAM, CAN_J1939);
    if (socketFd < 0)
        throw BuildError("Failed to create J1939 socket");

    struct ifreq ifr{};
    std::strncpy(ifr.ifr_name, m_interface.c_str(), IFNAMSIZ - 1);

    if (ioctl(socketFd, SIOCGIFINDEX, &ifr) < 0)
    {
        close(socketFd);
        throw BuildError("Failed to resolve CAN interface index for interface '" + m_interface + "'");
    }

    struct sockaddr_can addr{};
    addr.can_family          = AF_CAN;
    addr.can_ifindex         = ifr.ifr_ifindex;
    addr.can_addr.j1939.name = J1939_NO_NAME;
    addr.can_addr.j1939.pgn  = J1939_NO_PGN;
    // A concrete source address is required for successful J1939 TX.
    addr.can_addr.j1939.addr = kDefaultSourceAddress;

    const int promisc = 1;
    if (setsockopt(socketFd, SOL_CAN_J1939, SO_J1939_PROMISC, &promisc, sizeof(promisc)) < 0)
    {
        close(socketFd);
        throw BuildError("Failed to enable J1939 promiscuous mode");
    }

    if (bind(socketFd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        close(socketFd);
        throw BuildError("Failed to bind J1939 socket on interface '" + m_interface + "'");
    }

    return socketFd;
}

void CanSetUp::ApplyPgnFilter(int socketFd, uint32_t pgn) const
{
    if (socketFd < 0)
        throw std::invalid_argument("Invalid socket fd");

    if (pgn > J1939_PGN_MAX)
        throw std::invalid_argument("PGN is out of range");

    const j1939_filter filter{
        .name = J1939_NO_NAME,
        .name_mask = 0,
        .pgn = pgn,
        .pgn_mask = J1939_PGN_MAX,
        .addr = J1939_NO_ADDR,
        .addr_mask = 0,
    };

    if (setsockopt(socketFd, SOL_CAN_J1939, SO_J1939_FILTER, &filter, sizeof(filter)) < 0)
        throw BuildError("Failed to apply J1939 PGN filter");
}