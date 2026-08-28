#include "gtest/gtest.h"
#include "../File/PMXFileReader.h"

#include <cstring>
#include <sstream>
#include <vector>

using namespace Phantom::File;

// ---------------------------------------------------------------------------
// Binary builder helpers
// ---------------------------------------------------------------------------

namespace {

class PMXBinaryBuilder {
public:
    // Append raw bytes
    void append(const void* data, size_t n) {
        const auto* p = static_cast<const uint8_t*>(data);
        buf_.insert(buf_.end(), p, p + n);
    }
    template<typename T> void push(T v) { append(&v, sizeof(T)); }

    void pushText(const std::string& s) {
        push(static_cast<int32_t>(s.size()));
        append(s.data(), s.size());
    }

    // Write PMX header + globals + 4 empty model-info strings.
    // vertexIndexSize / boneIndexSize default to 1.
    void writeHeader(float version = 2.0f, uint8_t encoding = 1 /*UTF-8*/,
                     uint8_t additionalUVs = 0,
                     uint8_t vertexIndexSize = 1,
                     uint8_t boneIndexSize = 1)
    {
        append("PMX ", 4);
        push(version);
        push(uint8_t(8));           // globals size
        push(encoding);
        push(additionalUVs);
        push(vertexIndexSize);      // vertex index size
        push(uint8_t(1));           // texture index size
        push(uint8_t(1));           // material index size
        push(boneIndexSize);
        push(uint8_t(1));           // morph index size
        push(uint8_t(1));           // rigid body index size
        // 4 empty model-info strings
        for (int i = 0; i < 4; ++i) push(int32_t(0));
    }

    // Write zero-count sections for vertices/indices/textures/materials/bones/morphs
    void writeEmptySections() {
        for (int i = 0; i < 6; ++i) push(int32_t(0));
    }

