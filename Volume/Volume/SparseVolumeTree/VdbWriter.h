#pragma once
// Writes SparseVolume<float> to an OpenVDB-compatible .vdb binary file.
// No OpenVDB library dependency; uses only the C++ standard library.
// Output format: Tree4<float,5,4,3>, COMPRESS_NONE, little-endian.
//
// File layout (version 224):
//   [FileHeader][FileMeta][gridCount=1][GridDescriptor]
//   [per-grid compression flags (uint32)][GridMeta][Transform]
//   [Tree topology: RootNode -> InternalB -> InternalA -> Leaf.valueMask]
//   [Tree buffers:  Leaf.valueMask + int8 + float[512]  per leaf, DFS order]

#include "SparseVolume.h"
#include "../../../../CGLib/Util/UnCopyable.h"

#include <fstream>
#include <vector>
#include <algorithm>
#include <unordered_map>
#include <array>
#include <bitset>
#include <cstdint>
#include <random>
#include <ctime>
#include <string>

namespace Phantom {
namespace Volume {

class SparseVolumeVdbWriter : private Phantom::UnCopyable {
public:
    SparseVolumeVdbWriter() = default;

    // Write volume to filePath. gridName becomes the OpenVDB grid name.
    // Returns false on I/O error.
    bool write(const std::string& filePath,
               const SparseVolume<float>& volume,
               const std::string& gridName = "density");

private:
    // ── File-level sections ──────────────────────────────────────────────────
    void writeFileHeader(std::ostream& os);
    void writeFileMeta(std::ostream& os);
    void writeGridDescriptor(std::ostream& os,
                             const std::string& gridName,
                             int64_t gridPos, int64_t blockPos, int64_t endPos);

    // ── Grid-level sections ──────────────────────────────────────────────────
    void writeGridMeta(std::ostream& os, const std::string& gridName);
    void writeTransform(std::ostream& os, float voxelSize);
    void writeTree(std::ostream& os, const SparseVolume<float>& volume);

    // ── In-memory mirror of Tree4<float,5,4,3> ──────────────────────────────
    // Populated by buildWriteTree() before serialization.

    struct WriteLeaf {                                     // Log2Dim=3, 8^3=512
        std::bitset<512>       valueMask;
        std::array<float, 512> buffer{};
    };
    struct WriteInternalA {                                // Log2Dim=4, 16^3=4096
        std::bitset<4096>       childMask;
        std::bitset<4096>       valueMask;
        std::array<float, 4096> tiles{};
        std::unordered_map<int, WriteLeaf> children;       // key = slot offset
    };
    struct WriteInternalB {                                // Log2Dim=5, 32^3=32768
        std::bitset<32768>       childMask;
        std::bitset<32768>       valueMask;
        std::array<float, 32768> tiles{};
        std::unordered_map<int, WriteInternalA> children;  // key = slot offset
    };
    struct WriteRoot {
        float background = 0.0f;
        std::unordered_map<Coord, WriteInternalB, Coord::Hash> children;
    };

    WriteRoot buildWriteTree(const SparseVolume<float>& volume) const;

    // ── Topology pass: writes tree structure + InternalNode tile values ───────
    void writeRootTopology   (std::ostream& os, const WriteRoot&      root) const;
    void writeInternalBTopology(std::ostream& os, const WriteInternalB& node) const;
    void writeInternalATopology(std::ostream& os, const WriteInternalA& node) const;
    void writeLeafTopology   (std::ostream& os, const WriteLeaf&      leaf) const;

    // ── Buffer pass: writes leaf voxel values (same DFS order as topology) ───
    void writeRootBuffers    (std::ostream& os, const WriteRoot&      root) const;
    void writeInternalBBuffers(std::ostream& os, const WriteInternalB& node) const;
    void writeInternalABuffers(std::ostream& os, const WriteInternalA& node) const;
    void writeLeafBuffer     (std::ostream& os, const WriteLeaf&      leaf) const;

    // ── Utilities ────────────────────────────────────────────────────────────

    static void writeString(std::ostream& os, const std::string& s);

    template<size_t N>
    static void writeNodeMask(std::ostream& os, const std::bitset<N>& mask) {
        static_assert(N % 64 == 0, "NodeMask size must be a multiple of 64");
        for (int i = 0; i < static_cast<int>(N / 64); ++i) {
            uint64_t word = 0;
            for (int b = 0; b < 64; ++b) {
                if (mask.test(static_cast<size_t>(i * 64 + b)))
                    word |= (uint64_t(1) << b);
            }
            writePOD<uint64_t>(os, word);
        }
    }

    template<typename T>
    static void writePOD(std::ostream& os, const T& v) {
        os.write(reinterpret_cast<const char*>(&v), sizeof(T));
    }

