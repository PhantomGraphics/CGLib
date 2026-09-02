#include "gtest/gtest.h"

#include <cstdint>
#include <sstream>
#include "../File/STLFileWriter.h"

using namespace Phantom::Math;
using namespace Phantom::File;

namespace {
    STLFile makeTwoFaceSTL()
    {
        STLFile stl;
        stl.header = std::string(79, ' ');
        stl.faceCount = 2;
        const Triangle3df t1({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
        const Triangle3df t2({ Vector3df(1,0,0), Vector3df(1,1,0), Vector3df(0,1,0) });
        stl.faces.push_back(STLFace(t1, Vector3df(0, 0, 1)));
        stl.faces.push_back(STLFace(t2, Vector3df(0, 0, 1)));
        return stl;
    }
}

TEST(STLBinaryFileWriterTest, TestWriteBinaryStreamSize)
{
    // Binary STL layout: 80-byte header + 4-byte count + N * 50-byte records
    const auto stl = makeTwoFaceSTL();
    const std::streamsize expected = 80 + 4 + static_cast<std::streamsize>(stl.faces.size()) * 50;

    STLFileWriter writer;
    std::stringstream ss;
    EXPECT_TRUE(writer.writeBinary(ss, stl));

    ss.seekg(0, std::ios::end);
    EXPECT_EQ(expected, ss.tellg());
}

TEST(STLBinaryFileWriterTest, TestWriteBinaryFaceCount)
{
    const auto stl = makeTwoFaceSTL();

    STLFileWriter writer;
    std::stringstream ss;
    EXPECT_TRUE(writer.writeBinary(ss, stl));

    // Face count is stored at byte offset 80 as a uint32_t
    ss.seekg(80);
    uint32_t count = 0;
    ss.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));
    EXPECT_EQ(2u, count);
}

TEST(STLBinaryFileWriterTest, TestWriteBinaryEmptySTL)
{
    STLFile stl;
    stl.header = std::string(79, ' ');

    STLFileWriter writer;
    std::stringstream ss;
    EXPECT_TRUE(writer.writeBinary(ss, stl));

    ss.seekg(0, std::ios::end);
    // 80-byte header + 4-byte count, no face records
    EXPECT_EQ(84, ss.tellg());
}

TEST(STLBinaryFileWriterTest, TestWriteBinaryShortHeaderIsZeroPadded)
{
    // Regression test: a header much shorter than 80 bytes used to read past the end of
    // std::string::c_str()'s buffer (undefined behavior) because the old implementation wrote
    // stream.write(header.c_str(), 80) unconditionally. Earlier tests never caught this since
    // they always padded the header to exactly 79 characters.
    STLFile stl;
    stl.header = "Phantom";

    STLFileWriter writer;
    std::stringstream ss;
    EXPECT_TRUE(writer.writeBinary(ss, stl));

    const std::string data = ss.str();
    ASSERT_GE(data.size(), 80u);
    EXPECT_EQ(0, data.compare(0, 7, "Phantom"));
    for (size_t i = 7; i < 80; ++i)
        EXPECT_EQ('\0', data[i]) << "byte " << i;
}

TEST(STLBinaryFileWriterTest, TestWriteBinaryEmptyHeaderIsZeroPadded)
{
    STLFile stl; // header left default-constructed (empty)

    STLFileWriter writer;
    std::stringstream ss;
    EXPECT_TRUE(writer.writeBinary(ss, stl));

    const std::string data = ss.str();
    ASSERT_GE(data.size(), 80u);
    for (size_t i = 0; i < 80; ++i)
        EXPECT_EQ('\0', data[i]) << "byte " << i;
}
