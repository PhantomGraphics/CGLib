#include "gtest/gtest.h"

#include "../File/STLFileWriter.h"

#include <filesystem>

using namespace Phantom::Math;
using namespace Phantom::File;

TEST(STLASCIILFileWriterTest, TestWrite)
{
	const Triangle3df t1
	(
		{
		Vector3df(0, 1, 2),
		Vector3df(3, 4, 5),
		Vector3df(6, 7, 8)
		}
	);
	const Triangle3df t2
	(
		{
		Vector3df(10, 11, 12),
		Vector3df(13, 14, 15),
		Vector3df(16, 17, 18)
		}
	);
	const Triangle3df t3
	(
		{
		Vector3df(0, 0, 0),
		Vector3df(1, 0, 0),
		Vector3df(0, 1, 0)
		}
	);


	STLFile stl;
	stl.header = "";
	stl.faceCount = 3;
	stl.faces.push_back(STLFace(t1, Vector3df(0, 0, 1)));
	stl.faces.push_back(STLFace(t2, Vector3df(0, 0, 1)));
	stl.faces.push_back(STLFace(t3, Vector3df(0, 0, 1)));

	const auto path = std::filesystem::temp_directory_path() / "STLASCIIFileWriterTest.stl";
	STLFileWriter writer;
	writer.writeAscii(path.string(), stl);
	std::filesystem::remove(path);
}