    template<typename MapT>
    static std::vector<std::pair<typename MapT::key_type, const typename MapT::mapped_type*>>
    sortedChildren(const MapT& map) {
        using K = typename MapT::key_type;
        using V = typename MapT::mapped_type;
        std::vector<std::pair<K, const V*>> sorted;
        sorted.reserve(map.size());
        for (const auto& kv : map)
            sorted.push_back({kv.first, &kv.second});
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        return sorted;
    }

    static std::string generateUUID();
};

// ──── Inline implementation ──────────────────────────────────────────────────

inline bool SparseVolumeVdbWriter::write(
    const std::string& filePath,
    const SparseVolume<float>& volume,
    const std::string& gridName)
{
    std::ofstream ofs(filePath, std::ios::binary);
    if (!ofs) return false;

    writeFileHeader(ofs);
    writeFileMeta(ofs);
    writePOD<uint32_t>(ofs, 1u); // gridCount

    const std::streampos posDesc = ofs.tellp();
    writeGridDescriptor(ofs, gridName, 0, 0, 0);

    const std::streampos posGrid = ofs.tellp();
    writePOD<uint32_t>(ofs, 0u); // COMPRESS_NONE
    writeGridMeta(ofs, gridName);
    writeTransform(ofs, volume.getVoxelSize());

    const std::streampos posBlock = ofs.tellp();
    writeTree(ofs, volume);
    const std::streampos posEnd = ofs.tellp();

    ofs.seekp(posDesc);
    writeGridDescriptor(ofs, gridName,
        static_cast<int64_t>(posGrid),
        static_cast<int64_t>(posBlock),
        static_cast<int64_t>(posEnd));

    ofs.seekp(posEnd);
    return ofs.good();
}

inline void SparseVolumeVdbWriter::writeFileHeader(std::ostream& os)
{
    writePOD<int64_t> (os, int64_t{0x56444220});
    writePOD<uint32_t>(os, 224u);
    writePOD<uint32_t>(os, 11u);
    writePOD<uint32_t>(os, 0u);
    writePOD<uint8_t> (os, 1u);

    const std::string uuid = generateUUID();
    os.write(uuid.c_str(), 36);
}

inline void SparseVolumeVdbWriter::writeFileMeta(std::ostream& os)
{
    writePOD<uint32_t>(os, 1u);
    writeString(os, "creator");
    writeString(os, "string");
    writeString(os, "Phantom::SparseVolumeVdbWriter");
}

inline void SparseVolumeVdbWriter::writeGridDescriptor(
    std::ostream& os,
    const std::string& gridName,
    int64_t gridPos, int64_t blockPos, int64_t endPos)
{
    writeString(os, gridName);
    writeString(os, "Tree_float_5_4_3");
    writeString(os, "");
    writePOD<int64_t>(os, gridPos);
    writePOD<int64_t>(os, blockPos);
    writePOD<int64_t>(os, endPos);
}

inline void SparseVolumeVdbWriter::writeGridMeta(std::ostream& os, const std::string& gridName)
{
    writePOD<uint32_t>(os, 1u);
    writeString(os, "name");
    writeString(os, "string");
    writeString(os, gridName);
}

inline void SparseVolumeVdbWriter::writeTransform(std::ostream& os, float voxelSize)
{
    writeString(os, "UniformScaleMap");

    const double vs       = static_cast<double>(voxelSize);
    const double inv      = 1.0 / vs;
    const double invSqr   = inv * inv;
    const double invTwice = 1.0 / (2.0 * vs);

    auto writeVec3d = [&](double v) {
        writePOD<double>(os, v);
        writePOD<double>(os, v);
        writePOD<double>(os, v);
    };
    writeVec3d(vs);
    writeVec3d(vs);
    writeVec3d(inv);
    writeVec3d(invSqr);
    writeVec3d(invTwice);
}

inline SparseVolumeVdbWriter::WriteRoot
SparseVolumeVdbWriter::buildWriteTree(const SparseVolume<float>& volume) const
{
    WriteRoot root;
    root.background = volume.getBackground();

    volume.forEachActive([&](const Coord& index, const Math::Vector3df& /*world*/, float value) {
        const int ix = index.x, iy = index.y, iz = index.z;

        const Coord rootKey(ix & ~4095, iy & ~4095, iz & ~4095);

        const int offsetB = ((ix >> 7) & 31) * 1024
                          + ((iy >> 7) & 31) * 32
                          + ((iz >> 7) & 31);

        const int offsetA = ((ix >> 3) & 15) * 256
                          + ((iy >> 3) & 15) * 16
                          + ((iz >> 3) & 15);

        const int offsetL = (ix & 7) * 64
                          + (iy & 7) * 8
                          + (iz & 7);

        WriteInternalB& intB = root.children[rootKey];
        intB.childMask.set(static_cast<size_t>(offsetB));

        WriteInternalA& intA = intB.children[offsetB];
        intA.childMask.set(static_cast<size_t>(offsetA));

        WriteLeaf& leaf = intA.children[offsetA];
        leaf.valueMask.set(static_cast<size_t>(offsetL));
        leaf.buffer[static_cast<size_t>(offsetL)] = value;
    });

    return root;
}

inline void SparseVolumeVdbWriter::writeTree(std::ostream& os, const SparseVolume<float>& volume)
{
    const WriteRoot root = buildWriteTree(volume);
    writePOD<int32_t>(os, 1);
    writeRootTopology(os, root);
    writeRootBuffers(os, root);
}

inline void SparseVolumeVdbWriter::writeRootTopology(
    std::ostream& os, const WriteRoot& root) const
{
    writePOD<float>   (os, root.background);
    writePOD<uint32_t>(os, 0u);
    writePOD<uint32_t>(os, static_cast<uint32_t>(root.children.size()));

    for (const auto& entry : sortedChildren(root.children)) {
        writePOD<int32_t>(os, entry.first.x);
        writePOD<int32_t>(os, entry.first.y);
        writePOD<int32_t>(os, entry.first.z);
        writeInternalBTopology(os, *entry.second);
    }
}

inline void SparseVolumeVdbWriter::writeInternalBTopology(
    std::ostream& os, const WriteInternalB& node) const
{
    writeNodeMask(os, node.childMask);
    writeNodeMask(os, node.valueMask);
    writePOD<int8_t>(os, 0);
    os.write(reinterpret_cast<const char*>(node.tiles.data()),
             static_cast<std::streamsize>(32768 * sizeof(float)));

    for (const auto& entry : sortedChildren(node.children))
        writeInternalATopology(os, *entry.second);
}

inline void SparseVolumeVdbWriter::writeInternalATopology(
    std::ostream& os, const WriteInternalA& node) const
{
    writeNodeMask(os, node.childMask);
    writeNodeMask(os, node.valueMask);
    writePOD<int8_t>(os, 0);
    os.write(reinterpret_cast<const char*>(node.tiles.data()),
             static_cast<std::streamsize>(4096 * sizeof(float)));

    for (const auto& entry : sortedChildren(node.children))
        writeLeafTopology(os, *entry.second);
}

inline void SparseVolumeVdbWriter::writeLeafTopology(
    std::ostream& os, const WriteLeaf& leaf) const
{
    writeNodeMask(os, leaf.valueMask);
}

inline void SparseVolumeVdbWriter::writeRootBuffers(
    std::ostream& os, const WriteRoot& root) const
{
    for (const auto& entry : sortedChildren(root.children))
        writeInternalBBuffers(os, *entry.second);
}

inline void SparseVolumeVdbWriter::writeInternalBBuffers(
    std::ostream& os, const WriteInternalB& node) const
{
    for (const auto& entry : sortedChildren(node.children))
        writeInternalABuffers(os, *entry.second);
}

inline void SparseVolumeVdbWriter::writeInternalABuffers(
    std::ostream& os, const WriteInternalA& node) const
{
    for (const auto& entry : sortedChildren(node.children))
        writeLeafBuffer(os, *entry.second);
}

inline void SparseVolumeVdbWriter::writeLeafBuffer(
    std::ostream& os, const WriteLeaf& leaf) const
{
    writeNodeMask(os, leaf.valueMask);
    writePOD<int8_t>(os, 0);
    os.write(reinterpret_cast<const char*>(leaf.buffer.data()),
             static_cast<std::streamsize>(512 * sizeof(float)));
}

inline void SparseVolumeVdbWriter::writeString(std::ostream& os, const std::string& s)
{
    const uint32_t len = static_cast<uint32_t>(s.size());
    os.write(reinterpret_cast<const char*>(&len), sizeof(uint32_t));
    os.write(s.c_str(), static_cast<std::streamsize>(len));
}

inline std::string SparseVolumeVdbWriter::generateUUID()
{
    std::mt19937 rng(static_cast<uint32_t>(std::time(nullptr)));
    std::uniform_int_distribution<int> dist(0, 15);
    const char* hex = "0123456789abcdef";

    std::string uuid(36, '0');
    for (int i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23)
            uuid[i] = '-';
        else
            uuid[i] = hex[dist(rng)];
    }
    uuid[14] = '4';
    uuid[19] = hex[(dist(rng) & 0x3) | 0x8];
    return uuid;
}

} // namespace Math
} // namespace Phantom
