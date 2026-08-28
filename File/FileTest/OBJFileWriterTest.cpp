#include "gtest/gtest.h"

#include "../File/OBJFileWriter.h"
#include "../File/OBJFileReader.h"

#include <filesystem>

using namespace Phantom::Math;
using namespace Phantom::File;

TEST(OBJFileWriterTest, TestWrite)
{
	OBJFile obj;
	obj.positions.push_back(Vector3dd(0, 0, 0));
	obj.positions.push_back(Vector3dd(1, 0, 0));
	obj.positions.push_back(Vector3dd(1, 1, 0));
	obj.positions.push_back(Vector3dd(0, 1, 0));
	obj.normals.push_back(Vector3dd(0, 0, 1));
	obj.texCoords.push_back(Vector2dd(0.0, 0.0));
	obj.texCoords.push_back(Vector2dd(1.0, 0.0));
	obj.texCoords.push_back(Vector2dd(1.0, 1.0));
	obj.texCoords.push_back(Vector2dd(0.0, 1.0));

	OBJFace face1;
	face1.positionIndices = { 1,2,4 };
	face1.normalIndices = { 1, 1, 1 };
	face1.texCoordIndices = { 1,2,4 };

	OBJFace face2;
	face1.positionIndices = { 3,4,2 };
	face1.normalIndices = { 1, 1, 1 };
	face1.texCoordIndices = { 3,4,2 };

	OBJGroup group;
	group.faces.push_back(face1);
	group.faces.push_back(face2);
	obj.groups.push_back(group);

	const auto path = std::filesystem::temp_directory_path() / "OBJFileWriterTest.obj";
	OBJFileWriter writer;
	EXPECT_TRUE(writer.write(path.string(), obj));
	std::filesystem::remove(path);
}

TEST(OBJFileWriterTest, TestWriteMultipleGroups)
{
	OBJFile src;
	src.positions.push_back(Vector3df(0, 0, 0));
	src.positions.push_back(Vector3df(1, 0, 0));
	src.positions.push_back(Vector3df(0, 1, 0));
	src.positions.push_back(Vector3df(0, 0, 1));
	src.normals.push_back(Vector3df(0, 0, 1));

	OBJFace f1; f1.positionIndices = {1,2,3}; f1.normalIndices = {1,1,1}; f1.texCoordIndices = {0,0,0};
	OBJFace f2; f2.positionIndices = {1,2,4}; f2.normalIndices = {1,1,1}; f2.texCoordIndices = {0,0,0};

	OBJGroup g1; g1.name = "groupA"; g1.faces.push_back(f1);
	OBJGroup g2; g2.name = "groupB"; g2.faces.push_back(f2);
	src.groups.push_back(g1);
	src.groups.push_back(g2);

	OBJFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.write(ss, src));

	OBJFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getOBJ();

	ASSERT_EQ(2u, dst.groups.size());
	EXPECT_EQ("groupA", dst.groups[0].name);
	EXPECT_EQ("groupB", dst.groups[1].name);
	EXPECT_EQ(1u, dst.groups[0].faces.size());
	EXPECT_EQ(1u, dst.groups[1].faces.size());
}

TEST(OBJFileWriterTest, TestWriteWithMtllib)
{
	OBJFile src;
	src.mtllibs.push_back("material.mtl");
	src.positions.push_back(Vector3df(0, 0, 0));
	src.positions.push_back(Vector3df(1, 0, 0));
	src.positions.push_back(Vector3df(0, 1, 0));
	src.normals.push_back(Vector3df(0, 0, 1));

	OBJFace f; f.positionIndices = {1,2,3}; f.normalIndices = {1,1,1}; f.texCoordIndices = {0,0,0};
	OBJGroup g; g.name = "mesh"; g.usemtl = "mat0"; g.faces.push_back(f);
	src.groups.push_back(g);

	OBJFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.write(ss, src));

	OBJFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getOBJ();

	ASSERT_EQ(1u, dst.mtllibs.size());
	EXPECT_EQ("material.mtl", dst.mtllibs[0]);

	ASSERT_EQ(1u, dst.groups.size());
	EXPECT_EQ("mat0", dst.groups[0].usemtl);
}