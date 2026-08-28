#include "gtest/gtest.h"

#include "../File/PLYFileReader.h"
#include "../File/PLYFileWriter.h"

using namespace Phantom::Math;
using namespace Phantom::File;

TEST(PLYFileReaderTest, TestReadASCII)
{
	PLYFile src;
	src.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1;
	p1.values.push_back(1.0f);
	p1.values.push_back(2.0f);
	p1.values.push_back(3.0f);
	src.vertices.push_back(p1);
	PLYPoint p2;
	p2.values.push_back(2.0f);
	p2.values.push_back(3.0f);
	p2.values.push_back(4.0f);
	src.vertices.push_back(p2);
	PLYPoint p3;
	p3.values.push_back(3.0f);
	p3.values.push_back(4.0f);
	p3.values.push_back(5.0f);
	src.vertices.push_back(p3);

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeASCII(ss, src));

	PLYFileReader reader;
	const auto isOk = reader.read(ss);
	EXPECT_TRUE(isOk);
	EXPECT_EQ(3, reader.getPLY().vertices.size());

	const auto ply = reader.getPLY();
	EXPECT_EQ(1.0f, ply.vertices[0].getValueAs<float>(0));
	EXPECT_EQ(2.0f, ply.vertices[0].getValueAs<float>(1));
	EXPECT_EQ(3.0f, ply.vertices[0].getValueAs<float>(2));
}

TEST(PLYFileReaderTest, TestReadBinary)
{
	PLYFile src;
	src.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1;
	p1.values.push_back(1.0f);
	p1.values.push_back(2.0f);
	p1.values.push_back(3.0f);
	src.vertices.push_back(p1);
	PLYPoint p2;
	p2.values.push_back(2.0f);
	p2.values.push_back(3.0f);
	p2.values.push_back(4.0f);
	src.vertices.push_back(p2);
	PLYPoint p3;
	p3.values.push_back(3.0f);
	p3.values.push_back(4.0f);
	p3.values.push_back(5.0f);
	src.vertices.push_back(p3);

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeBinary(ss, src));

	PLYFileReader reader;
	const auto isOk = reader.read(ss);
	EXPECT_TRUE(isOk);
	EXPECT_EQ(3, reader.getPLY().vertices.size());
}

TEST(PLYFileReaderTest, TestAsciiRoundTrip)
{
	PLYFile src;
	src.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1; p1.values.push_back(1.0f); p1.values.push_back(2.0f); p1.values.push_back(3.0f);
	PLYPoint p2; p2.values.push_back(4.0f); p2.values.push_back(5.0f); p2.values.push_back(6.0f);
	src.vertices.push_back(p1);
	src.vertices.push_back(p2);

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeASCII(ss, src));

	PLYFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getPLY();

	ASSERT_EQ(2u, dst.vertices.size());
	EXPECT_FLOAT_EQ(1.0f, dst.vertices[0].getValueAs<float>(0));
	EXPECT_FLOAT_EQ(2.0f, dst.vertices[0].getValueAs<float>(1));
	EXPECT_FLOAT_EQ(3.0f, dst.vertices[0].getValueAs<float>(2));
	EXPECT_FLOAT_EQ(4.0f, dst.vertices[1].getValueAs<float>(0));
}

TEST(PLYFileReaderTest, TestBinaryRoundTrip)
{
	PLYFile src;
	src.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1; p1.values.push_back(1.0f); p1.values.push_back(2.0f); p1.values.push_back(3.0f);
	PLYPoint p2; p2.values.push_back(4.0f); p2.values.push_back(5.0f); p2.values.push_back(6.0f);
	src.vertices.push_back(p1);
	src.vertices.push_back(p2);

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeBinary(ss, src));

	PLYFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getPLY();

	ASSERT_EQ(2u, dst.vertices.size());
	EXPECT_FLOAT_EQ(1.0f, dst.vertices[0].getValueAs<float>(0));
	EXPECT_FLOAT_EQ(2.0f, dst.vertices[0].getValueAs<float>(1));
	EXPECT_FLOAT_EQ(3.0f, dst.vertices[0].getValueAs<float>(2));
	EXPECT_FLOAT_EQ(4.0f, dst.vertices[1].getValueAs<float>(0));
}

TEST(PLYFileReaderTest, TestBinaryRoundTripWithFace)
{
	PLYFile src;
	src.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1; p1.values.push_back(0.0f); p1.values.push_back(0.0f); p1.values.push_back(0.0f);
	PLYPoint p2; p2.values.push_back(1.0f); p2.values.push_back(0.0f); p2.values.push_back(0.0f);
	PLYPoint p3; p3.values.push_back(0.0f); p3.values.push_back(1.0f); p3.values.push_back(0.0f);
	src.vertices.push_back(p1);
	src.vertices.push_back(p2);
	src.vertices.push_back(p3);
	src.faces.push_back({ 0, 1, 2 });

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeBinary(ss, src));

	PLYFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getPLY();

	ASSERT_EQ(3u, dst.vertices.size());
	ASSERT_EQ(1u, dst.faces.size());
	ASSERT_EQ(3u, dst.faces[0].size());
	EXPECT_EQ(0u, dst.faces[0][0]);
	EXPECT_EQ(1u, dst.faces[0][1]);
	EXPECT_EQ(2u, dst.faces[0][2]);
}

TEST(PLYFileReaderTest, TestAsciiRoundTripWithFace)
{
	PLYFile src;
	src.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	src.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1; p1.values.push_back(0.0f); p1.values.push_back(0.0f); p1.values.push_back(0.0f);
	PLYPoint p2; p2.values.push_back(1.0f); p2.values.push_back(0.0f); p2.values.push_back(0.0f);
	PLYPoint p3; p3.values.push_back(0.0f); p3.values.push_back(1.0f); p3.values.push_back(0.0f);
	src.vertices.push_back(p1);
	src.vertices.push_back(p2);
	src.vertices.push_back(p3);
	src.faces.push_back({ 0, 1, 2 });

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeASCII(ss, src));

	PLYFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getPLY();

	ASSERT_EQ(3u, dst.vertices.size());
	ASSERT_EQ(1u, dst.faces.size());
	ASSERT_EQ(3u, dst.faces[0].size());
	EXPECT_EQ(0u, dst.faces[0][0]);
	EXPECT_EQ(1u, dst.faces[0][1]);
	EXPECT_EQ(2u, dst.faces[0][2]);
}