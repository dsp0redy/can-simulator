#include "process-data.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <random>

namespace
{
void SetBitsLittleEndian(std::vector<uint8_t>& payload, uint16_t startBit, uint16_t bitSize, uint64_t value)
{
    for (uint16_t i = 0; i < bitSize; ++i)
    {
        const uint32_t bitIndex = static_cast<uint32_t>(startBit) + i;
        const size_t byteIndex = bitIndex / 8;
        const uint8_t bitOffset = static_cast<uint8_t>(bitIndex % 8);

        if (byteIndex >= payload.size())
            return;

        const bool bitSet = ((value >> i) & 0x1ULL) != 0;
        if (bitSet)
            payload[byteIndex] |= static_cast<uint8_t>(1U << bitOffset);
        else
            payload[byteIndex] &= static_cast<uint8_t>(~(1U << bitOffset));
    }
}

uint64_t BuildRandomRawValue(const SignalConfig& signal, std::mt19937_64& rng)
{
    if (signal.bit_size == 0)
        return 0;

    const uint8_t width = static_cast<uint8_t>(std::min<uint16_t>(signal.bit_size, 64));
    const uint64_t mask = (width == 64) ? std::numeric_limits<uint64_t>::max() : ((1ULL << width) - 1ULL);

    if (signal.signedness == Signedness::Signed)
    {
        const int64_t minValue = (width == 64) ? std::numeric_limits<int64_t>::min() : -(1LL << (width - 1));
        const int64_t maxValue = (width == 64) ? std::numeric_limits<int64_t>::max() : ((1LL << (width - 1)) - 1);
        std::uniform_int_distribution<int64_t> dist(minValue, maxValue);
        return static_cast<uint64_t>(dist(rng)) & mask;
    }

    std::uniform_int_distribution<uint64_t> dist(0, mask);
    return dist(rng);
}
} // namespace

std::vector<uint8_t> ProcessData::BuildRandomPayload(const PgnData& pgnData) const
{
    std::vector<uint8_t> payload(pgnData.dlc, 0x00);
    std::random_device rd;
    std::mt19937_64 rng(rd());

    for (const auto& [spn, signal] : pgnData.signals)
    {
        (void)spn;
        const uint64_t rawValue = BuildRandomRawValue(signal, rng);

        // Current packing strategy uses little-endian bit indexing.
        SetBitsLittleEndian(payload, signal.start_bit, signal.bit_size, rawValue);
    }

    return payload;
}
