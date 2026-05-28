#ifndef PROCESS_DATA_HPP
#define PROCESS_DATA_HPP

#include "j1939-db.hpp"

#include <cstdint>
#include <vector>

class ProcessData
{
public:
    std::vector<uint8_t> BuildRandomPayload(const PgnData& pgnData) const;
};

#endif // PROCESS_DATA_HPP
