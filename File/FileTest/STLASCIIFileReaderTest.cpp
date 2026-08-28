#include "gtest/gtest.h"

#include "CGLib/Math/Vector3d.h"
#include "../File/STLFileReader.h"
#include "../File/STLFileWriter.h"

using namespace Phantom::Math;
using namespace Phantom::File;

namespace {
	std::stringstream getSampleAscii() {
		std::stringstream stream;
		stream
			<< "solid cube-ascii" << std::endl
			<< "facet normal  0.0  0.0 1.0" << std::endl
			<< "	outer loop" << std::endl
			<< "		vertex  0.0  0.0  1.0" << std::endl
			<< "		vertex  1.0  0.0  1.0" << std::endl
			<< "		vertex  0.0  1.0  1.0" << std::endl
			<< "endloop" << std::endl
			<< "endfacet" << std::endl
			<< "facet normal 0.0 0.0 1.0" << std::endl
			<< "	outer loop" << std::endl
			<< "		vertex  1.0  1.0  1.0" << std::endl
			<< "		vertex  0.0  1.0  1.0" << std::endl
			<< "		vertex  1.0  0.0  1.0" << std::endl
			<< "	endloop" << std::endl
			<< "endfacet" << std::endl
			<< "endsolid" << std::endl;
		return stream;
	}

	std::stringstream getNanSample() {
		std::stringstream stream;
		stream
			<< "solid sample-nan" << std::endl
			<< "facet normal -nan(ind) -nan(ind) -nan(ind)" << std::endl
			<< "	outer loop" << std::endl
			<< "		vertex 4.05018 3.02242 - 1.09746" << std::endl
			<< "		vertex 4.05018 1.99465 - 1.09387" << std::endl
			<< "		vertex 4.05018 3.02242 - 1.09746" << std::endl
			<< "endloop" << std::endl
			<< "endfacet" << std::endl
			<< "endsolid" << std::endl;
		return stream;
	}

}

TEST(STLASCIIFileReaderTest, TestRead)
{
	auto stream = getSampleAscii();

	STLFileReader reader;
	EXPECT_TRUE(reader.readAscii(stream));
	const auto& faces = reader.getSTL().faces;

	EXPECT_EQ(2, faces.size());

	const auto& face1 = faces[0];
	EXPECT_EQ(Vector3df(0, 0, 1), face1.normal);
	EXPECT_EQ(Vector3df(0, 0, 1), face1.triangle.getVertices()[0]);
	EXPECT_EQ(Vector3df(1, 0, 1), face1.triangle.getVertices()[1]);
	EXPECT_EQ(Vector3df(0, 1, 1), face1.triangle.getVertices()[2]);

	const auto& face2 = faces[1];
	EXPECT_EQ(Vector3df(0, 0, 1), face2.normal);
	EXPECT_EQ(Vector3df(1, 1, 1), face2.triangle.getVertices()[0]);
	EXPECT_EQ(Vector3df(0, 1, 1), face2.triangle.getVertices()[1]);
	EXPECT_EQ(Vector3df(1, 0, 1), face2.triangle.getVertices()[2]);
}

TEST(STLASCIIFileReaderTest, TestReadNan)
{
	auto stream = getNanSample();

	STLFileReader reader;
	const auto isOk = reader.readAscii(stream);
	EXPECT_FALSE(isOk);
}

TEST(STLASCIIFileReaderTest, TestAsciiRoundTrip)
{
	STLFile src;
	src.header = "round-trip-test";
	const Triangle3df t1({ Vector3df(0,0,0), Vector3df(1,0,0), Vector3df(0,1,0) });
	const Triangle3df t2({ Vector3df(1,0,0), Vector3df(1,1,0), Vector3df(0,1,0) });
	src.faces.push_back(STLFace(t1, Vector3df(0, 0, 1)));
	src.faces.push_back(STLFace(t2, Vector3df(0, 0, 1)));

	STLFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeAscii(ss, src));

	STLFileReader reader;
	EXPECT_TRUE(reader.readAscii(ss));
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