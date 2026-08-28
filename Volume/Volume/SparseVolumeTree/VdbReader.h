#pragma once
// Reads a .vdb file written by SparseVolumeVdbWriter back into SparseVolume<float>.
// No OpenVDB library dependency; uses only the C++ standard library.
// Supported format: version 224, Tree_float_5_4_3, COMPRESS_NONE, little-endian.

#include "SparseVolume.h"
#include "../../../../CGLib/Util/UnCopyable.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace Phantom {
namespace Volume {

class SparseVolumeVdbReader : private Phantom::UnCopyable {
public:
    SparseVolumeVdbReader() = default;

    // Read the first float grid from filePath.
    // Returns nullptr if the file cannot be read or has an unsupported format.
    std::unique_ptr<SparseVolume<float>> read(const std::string& filePath);

private:
    struct LeafInfo {
        Coord    rootKey;
        int      offsetB;
        int      offsetA;
        uint64_t valueMask[8]; // 512 bits
    };

    std::vector<LeafInfo> leaves_;

    void readInternalBTopology(std::istream& is, const Coord& rootKey);
    void readInternalATopology(std::istream& is, const Coord& rootKey, int offsetB);

    static Coord voxelCoord(const Coord& rootKey, int offsetB, int offsetA, int offsetL);

    static std::vector<int> setBits(const uint64_t* words, int nWords);

    template<typename T>
    static T readPOD(std::istream& is) {
        T v{};
        is.read(reinterpret_cast<char*>(&v), sizeof(T));
        return v;
    }

    static std::string readString(std::istream& is) {
        const uint32_t len = readPOD<uint32_t>(is);
        std::string s(len, '\0');
        is.read(&s[0], static_cast<std::streamsize>(len));
        return s;
    }

    template<int N>
    static void readMask(std::istream& is, uint64_t* dst) {
        static_assert(N % 64 == 0, "mask size must be multiple of 64");
        is.read(reinterpret_cast<char*>(dst), (N / 64) * sizeof(uint64_t));
    }

