#include "PMDFileReader.h"
#include <cstring>
#include <fstream>

namespace Phantom {
    namespace File {

bool PMDFileReader::read(const std::filesystem::path& path)
{
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    return readFrom(ifs);
}

bool PMDFileReader::read(std::istream& stream)
{
    return readFrom(stream);
}

bool PMDFileReader::readFrom(std::istream& s)
{
    pmd_ = PMDFile{};

    s.read(pmd_.header.magic, 3);
    if (!s || std::strncmp(pmd_.header.magic, "Pmd", 3) != 0) return false;
    s.read(reinterpret_cast<char*>(&pmd_.header.version), 4);
    s.read(pmd_.header.modelName, 20);
    s.read(pmd_.header.comment, 256);
    if (!s) return false;

    uint32_t vertCount = 0;
    s.read(reinterpret_cast<char*>(&vertCount), 4);
    pmd_.vertices.resize(vertCount);
    for (auto& v : pmd_.vertices)
        s.read(reinterpret_cast<char*>(&v), sizeof(PMDVertex));

    uint32_t idxCount = 0;
    s.read(reinterpret_cast<char*>(&idxCount), 4);
    pmd_.indices.resize(idxCount);
    for (auto& idx : pmd_.indices)
        s.read(reinterpret_cast<char*>(&idx), 2);

    uint32_t matCount = 0;
    s.read(reinterpret_cast<char*>(&matCount), 4);
    pmd_.materials.resize(matCount);
    for (auto& m : pmd_.materials)
        s.read(reinterpret_cast<char*>(&m), sizeof(PMDMaterial));

    uint16_t boneCount = 0;
    s.read(reinterpret_cast<char*>(&boneCount), 2);
    pmd_.bones.resize(boneCount);
    for (auto& b : pmd_.bones)
        s.read(reinterpret_cast<char*>(&b), sizeof(PMDBone));

    if (!s) return false;

    // IK list
    uint16_t ikCount = 0;
    s.read(reinterpret_cast<char*>(&ikCount), 2);
    pmd_.iks.resize(ikCount);
    for (auto& ik : pmd_.iks) {
        PMDIKHeader hdr{};
        s.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        ik.ikBoneIndex     = hdr.ikBoneIndex;
        ik.targetBoneIndex = hdr.targetBoneIndex;
        ik.iterationCount  = hdr.iterationCount;
        ik.controlWeight   = hdr.controlWeight;
        ik.linkBones.resize(hdr.linkCount);
        for (auto& lb : ik.linkBones)
            s.read(reinterpret_cast<char*>(&lb), 2);
    }

    if (!s) return false;

    // Morph (face) list
    uint16_t morphCount = 0;
    s.read(reinterpret_cast<char*>(&morphCount), 2);
    pmd_.morphs.resize(morphCount);
    for (auto& morph : pmd_.morphs) {
        PMDMorphHeader hdr{};
        s.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        std::memcpy(morph.name, hdr.name, 20);
        morph.category = hdr.category;
        morph.vertices.resize(hdr.vertexCount);
        for (auto& mv : morph.vertices) {
            PMDMorphVertexPacked packed{};
            s.read(reinterpret_cast<char*>(&packed), sizeof(packed));
            mv.vertexIndex   = packed.vertexIndex;
            mv.posOffset[0]  = packed.posOffset[0];
            mv.posOffset[1]  = packed.posOffset[1];
            mv.posOffset[2]  = packed.posOffset[2];
        }
    }

    return !s.bad();
}

    } // namespace File
} // namespace Phantom
