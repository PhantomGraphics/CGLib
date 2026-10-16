#pragma once
#include <filesystem>
#include <istream>
#include <string>
#include "PMXFile.h"

namespace Phantom {
    namespace File {

struct PMXParseStats {
    bool        success    = false;
    std::string failedAt;       // section name when parse failed, e.g. "bones[42]"
    int     vertCount  = -1;
    int     idxCount   = -1;
    int     texCount   = -1;
    int     matCount   = -1;
    int     boneCount  = -1;
    int     morphCount = -1;
    int64_t boneSectionStart  = -1; // stream position just after boneCount was read
    int64_t lastBoneStreamPos = -1; // stream position just before the failing bone's name
};

class PMXFileReader {
public:
    bool read(const std::filesystem::path& path);
    bool read(std::istream& stream);
    const PMXFile&       getPMX()        const { return pmx_;   }
    const PMXParseStats& getParseStats() const { return stats_; }

private:
    bool readFrom(std::istream& s);

    // Read text: int32 byte-length + raw bytes.
    // utf16 == true → UTF-16LE input, converted to UTF-8 (portable, no OS API).
    static std::string readText(std::istream& s, bool utf16);

    // Read a signed index (bone/texture/material/morph) of the given byte width.
    // Returns -1 for the maximum unsigned value (PMX "no entry" convention).
    static int32_t readSignedIndex(std::istream& s, int indexSize);

    // Read an unsigned vertex index (always >= 0).
    static int32_t readUnsignedIndex(std::istream& s, int indexSize);

    PMXFile       pmx_;
    PMXParseStats stats_;
};

    } // namespace File
} // namespace Phantom
