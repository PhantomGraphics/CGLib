#include "gtest/gtest.h"
#include "../File/OBJFileReader.h"
#include "../File/OBJFileWriter.h"

#include <filesystem>
#include <fstream>

using namespace Phantom::Math;
using namespace Phantom::File;

// from http://www.martinreddy.net/gfx/3d/OBJ.spec
TEST(OBJFileReaderTest, TestExampleSquare)
{
	std::stringstream stream;
	stream
		<< "v 0.000000 2.000000 0.000000" << std::endl
		<< "v 0.000000 0.000000 0.000000" << std::endl
		<< "v 2.000000 0.000000 0.000000" << std::endl
		<< "v 2.000000 2.000000 0.000000" << std::endl
		<< "f 1 2 3 4" << std::endl;
	OBJFileReader reader;
	reader.read(stream);
	const auto& obj = reader.getOBJ();
	EXPECT_EQ(4, obj.positions.size());
}

TEST(OBJFileReaderTest, TestExampleCube)
{
	std::stringstream stream;
	stream
		<< "v 0.000000 2.000000 2.000000" << std::endl
		<< "v 0.000000 0.000000 2.000000" << std::endl
		<< "v 2.000000 0.000000 2.000000" << std::endl
		<< "v 2.000000 2.000000 2.000000" << std::endl
		<< "v 0.000000 2.000000 0.000000" << std::endl
		<< "v 0.000000 0.000000 0.000000" << std::endl
		<< "v 2.000000 0.000000 0.000000" << std::endl
		<< "v 2.000000 2.000000 0.000000" << std::endl
		<< "f 1 2 3 4" << std::endl
		<< "f 8 7 6 5" << std::endl
		<< "f 4 3 7 8" << std::endl
		<< "f 5 1 4 8" << std::endl
		<< "f 5 6 2 1" << std::endl
		<< "f 2 6 7 3" << std::endl;

	OBJFileReader reader;
	reader.read(stream);
	const auto& obj = reader.getOBJ();
	EXPECT_EQ(8, obj.positions.size());
	EXPECT_EQ(0, obj.normals.size());
	EXPECT_EQ(1, obj.groups.size());
	EXPECT_EQ(6, obj.groups[0].faces.size());
}

TEST(OBJFileReaderTest, TestNegativeReferenceNumber)
{
	std::stringstream stream;
	stream
		<< "v 0.000000 2.000000 2.000000" << std::endl
		<< "v 0.000000 0.000000 2.000000" << std::endl
		<< "v 2.000000 0.000000 2.000000" << std::endl
		<< "v 2.000000 2.000000 2.000000" << std::endl
		<< "f -4 -3 -2 -1" << std::endl;
	OBJFileReader reader;
	reader.read(stream);
	const auto& obj = reader.getOBJ();
	EXPECT_EQ(4, obj.positions.size());
	EXPECT_EQ(1, obj.groups.size());
	EXPECT_EQ(1, obj.groups[0].faces.size());
}

TEST(OBJFileReaderTest, TestExampleGroups)
{
	std::stringstream stream;
	stream
		<< "g front cube" << std::endl
		<< "f 1 2 3 4" << std::endl
		<< "g back cube" << std::endl
		<< "f 8 7 6 5" << std::endl
		<< "g right cube" << std::endl
		<< "f 4 3 7 8" << std::endl
		<< "g top cube" << std::endl
		<< "f 5 1 4 8" << std::endl
		<< "g left cube" << std::endl
		<< "f 5 6 2 1" << std::endl
		<< "g bottom cube" << std::endl
		<< "f 2 6 7 3" << std::endl
		<< "# 6 elements" << std::endl;

	OBJFileReader reader;
	reader.read(stream);
	const auto& obj = reader.getOBJ();
	EXPECT_EQ(6, obj.groups.size());
}

