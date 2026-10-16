#include "AssetId.h"

#include <cstdint>
#include <iomanip>
#include <random>
#include <sstream>

namespace Phantom::Asset {

AssetId AssetId::generate()
{
    static thread_local std::mt19937_64 rng{ std::random_device{}() };
    std::uniform_int_distribution<uint32_t> dist32;
    std::uniform_int_distribution<uint16_t> dist16;
    std::uniform_int_distribution<uint64_t> dist48(0, (1ULL << 48) - 1);

    const uint32_t timeLow = dist32(rng);
    const uint16_t timeMid = dist16(rng);
    const uint16_t timeHiAndVersion = static_cast<uint16_t>((dist16(rng) & 0x0FFFu) | 0x4000u); // version 4
    const uint16_t clockSeq = static_cast<uint16_t>((dist16(rng) & 0x3FFFu) | 0x8000u); // variant 10xx
    const uint64_t node = dist48(rng);

    std::ostringstream out;
    out << std::hex << std::setfill('0')
        << std::setw(8) << timeLow << '-'
        << std::setw(4) << timeMid << '-'
        << std::setw(4) << timeHiAndVersion << '-'
        << std::setw(4) << clockSeq << '-'
        << std::setw(12) << node;
    return AssetId(out.str());
}

} // namespace Phantom::Asset
