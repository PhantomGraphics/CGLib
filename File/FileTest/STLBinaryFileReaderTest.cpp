#include "gtest/gtest.h"

#include <cstdint>
#include <sstream>
#include "../File/STLFileReader.h"
#include "../File/STLFileWriter.h"

using namespace Phantom::Math;
using namespace Phantom::File;

namespace {
    void writeBinaryFace(std::ostream& out, const STLFace& face)
    {
        auto wf = [&](float f) { out.write(reinterpret_cast<const char*>(&f), sizeof(float)); };
        wf(face.normal.x); wf(face.normal.y); wf(face.normal.z);
        for (const auto& v : face.triangle.getVertices()) {
            wf(v.x); wf(v.y); wf(v.z);
        }
        char pad[2] = { 0, 0 };
        out.write(pad, 2);
    }

    std::stringstream buildBinarySTL(const std::vector<STLFace>& faces)
    {
        std::stringstream ss;
        // 80-byte header: 79 spaces + null terminator
        std::string hdr(79, ' ');
        ss.write(hdr.c_str(), 80);
        uint32_t count = static_cast<uint32_t>(faces.size());
        ss.write(reinterpret_cast<const char*>(&count), sizeof(uint32_t));
        for (const auto& f : faces) {
            writeBinaryFace(ss, f);
        }
        return ss;
    }
}

TEST(STLBinaryFileReaderTest, TestReadBinaryWithTwoFaces)
{
    const Triangle3df t1({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
    const Triangle3df t2({ Vector3df(1,0,0), Vector3df(1,1,0), Vector3df(0,1,0) });
    std::vector<STLFace> faces = {
        STLFace(t1, Vector3df(0, 0, 1)),
        STLFace(t2, Vector3df(0, 0, 1))
    };

    auto ss = buildBinarySTL(faces);

    STLFileReader reader;
    EXPECT_TRUE(reader.readBinary(ss));

    const auto& result = reader.getSTL();
    ASSERT_EQ(2u, result.faces.size());
}

TEST(STLBinaryFileReaderTest, TestReadBinaryNormalAndVertices)
{
    const Triangle3df tri({ Vector3df(1,2,3), Vector3df(4,5,6), Vector3df(7,8,9) });
    std::vector<STLFace> faces = { STLFace(tri, Vector3df(0, 0, 1)) };

    auto ss = buildBinarySTL(faces);

    STLFileReader reader;
    EXPECT_TRUE(reader.readBinary(ss));

    const auto& result = reader.getSTL();
    ASSERT_EQ(1u, result.faces.size());

    const auto& f = result.faces[0];
    EXPECT_EQ(Vector3df(0, 0, 1), f.normal);
    EXPECT_EQ(Vector3df(1, 2, 3), f.triangle.getVertices()[0]);
    EXPECT_EQ(Vector3df(4, 5, 6), f.triangle.getVertices()[1]);
    EXPECT_EQ(Vector3df(7, 8, 9), f.triangle.getVertices()[2]);
}

TEST(STLBinaryFileReaderTest, TestBinaryRoundTrip)
{
    STLFile src;
    src.header = std::string(79, ' ');
    src.faceCount = 2;

    const Triangle3df t1({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
    const Triangle3df t2({ Vector3df(1,0,0), Vector3df(1,1,0), Vector3df(0,1,0) });
    src.faces.push_back(STLFace(t1, Vector3df(0, 0, 1)));
    src.faces.push_back(STLFace(t2, Vector3df(0, 0, 1)));

    STLFileWriter writer;
    std::stringstream ss;
    EXPECT_TRUE(writer.writeBinary(ss, src));

    STLFileReader reader;
    EXPECT_TRUE(reader.readBinary(ss));
    const auto& dst = reader.getSTL();

    ASSERT_EQ(2u, dst.faces.size());

    EXPECT_EQ(Vector3df(0, 0, 1), dst.faces[0].normal);
    EXPECT_EQ(Vector3df(0, 0, 0), dst.faces[0].triangle.getVertices()[0]);
    EXPECT_EQ(Vector3df(1, 0, 0), dst.faces[0].triangle.getVertices()[1]);
    EXPECT_EQ(Vector3df(0, 1, 0), dst.faces[0].triangle.getVertices()[2]);

    EXPECT_EQ(Vector3df(0, 0, 1), dst.faces[1].normal);
    EXPECT_EQ(Vector3df(1, 0, 0), dst.faces[1].triangle.getVertices()[0]);
    EXPECT_EQ(Vector3df(1, 1, 0), dst.faces[1].triangle.getVertices()[1]);
    EXPECT_EQ(Vector3df(0, 1, 0), dst.faces[1].triangle.getVertices()[2]);
}