TEST(OBJFileReaderTest, TestExampleSmoothingGroup)
{
	std::stringstream stream;
	stream
		<< "v 0.000000 2.000000 0.000000" << std::endl
		<< "v 0.000000 0.000000 0.000000" << std::endl
		<< "v 2.000000 0.000000 0.000000" << std::endl
		<< "v 2.000000 2.000000 0.000000" << std::endl
		<< "v 4.000000 0.000000 -1.255298" << std::endl
		<< "v 4.000000 2.000000 -1.255298" << std::endl
		<< "# 6 vertices" << std::endl
		<< "g all" << std::endl
		<< "s 1" << std::endl
		<< "f 1 2 3 4" << std::endl
		<< "f 4 3 5 6" << std::endl
		<< "# 2 elements" << std::endl;

	OBJFileReader reader;
	reader.read(stream);
	const auto& obj = reader.getOBJ();
	EXPECT_EQ(2, obj.groups[0].faces.size());
	EXPECT_EQ(6, obj.positions.size());
}

TEST(OBJFileReaderTest, TestExampleTextureMappedSquare)
{
	std::stringstream stream;
	stream
		<< "mtllib master.mtl" << std::endl
		<< "v 0.000000 2.000000 0.000000" << std::endl
		<< "v 0.000000 0.000000 0.000000" << std::endl
		<< "v 2.000000 0.000000 0.000000" << std::endl
		<< "v 2.000000 2.000000 0.000000" << std::endl
		<< "vt 0.000000 1.000000 0.000000" << std::endl
		<< "vt 0.000000 0.000000 0.000000" << std::endl
		<< "vt 1.000000 0.000000 0.000000" << std::endl
		<< "vt 1.000000 1.000000 0.000000" << std::endl
		<< "# 4 vertices" << std::endl
		<< "usemtl wood" << std::endl
		<< "f 1/1 2/2 3/3 4/4" << std::endl
		<< "# 1 element" << std::endl;

	OBJFileReader reader;
	reader.read(stream);
	const auto& obj = reader.getOBJ();
	EXPECT_EQ(1, obj.groups[0].faces.size());
	EXPECT_EQ(4, obj.positions.size());
	EXPECT_EQ(4, obj.texCoords.size());
}

TEST(OBJFileReaderTest, TestReadGroup)
{
	std::stringstream stream;
	stream
		<< "g" << std::endl
		<< "f 1//1 2//1 4//1 3//1" << std::endl
		<< "f 6//2 5//2 7//2 8//2" << std::endl
		<< "f 1//3 3//3 7//3 5//3" << std::endl
		<< "f 2//4 6//4 8//4 4//4" << std::endl
		<< "f 3//5 4//5 8//5 7//5" << std::endl
		<< "f 1//6 5//6 6//6 2//6" << std::endl;

	OBJFileReader reader;
	EXPECT_TRUE(reader.read(stream));
	const auto obj = reader.getOBJ();
	EXPECT_EQ(1, obj.groups.size());
	EXPECT_EQ(6, obj.groups.front().faces.size());
}

