#ifndef J1939_DB_HPP
#define J1939_DB_HPP

#include <cstdint>
#include <map>

// Signal byte ordering inside CAN payload
enum class Endianness
{
    LittleEndian, // Little-endian
    BigEndian     // Big-endian
};

// Signal value type
enum class Signedness
{
    Unsigned,
    Signed
};

// SPN signal metadata
struct SignalConfig
{
    // Start bit position inside payload
    uint16_t start_bit;

    // Number of bits used by signal
    uint16_t bit_size;

    // Physical scaling factor
    double scale;

    // Physical offset
    double offset;

    // Minimum physical value
    double min;

    // Maximum physical value
    double max;

    // Signal endianness
    Endianness endianness;

    // Signed or unsigned signal
    Signedness signedness;
};

// PGN metadata
struct PgnData
{
    // J1939 message priority
    uint8_t priority;

    // Payload size in bytes
    uint8_t dlc;

    // Default source address
    uint8_t source_address;

    // Optional periodic transmit time in milliseconds
    uint32_t cycle_time_ms{0};

    // key = SPN, value = signal metadata
    std::map<uint32_t, SignalConfig> signals;
};

// key = PGN, value = PGN metadata and SPNs
using J1939Database = std::map<uint32_t, PgnData>;

#endif // J1939_DB_HPP
