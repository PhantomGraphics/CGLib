#include "gtest/gtest.h"

#include <sstream>
#include "../File/STLFileReader.h"
#include "../File/OBJFileReader.h"
#include "../File/MTLFileReader.h"
#include "../File/PLYFileReader.h"

using namespace Phantom::File;

// --- STL ASCII error handling ---

TEST(FileErrorHandlingTest, TestSTLReadAsciiNonexistentFile)
{
    STLFileReader reader;
    EXPECT_FALSE(reader.readAscii("__nonexistent_file__.stl"));
}

TEST(FileErrorHandlingTest, TestSTLReadBinaryNonexistentFile)
{
    STLFileReader reader;
    EXPECT_FALSE(reader.readBinary("__nonexistent_file__.stl"));
}

TEST(FileErrorHandlingTest, TestSTLReadAsciiEmptyStream)
{
    std::stringstream ss;
    STLFileReader reader;
    EXPECT_FALSE(reader.readAscii(ss));
}

TEST(FileErrorHandlingTest, TestSTLReadAsciiInvalidKeyword)
{
    // Stream starts with a token other than "solid"
    std::stringstream ss;
    ss << "notsolid mesh" << std::endl
       << "endsolid" << std::endl;
    STLFileReader reader;
    EXPECT_FALSE(reader.readAscii(ss));
}

// --- OBJ error handling ---

TEST(FileErrorHandlingTest, TestOBJReadNonexistentFile)
{
    OBJFileReader reader;
    EXPECT_FALSE(reader.read("__nonexistent_file__.obj"));
}

TEST(FileErrorHandlingTest, TestOBJReadMalformedVertexIsSkippedNotCrash)
{
    // Non-numeric token in a "v" line must not crash the reader (previously an
    // unguarded std::stof would throw std::invalid_argument here).
    std::stringstream ss;
    ss << "v 0.0 not_a_number 0.0" << std::endl
       << "v 1.0 2.0 3.0" << std::endl;
    OBJFileReader reader;
    EXPECT_TRUE(reader.read(ss));
    const auto& obj = reader.getOBJ();
    // The malformed line is skipped; only the valid vertex is kept.
    EXPECT_EQ(1u, obj.positions.size());
}

TEST(FileErrorHandlingTest, TestOBJReadMalformedFaceIsSkippedNotCrash)
{
    // Non-numeric index token in an "f" line must not crash the reader
    // (previously an unguarded std::stoi would throw std::invalid_argument here).
    std::stringstream ss;
    ss << "v 0.0 0.0 0.0" << std::endl
       << "v 1.0 0.0 0.0" << std::endl
       << "v 0.0 1.0 0.0" << std::endl
       << "f abc def ghi" << std::endl
       << "f 1 2 3" << std::endl;
    OBJFileReader reader;
    EXPECT_TRUE(reader.read(ss));
    const auto& obj = reader.getOBJ();
    ASSERT_EQ(1u, obj.groups.size());
    EXPECT_EQ(1u, obj.groups[0].faces.size());
}

// --- MTL error handling ---

TEST(FileErrorHandlingTest, TestMTLReadNonexistentFile)
{
    MTLFileReader reader;
    EXPECT_FALSE(reader.read("__nonexistent_file__.mtl"));
}

// --- PLY error handling ---

TEST(FileErrorHandlingTest, TestPLYReadNonexistentFile)
{
    PLYFileReader reader;
    EXPECT_FALSE(reader.read("__nonexistent_file__.ply"));
}

TEST(FileErrorHandlingTest, TestPLYReadMalformedVertexCountFails)
{
    // Non-numeric vertex count in the header must not crash the reader
    // (previously an unguarded std::stoi would throw std::invalid_argument here).
    std::stringstream ss;
    ss << "format ascii 1.0" << std::endl
       << "element vertex not_a_number" << std::endl
       << "property float x" << std::endl
       << "property float y" << std::endl
       << "property float z" << std::endl
       << "end_header" << std::endl
       << "0.0 0.0 0.0" << std::endl;
    PLYFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}

TEST(FileErrorHandlingTest, TestPLYReadMalformedVertexValueFails)
{
    // Non-numeric vertex value in the data section must not crash the reader
    // (previously an unguarded std::stof would throw std::invalid_argument here).
    std::stringstream ss;
    ss << "format ascii 1.0" << std::endl
       << "element vertex 1" << std::endl
       << "property float x" << std::endl
       << "property float y" << std::endl
       << "property float z" << std::endl
       << "end_header" << std::endl
       << "0.0 not_a_number 0.0" << std::endl;
    PLYFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}

TEST(FileErrorHandlingTest, TestPLYReadFaceLineWithTooFewIndicesFails)
{
    // A face line whose declared vertex count exceeds the number of index
    // tokens actually present must not crash the reader (previously an
    // unguarded std::stoi/vector operator[] would misbehave here).
    std::stringstream ss;
    ss << "format ascii 1.0" << std::endl
       << "element vertex 3" << std::endl
       << "property float x" << std::endl
       << "property float y" << std::endl
       << "property float z" << std::endl
       << "element face 1" << std::endl
       << "property list uchar int vertex_indices" << std::endl
       << "end_header" << std::endl
       << "0.0 0.0 0.0" << std::endl
       << "1.0 0.0 0.0" << std::endl
       << "0.0 1.0 0.0" << std::endl
       << "3 0 1" << std::endl;
    PLYFileReader reader;
    EXPECT_FALSE(reader.read(ss));
}
