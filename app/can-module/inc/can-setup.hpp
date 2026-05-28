#ifndef CAN_SETUP_HPP
#define CAN_SETUP_HPP

#include <cstdint>
#include <string>

class CanSetUp
{
public:
    CanSetUp() = default;

    explicit CanSetUp(const std::string& interface)
        : m_interface(interface)
    {
    }

    ~CanSetUp() = default;
    int CanSocketSetup();
    void ApplyPgnFilter(int socketFd, uint32_t pgn) const;

private:
    std::string m_interface;
};

#endif // CAN_SETUP_HPP