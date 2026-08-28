#define _CRT_SECURE_NO_DEPRECATE
#include "MTLFileReader.h"

#include "Helper.h"

#include <vector>

#include <cassert>

#include <string>

#include "CGLib/Graphics/ColorRGBA.h"


using namespace Phantom::Graphics;
using namespace Phantom::File;

bool MTLFileReader::read(const std::filesystem::path& filePath)
{
	std::ifstream stream;
	stream.open(filePath);

	if (!stream.is_open()) {
		return false;
	}

	return read(stream);
}

bool MTLFileReader::read(std::istream& stream)
{
	std::string str;

	std::string comment;

	std::string header = Helper::read< std::string >(stream);

	while (!stream.eof()) {
		if (header.front() == '#') {
			//std::getline( stream, header );
		}
		else if (header == "newmtl") {
			MTL mtl;
			mtl.name = Helper::read<std::string>(stream);
			this->mtl.materials.push_back(mtl);
			std::getline(stream, header);
		}
		else if (header == "Ka") {
			this->mtl.materials.back().ambient = Helper::readColorRGB(stream);
		}
		else if (header == "Kd") {
			this->mtl.materials.back().diffuse = Helper::readColorRGB(stream);
		}
		else if (header == "Ks") {
			this->mtl.materials.back().specular = Helper::readColorRGB(stream);
		}
		else if (header == "Ns") {
			this->mtl.materials.back().specularExponent = Helper::read< float >(stream);
		}
		else if (header == "d" || header == "Tr") {
			this->mtl.materials.back().transparent = Helper::read<float>(stream);
		}
		else if (header == "map_Ka") {
			this->mtl.materials.back().ambientTexture = Helper::read< std::string >(stream);
		}
		else if (header == "map_Kd") {
			std::string str = Helper::read< std::string >(stream);
			this->mtl.materials.back().diffuseTexture = str;
		}
		else if (header == "map_Ks") {
			this->mtl.materials.back().shininessTexture = Helper::read< std::string >(stream);
		}
		else if (header == "map_Ns") {
			const std::string& filename = Helper::read< std::string >(stream);
		}
		else if (header == "map_d") {
			const std::string& filename = Helper::read< std::string >(stream);
		}
		else if (header == "map_bump" || header == "bump") {
			this->mtl.materials.back().bumpTexture = Helper::read< std::string >(stream);
		}
		else if (header == "illum") {
			this->mtl.materials.back().illumination = MTL::Illumination(Helper::read< int >(stream));
		}
		else if (header == "Ni") {
			this->mtl.materials.back().opticalDensity = Helper::read<float>(stream);
		}
		stream >> header;
	}
	return true;
}