TEST(OBJFileReaderTest, TestRoundTripBasic)
{
	OBJFile src;
	src.positions.push_back(Vector3df(1, 0, 0));
	src.positions.push_back(Vector3df(0, 1, 0));
	src.positions.push_back(Vector3df(0, 0, 1));
	src.normals.push_back(Vector3df(0, 0, 1));
	src.texCoords.push_back(Vector2df(0, 0));
	src.texCoords.push_back(Vector2df(1, 0));
	src.texCoords.push_back(Vector2df(0, 1));

	OBJFace face;
	face.positionIndices = { 1, 2, 3 };
	face.normalIndices   = { 1, 1, 1 };
	face.texCoordIndices = { 1, 2, 3 };

	OBJGroup group;
	group.name = "tri";
	group.faces.push_back(face);
	src.groups.push_back(group);

	OBJFileWriter writer;
	std::stringstream ss;
	EXPECT_TRUE(writer.write(ss, src));

	OBJFileReader reader;
	EXPECT_TRUE(reader.read(ss));
	const auto& dst = reader.getOBJ();

	ASSERT_EQ(3u, dst.positions.size());
	EXPECT_EQ(Vector3df(1, 0, 0), dst.positions[0]);
	EXPECT_EQ(Vector3df(0, 1, 0), dst.positions[1]);
	EXPECT_EQ(Vector3df(0, 0, 1), dst.positions[2]);

	ASSERT_EQ(1u, dst.normals.size());
	EXPECT_EQ(Vector3df(0, 0, 1), dst.normals[0]);

	ASSERT_EQ(3u, dst.texCoords.size());
	EXPECT_EQ(Vector2df(0, 0), dst.texCoords[0]);

	ASSERT_EQ(1u, dst.groups.size());
	EXPECT_EQ("tri", dst.groups[0].name);
	ASSERT_EQ(1u, dst.groups[0].faces.size());

	const auto& f = dst.groups[0].faces[0];
	ASSERT_EQ(3u, f.positionIndices.size());
	EXPECT_EQ(1u, f.positionIndices[0]);
	EXPECT_EQ(2u, f.positionIndices[1]);
	EXPECT_EQ(3u, f.positionIndices[2]);
}

TEST(OBJFileReaderTest, TestLoadsReferencedMtllib)
{
	const auto dir = std::filesystem::temp_directory_path() / "OBJFileReaderTest_TestLoadsReferencedMtllib";
	std::filesystem::create_directories(dir);
	const auto objPath = dir / "square.obj";
	const auto mtlPath = dir / "master.mtl";

	{
		std::ofstream mtlStream(mtlPath);
		mtlStream
			<< "newmtl wood" << std::endl
			<< "Kd 0.8 0.4 0.1" << std::endl;
	}
	{
		std::ofstream objStream(objPath);
		objStream
			<< "mtllib master.mtl" << std::endl
			<< "v 0.000000 2.000000 0.000000" << std::endl
			<< "v 0.000000 0.000000 0.000000" << std::endl
			<< "v 2.000000 0.000000 0.000000" << std::endl
			<< "v 2.000000 2.000000 0.000000" << std::endl
			<< "usemtl wood" << std::endl
			<< "f 1 2 3 4" << std::endl;
	}

	OBJFileReader reader;
	ASSERT_TRUE(reader.read(objPath));
	const auto& obj = reader.getOBJ();

	ASSERT_EQ(1u, obj.mtl.materials.size());
	EXPECT_EQ("wood", obj.mtl.materials[0].name);
	EXPECT_FLOAT_EQ(0.8f, obj.mtl.materials[0].diffuse.r);
	EXPECT_FLOAT_EQ(0.4f, obj.mtl.materials[0].diffuse.g);
	EXPECT_FLOAT_EQ(0.1f, obj.mtl.materials[0].diffuse.b);
	ASSERT_EQ(1u, obj.groups.size());
	EXPECT_EQ("wood", obj.groups[0].usemtl);

	std::filesystem::remove_all(dir);
}

TEST(OBJFileReaderTest, MissingMtllibDoesNotFailObjRead)
{
	const auto dir = std::filesystem::temp_directory_path() / "OBJFileReaderTest_MissingMtllib";
	std::filesystem::create_directories(dir);
	const auto objPath = dir / "square.obj";

	{
		std::ofstream objStream(objPath);
		objStream
			<< "mtllib does_not_exist.mtl" << std::endl
			<< "v 0.000000 2.000000 0.000000" << std::endl
			<< "v 0.000000 0.000000 0.000000" << std::endl
			<< "v 2.000000 0.000000 0.000000" << std::endl
			<< "v 2.000000 2.000000 0.000000" << std::endl
			<< "f 1 2 3 4" << std::endl;
	}

	OBJFileReader reader;
	EXPECT_TRUE(reader.read(objPath));
	const auto& obj = reader.getOBJ();
	EXPECT_EQ(4u, obj.positions.size());
	EXPECT_TRUE(obj.mtl.materials.empty());

	std::filesystem::remove_all(dir);
}