    std::string build() const {
        return std::string(buf_.begin(), buf_.end());
    }

private:
    std::vector<uint8_t> buf_;
};

// Build a minimal valid PMX with no content
std::string buildMinimalPMX()
{
    PMXBinaryBuilder b;
    b.writeHeader();
    b.writeEmptySections();
    return b.build();
}

// Build a PMX with one BDEF1 vertex
std::string buildPMXOneBDEF1Vertex()
{
    PMXBinaryBuilder b;
    b.writeHeader(2.0f, 1, 0, /*vertexIndexSize=*/1, /*boneIndexSize=*/1);
    // vertices: count=1
    b.push(int32_t(1));
    b.push(1.0f); b.push(2.0f); b.push(3.0f);  // position
    b.push(0.0f); b.push(1.0f); b.push(0.0f);  // normal
    b.push(0.5f); b.push(0.5f);                 // uv
    b.push(uint8_t(0));                          // BDEF1
    b.push(int8_t(2));                           // bone index 2
    b.push(0.0f);                                // edgeFactor
    // remaining empty sections
    for (int i = 0; i < 5; ++i) b.push(int32_t(0));
    return b.build();
}

// Build a PMX with one BDEF4 vertex
std::string buildPMXOneBDEF4Vertex()
{
    PMXBinaryBuilder b;
    b.writeHeader(2.0f, 1, 0, 1, 1);
    b.push(int32_t(1));
    b.push(0.0f); b.push(0.0f); b.push(0.0f);  // position
    b.push(0.0f); b.push(1.0f); b.push(0.0f);  // normal
    b.push(0.0f); b.push(0.0f);                 // uv
    b.push(uint8_t(2));                          // BDEF4
    b.push(int8_t(0)); b.push(int8_t(1));
    b.push(int8_t(2)); b.push(int8_t(3));       // bone indices
    b.push(0.4f); b.push(0.3f);
    b.push(0.2f); b.push(0.1f);                 // weights (sum = 1.0)
    b.push(0.0f);                                // edgeFactor
    for (int i = 0; i < 5; ++i) b.push(int32_t(0));
    return b.build();
}

// Build a PMX with two bones (second has parent == first)
std::string buildPMXTwoBones()
{
    PMXBinaryBuilder b;
    b.writeHeader(2.0f, 1, 0, 1, 1);
    // no vertices/indices/textures/materials
    for (int i = 0; i < 4; ++i) b.push(int32_t(0));
    // bones: count=2
    b.push(int32_t(2));
    // bone 0: "Root"
    b.pushText("Root"); b.pushText("Root");
    b.push(0.0f); b.push(0.0f); b.push(0.0f); // position
    b.push(int8_t(-1));                         // parent: none
    b.push(int32_t(0));                         // transform layer
    b.push(uint16_t(0x0002 | 0x0008));          // flags: rotatable | visible, tail=position
    b.push(0.0f); b.push(0.1f); b.push(0.0f);  // tailPosition (flag 0x0001 not set)
    // bone 1: "Child"
    b.pushText("Child"); b.pushText("Child");
    b.push(0.0f); b.push(1.0f); b.push(0.0f); // position
    b.push(int8_t(0));                          // parent: bone 0
    b.push(int32_t(0));
    b.push(uint16_t(0x0002 | 0x0008));
    b.push(0.0f); b.push(0.1f); b.push(0.0f);  // tailPosition
    // no morphs
    b.push(int32_t(0));
    return b.build();
}

// Build a PMX with one vertex morph
std::string buildPMXOneMorph()
{
    PMXBinaryBuilder b;
    b.writeHeader();
    // no vertices/indices/textures/materials/bones
    for (int i = 0; i < 5; ++i) b.push(int32_t(0));
    // morphs: count=1, type=vertex, category=2 (eye)
    b.push(int32_t(1));
    b.pushText("eye_open"); b.pushText("eye_open_en");
    b.push(uint8_t(2));   // category: eye
    b.push(uint8_t(1));   // type: vertex
    b.push(int32_t(2));   // 2 elements
    // element 0: vertex 0, offset (1,2,3)
    b.push(uint8_t(0));   // vertex index (1-byte unsigned)
    b.push(1.0f); b.push(2.0f); b.push(3.0f);
    // element 1: vertex 1, offset (0,0,-1)
    b.push(uint8_t(1));
    b.push(0.0f); b.push(0.0f); b.push(-1.0f);
    return b.build();
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST(PMXFileReaderTest, EmptyModelReads)
{
    const auto data = buildMinimalPMX();
    std::istringstream ss(data);
    PMXFileReader r;
    EXPECT_TRUE(r.read(ss));
    EXPECT_FLOAT_EQ(2.0f, r.getPMX().version);
    EXPECT_EQ(0u, r.getPMX().vertices.size());
    EXPECT_EQ(0u, r.getPMX().bones.size());
    EXPECT_EQ(0u, r.getPMX().morphs.size());
}

TEST(PMXFileReaderTest, InvalidMagicFails)
{
    const std::string bad = "PMD \x00\x00\x00\x00";
    std::istringstream ss(bad);
    PMXFileReader r;
    EXPECT_FALSE(r.read(ss));
}

TEST(PMXFileReaderTest, EmptyStreamFails)
{
    std::istringstream ss("");
    PMXFileReader r;
    EXPECT_FALSE(r.read(ss));
}

TEST(PMXFileReaderTest, GlobalsParsed)
{
    const auto data = buildMinimalPMX();
    std::istringstream ss(data);
    PMXFileReader r;
    ASSERT_TRUE(r.read(ss));
    EXPECT_EQ(1, r.getPMX().globals.encoding);         // UTF-8
    EXPECT_EQ(0, r.getPMX().globals.additionalUVs);
    EXPECT_EQ(1, r.getPMX().globals.vertexIndexSize);
    EXPECT_EQ(1, r.getPMX().globals.boneIndexSize);
}

TEST(PMXFileReaderTest, BDEF1VertexRead)
{
    const auto data = buildPMXOneBDEF1Vertex();
    std::istringstream ss(data);
    PMXFileReader r;
    ASSERT_TRUE(r.read(ss));
    ASSERT_EQ(1u, r.getPMX().vertices.size());
    const auto& v = r.getPMX().vertices[0];
    EXPECT_FLOAT_EQ(1.0f, v.position[0]);
    EXPECT_FLOAT_EQ(2.0f, v.position[1]);
    EXPECT_FLOAT_EQ(3.0f, v.position[2]);
    EXPECT_EQ(0, v.weightType);   // BDEF1
    EXPECT_EQ(2, v.boneIndices[0]);
    EXPECT_FLOAT_EQ(1.0f, v.boneWeights[0]);
    EXPECT_FLOAT_EQ(0.0f, v.boneWeights[1]);
}

TEST(PMXFileReaderTest, BDEF4VertexRead)
{
    const auto data = buildPMXOneBDEF4Vertex();
    std::istringstream ss(data);
    PMXFileReader r;
    ASSERT_TRUE(r.read(ss));
    ASSERT_EQ(1u, r.getPMX().vertices.size());
    const auto& v = r.getPMX().vertices[0];
    EXPECT_EQ(2, v.weightType); // BDEF4
    EXPECT_EQ(0, v.boneIndices[0]);
    EXPECT_EQ(1, v.boneIndices[1]);
    EXPECT_EQ(2, v.boneIndices[2]);
    EXPECT_EQ(3, v.boneIndices[3]);
    EXPECT_FLOAT_EQ(0.4f, v.boneWeights[0]);
    EXPECT_FLOAT_EQ(0.3f, v.boneWeights[1]);
    EXPECT_FLOAT_EQ(0.2f, v.boneWeights[2]);
    EXPECT_FLOAT_EQ(0.1f, v.boneWeights[3]);
}

TEST(PMXFileReaderTest, BonesRead)
{
    const auto data = buildPMXTwoBones();
    std::istringstream ss(data);
    PMXFileReader r;
    ASSERT_TRUE(r.read(ss));
    ASSERT_EQ(2u, r.getPMX().bones.size());
    EXPECT_EQ("Root",  r.getPMX().bones[0].name);
    EXPECT_EQ("Child", r.getPMX().bones[1].name);
    EXPECT_EQ(-1, r.getPMX().bones[0].parentBoneIndex);
    EXPECT_EQ(0,  r.getPMX().bones[1].parentBoneIndex);
    EXPECT_FLOAT_EQ(0.0f, r.getPMX().bones[0].position[1]);
    EXPECT_FLOAT_EQ(1.0f, r.getPMX().bones[1].position[1]);
}

TEST(PMXFileReaderTest, VertexMorphRead)
{
    const auto data = buildPMXOneMorph();
    std::istringstream ss(data);
    PMXFileReader r;
    ASSERT_TRUE(r.read(ss));
    ASSERT_EQ(1u, r.getPMX().morphs.size());
    const auto& m = r.getPMX().morphs[0];
    EXPECT_EQ("eye_open", m.name);
    EXPECT_EQ(2, m.category);
    EXPECT_EQ(1, m.morphType);
    ASSERT_EQ(2u, m.vertexMorphs.size());
    EXPECT_EQ(0, m.vertexMorphs[0].vertexIndex);
    EXPECT_FLOAT_EQ(1.0f, m.vertexMorphs[0].offset[0]);
    EXPECT_FLOAT_EQ(2.0f, m.vertexMorphs[0].offset[1]);
    EXPECT_FLOAT_EQ(3.0f, m.vertexMorphs[0].offset[2]);
    EXPECT_EQ(1, m.vertexMorphs[1].vertexIndex);
    EXPECT_FLOAT_EQ(-1.0f, m.vertexMorphs[1].offset[2]);
}
