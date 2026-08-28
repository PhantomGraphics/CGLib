#include "gtest/gtest.h"

#include "../File/PLYFileWriter.h"

using namespace Phantom::Math;
using namespace Phantom::File;

TEST(PLYFileWriterTest, TestWriteASCII)
{
	PLYFile ply;
	ply.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	ply.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	ply.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1;
	p1.values.push_back(1.0f);
	p1.values.push_back(2.0f);
	p1.values.push_back(3.0f);
	ply.vertices.push_back(p1);
	PLYPoint p2;
	p2.values.push_back(2.0f);
	p2.values.push_back(3.0f);
	p2.values.push_back(4.0f);
	ply.vertices.push_back(p2);
	PLYPoint p3;
	p3.values.push_back(3.0f);
	p3.values.push_back(4.0f);
	p3.values.push_back(5.0f);
	ply.vertices.push_back(p3);

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeASCII(ss, ply));
}

TEST(PLYFileWriterTest, TestWriteBinary)
{
	PLYFile ply;
	ply.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	ply.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	ply.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1;
	p1.values.push_back(1.0f);
	p1.values.push_back(2.0f);
	p1.values.push_back(3.0f);
	ply.vertices.push_back(p1);
	PLYPoint p2;
	p2.values.push_back(2.0f);
	p2.values.push_back(3.0f);
	p2.values.push_back(4.0f);
	ply.vertices.push_back(p2);
	PLYPoint p3;
	p3.values.push_back(3.0f);
	p3.values.push_back(4.0f);
	p3.values.push_back(5.0f);
	ply.vertices.push_back(p3);

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeBinary(ss, ply));
}

TEST(PLYFileWriterTest, TestWriteASCIIWithFace)
{
	PLYFile ply;
	ply.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	ply.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	ply.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1; p1.values.push_back(1.0f); p1.values.push_back(2.0f); p1.values.push_back(3.0f);
	PLYPoint p2; p2.values.push_back(2.0f); p2.values.push_back(3.0f); p2.values.push_back(4.0f);
	PLYPoint p3; p3.values.push_back(3.0f); p3.values.push_back(4.0f); p3.values.push_back(5.0f);
	ply.vertices.push_back(p1);
	ply.vertices.push_back(p2);
	ply.vertices.push_back(p3);

	// 三角形face追加
	ply.faces.push_back({ 0, 1, 2 });

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeASCII(ss, ply));
}

TEST(PLYFileWriterTest, TestWriteBinaryWithFace)
{
	PLYFile ply;
	ply.properties.push_back(PLYProperty("x", PLYType::FLOAT));
	ply.properties.push_back(PLYProperty("y", PLYType::FLOAT));
	ply.properties.push_back(PLYProperty("z", PLYType::FLOAT));

	PLYPoint p1; p1.values.push_back(1.0f); p1.values.push_back(2.0f); p1.values.push_back(3.0f);
	PLYPoint p2; p2.values.push_back(2.0f); p2.values.push_back(3.0f); p2.values.push_back(4.0f);
	PLYPoint p3; p3.values.push_back(3.0f); p3.values.push_back(4.0f); p3.values.push_back(5.0f);
	ply.vertices.push_back(p1);
	ply.vertices.push_back(p2);
	ply.vertices.push_back(p3);

	// 三角形face追加
	ply.faces.push_back({ 0, 1, 2 });

	PLYFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.writeBinary(ss, ply));
}