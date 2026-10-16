#include "VMDFileReader.h"
#include <cstring>
#include <fstream>

namespace Phantom {
    namespace File {

bool VMDFileReader::read(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    return readFrom(ifs);
}

bool VMDFileReader::read(std::istream& stream)
{
    return readFrom(stream);
}

bool VMDFileReader::readFrom(std::istream& s)
{
    vmd_ = VMDFile{};

    s.read(reinterpret_cast<char*>(&vmd_.header), sizeof(VMDHeader));
    if (!s || std::strncmp(vmd_.header.magic, "Vocaloid Motion Data 0002", 25) != 0)
        return false;

    uint32_t boneCount = 0;
    s.read(reinterpret_cast<char*>(&boneCount), 4);
    if (!s) return false;
    vmd_.boneKeyframes.resize(boneCount);
    for (auto& kf : vmd_.boneKeyframes)
        s.read(reinterpret_cast<char*>(&kf), sizeof(VMDBoneKeyframe));

    if (s.good()) {
        uint32_t morphCount = 0;
        s.read(reinterpret_cast<char*>(&morphCount), 4);
        if (s.good()) {
            vmd_.morphKeyframes.resize(morphCount);
            for (auto& kf : vmd_.morphKeyframes)
                s.read(reinterpret_cast<char*>(&kf), sizeof(VMDMorphKeyframe));
        }
    }

    return !s.bad();
}

    } // namespace File
} // namespace Phantom
