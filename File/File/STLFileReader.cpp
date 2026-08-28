#include "STLFileReader.h"

#include <fstream>
#include "Helper.h"


using namespace Phantom::Math;
using namespace Phantom::File;

namespace {
	size_t getStreamSize(std::istream& stream)
	{
		stream.seekg(0, std::ios::end);
		std::fstream::pos_type pos_end = stream.tellg();

		stream.seekg(0, std::ios::beg);
		std::fstream::pos_type pos_beg = stream.tellg();

		return pos_end - pos_beg;
	}
}

bool STLFileReader::isBinary(const std::filesystem::path& filePath)
{
	std::ifstream stream(filePath, std::ios::binary);
	if (!stream.is_open()) {
		return false;
	}
	return isBinary(stream);
}

bool STLFileReader::isBinary(std::istream& in)
{
	size_t fileSize = getStreamSize(in);

	char head[80];
	in.read(head, 80);

	unsigned int faceCount = 0;
	in.read((char*)&faceCount, sizeof(unsigned int));

	const auto expectedBinaryFileSize = faceCount * 50 + 84;
	in.seekg(0, std::ios::beg);

	return (expectedBinaryFileSize == fileSize);
}

bool STLFileReader::readAscii(const std::filesystem::path& filePath)
{
	std::ifstream stream(filePath);
	if (!stream.is_open()) {
		return false;
	}
	return readAscii(stream);
}

bool STLFileReader::readAscii(std::istream& stream)
{
	std::string str = Helper::read<std::string>(stream);

	if (str != "solid") {
		return false;
	}

	std::getline(stream, str);

	stl.header = str;

	str = Helper::read<std::string>(stream);
	while (str != "endsolid") {
		if (str != "facet") {
			return false;
		}
		str = Helper::read<std::string>(stream);
		if (str != "normal") {
			return false;
		}

		const auto& normal = Helper::readVector<double>(stream);

		stream >> str;
		if (str != "outer") {
			return false;
		}

		stream >> str;
		if (str != "loop") {
			return false;
		}

		std::array<Vector3df, 3> vertices;
		for (int i = 0; i < 3; ++i) {
			stream >> str;
			if (str != "vertex") {
				return false;
			}

			vertices[i] = Helper::readVector<double>(stream);
		}

		stl.faces.push_back(STLFace(Triangle3df(vertices), normal));

		str = Helper::read<std::string>(stream);
		if (str != "endloop") {
			return false;
		}

		str = Helper::read<std::string>(stream);
		if (str != "endfacet") {
			return false;
		}

		str = Helper::read<std::string>(stream);
	}

	return stream.good();
}

bool STLFileReader::readBinary(const std::filesystem::path& filePath)
{
	std::ifstream stream(filePath, std::ios::binary);
	if (!stream.is_open()) {
		return false;
	}
	return readBinary(stream);
}

bool STLFileReader::readBinary(std::istream& stream)
{
	char head[80];
	stream.read(head, 80);
	stl.header = head;

	unsigned int howMany;
	stream.read((char*)&howMany, sizeof(unsigned int));

	for (size_t i = 0; i < howMany; ++i) {
		float normal[3];
		stream.read((char*)&normal, sizeof(float) * 3);
		float pos0[3];
		stream.read((char*)&pos0, sizeof(float) * 3);
		float pos1[3];
		stream.read((char*)&pos1, sizeof(float) * 3);
		float pos2[3];
		stream.read((char*)&pos2, sizeof(float) * 3);
		char padding[2];
		stream.read((char*)padding, sizeof(char) * 2);

		if (!stream.good()) {
			assert(false);
		}

		Vector3df fn(normal[0], normal[1], normal[2]);
		std::array< Vector3df, 3 > vertices = {
			Vector3df(pos0[0], pos0[1], pos0[2]),
			Vector3df(pos1[0], pos1[1], pos1[2]),
			Vector3df(pos2[0], pos2[1], pos2[2]),
		};
		STLFace face(Triangle3df(vertices), fn);
		stl.faces.push_back(face);
	}
	return true;
	//return stream.good();
}