    static void skipMeta(std::istream& is) {
        const uint32_t n = readPOD<uint32_t>(is);
        for (uint32_t i = 0; i < n; ++i) {
            readString(is); // key
            readString(is); // type
            readString(is); // value
        }
    }
};

// ──── Implementation ───────────────────────────────────────────────────────────

inline std::unique_ptr<SparseVolume<float>>
SparseVolumeVdbReader::read(const std::string& filePath)
{
    std::ifstream ifs(filePath, std::ios::binary);
    if (!ifs) return nullptr;

    // FileHeader
    if (readPOD<int64_t>(ifs) != int64_t{0x56444220}) return nullptr; // magic
    if (readPOD<uint32_t>(ifs) != 224u)                return nullptr; // version
    readPOD<uint32_t>(ifs); // libMajor
    readPOD<uint32_t>(ifs); // libMinor
    const uint8_t hasOffsets = readPOD<uint8_t>(ifs);
    char uuid[36]; ifs.read(uuid, 36);

    // FileMeta
    skipMeta(ifs);

    // gridCount
    if (readPOD<uint32_t>(ifs) == 0u) return nullptr;

    // GridDescriptor (only first grid)
    readString(ifs); // uniqueName
    const std::string gridType = readString(ifs);
    readString(ifs); // instanceParent
    const int64_t gridPos = readPOD<int64_t>(ifs);
    readPOD<int64_t>(ifs); // blockPos
    readPOD<int64_t>(ifs); // endPos

    if (gridType != "Tree_float_5_4_3") return nullptr;

    if (hasOffsets) ifs.seekg(static_cast<std::streamoff>(gridPos));

    readPOD<uint32_t>(ifs); // compressionFlags (COMPRESS_NONE=0)

    // GridMeta
    skipMeta(ifs);

    // Transform: mapType string + 5 Vec3d (15 doubles)
    if (readString(ifs) != "UniformScaleMap") return nullptr;
    double td[15]{};
    for (int i = 0; i < 15; ++i) td[i] = readPOD<double>(ifs);
    const float voxelSize = static_cast<float>(td[3]); // second Vec3d .x

    // Tree: bufferCount (int32)
    readPOD<int32_t>(ifs);

    // Topology pass
    leaves_.clear();
    const float bg           = readPOD<float>(ifs);
    readPOD<uint32_t>(ifs);  // tileCount (always 0 in our writer)
    const uint32_t rootN     = readPOD<uint32_t>(ifs);
    for (uint32_t i = 0; i < rootN; ++i) {
        Coord rk;
        rk.x = readPOD<int32_t>(ifs);
        rk.y = readPOD<int32_t>(ifs);
        rk.z = readPOD<int32_t>(ifs);
        readInternalBTopology(ifs, rk);
    }

    // Buffer pass
    auto volume = std::make_unique<SparseVolume<float>>(bg);
    volume->setVoxelSize(voxelSize);

    std::array<float, 512> leafBuf;
    uint64_t bufMask[8];
    for (const auto& leaf : leaves_) {
        readMask<512>(ifs, bufMask); // valueMask (repeated from topology)
        readPOD<int8_t>(ifs);        // metadata byte (COMPRESS_NONE = 0)
        ifs.read(reinterpret_cast<char*>(leafBuf.data()), 512 * sizeof(float));

        for (int i = 0; i < 512; ++i) {
            if (leaf.valueMask[i / 64] & (uint64_t(1) << (i % 64)))
                volume->setValue(voxelCoord(leaf.rootKey, leaf.offsetB, leaf.offsetA, i),
                                 leafBuf[i]);
        }
    }

    if (!ifs.good()) return nullptr;
    return volume;
}

inline void SparseVolumeVdbReader::readInternalBTopology(
    std::istream& is, const Coord& rootKey)
{
    uint64_t childMask[512], valueMask[512];
    readMask<32768>(is, childMask);
    readMask<32768>(is, valueMask);
    readPOD<int8_t>(is); // metadata byte
    is.seekg(32768 * sizeof(float), std::ios::cur);

    for (const int b : setBits(childMask, 512))
        readInternalATopology(is, rootKey, b);
}

inline void SparseVolumeVdbReader::readInternalATopology(
    std::istream& is, const Coord& rootKey, int offsetB)
{
    uint64_t childMask[64], valueMask[64];
    readMask<4096>(is, childMask);
    readMask<4096>(is, valueMask);
    readPOD<int8_t>(is); // metadata byte
    is.seekg(4096 * sizeof(float), std::ios::cur);

    for (const int a : setBits(childMask, 64)) {
        LeafInfo leaf;
        leaf.rootKey = rootKey;
        leaf.offsetB = offsetB;
        leaf.offsetA = a;
        readMask<512>(is, leaf.valueMask);
        leaves_.push_back(leaf);
    }
}

inline Coord SparseVolumeVdbReader::voxelCoord(
    const Coord& rk, int offsetB, int offsetA, int offsetL)
{
    const int bx = (offsetB >> 10) & 31, by = (offsetB >> 5) & 31, bz = offsetB & 31;
    const int ax = (offsetA >>  8) & 15, ay = (offsetA >> 4) & 15, az = offsetA & 15;
    const int lx = (offsetL >>  6) &  7, ly = (offsetL >> 3) &  7, lz = offsetL & 7;
    return Coord(
        rk.x + (bx << 7) + (ax << 3) + lx,
        rk.y + (by << 7) + (ay << 3) + ly,
        rk.z + (bz << 7) + (az << 3) + lz);
}

inline std::vector<int> SparseVolumeVdbReader::setBits(const uint64_t* words, int nWords)
{
    std::vector<int> out;
    for (int w = 0; w < nWords; ++w) {
        uint64_t word = words[w];
        const int base = w * 64;
        for (int b = 0; b < 64; ++b) {
            if (word & (uint64_t(1) << b))
                out.push_back(base + b);
        }
    }
    return out;
}

} // namespace Math
} // namespace Phantom
