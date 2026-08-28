#include "PLYFileReader.h"

//#include "PCDFileReader.h"
#include <fstream>
#include <sstream>
#include <vector>
#include <cassert>
#include <charconv>
#include <cstdio>

using namespace Phantom::Math;
using namespace Phantom::File;

namespace {
	std::vector<std::string> split(const std::string& input, char delimiter)
	{
		std::istringstream stream(input);

		std::string field;
		std::vector<std::string> result;
		while (std::getline(stream, field, delimiter)) {
			result.push_back(field);
		}
		return result;
	}

	bool parseUInt(const std::string& s, unsigned int& out)
	{
		const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
		return ec == std::errc{};
	}

	bool parseInt(const std::string& s, int& out)
	{
		const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
		return ec == std::errc{};
	}

	bool parseFloatVal(const std::string& s, float& out)
	{
		const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
		return ec == std::errc{};
	}
}

bool PLYFileReader::read(const std::filesystem::path& filename)
{
	std::ifstream stream(filename, std::ios::in | std::ios::binary);
	if (!stream.is_open()) {
		return false;
	}
	return read(stream);
}

bool PLYFileReader::read(std::istream& stream)
{
	unsigned int count = 0;
	unsigned int faceCount = 0;

	// 追加メンバー
	std::string faceListCountType = "uchar";
	std::string faceListIndexType = "int";

	std::string str;
	bool isAscii = false;
	while (std::getline(stream, str)) {
		if (str.empty()) {
			continue;
		}
		const auto& splitted = ::split(str, ' ');
		//assert(splitted.size() >= 2);
		if (splitted[0] == "comment") {
			continue;
		}
		else if (splitted[0] == "format") {
			if (splitted[1] == "ascii") {
				isAscii = true;
			}
		}
		else if (splitted[0] == "element") {
			if (splitted.size() < 3) {
				fprintf(stderr, "PLYFileReader: malformed 'element' header line, aborting: %s\n", str.c_str());
				return false;
			}
			if (splitted[1] == "vertex") {
				if (!parseUInt(splitted[2], count)) {
					fprintf(stderr, "PLYFileReader: invalid vertex count, aborting: %s\n", str.c_str());
					return false;
				}
			}
			else if (splitted[1] == "face") {
				if (!parseUInt(splitted[2], faceCount)) {
					fprintf(stderr, "PLYFileReader: invalid face count, aborting: %s\n", str.c_str());
					return false;
				}
			}
		}
		else if (splitted[0] == "property") {
			if (splitted[1] == "list") {
//				auto typeName = splitted[1];
//				auto name = splitted[2];
				faceListCountType = splitted[2];
				faceListIndexType = splitted[3];
			}
			else {
				auto typeName = splitted[1];
				auto name = splitted[2];
				PLYProperty prop(name, PLYProperty::toType(typeName));
				ply.properties.push_back(prop);
			}
		}
		else if (splitted[0].starts_with("end_header")) {
			break;
		}
	}

	if (isAscii) {
		return readAsciiData(stream, count, faceCount);
	}
	else {
		return readBinaryData(stream, count, faceCount);
	}

	return false;
}

namespace {
	template<typename T>
	T read_binary_as(std::istream& is)
	{
		T val;
		is.read(reinterpret_cast<char*>(std::addressof(val)), sizeof(T));
		return val;
	}
}

bool PLYFileReader::readAsciiData(std::istream& stream, const unsigned int count, const unsigned int faceCount)
{
	std::string str;
	for (unsigned int i = 0; i < count; ++i) {
		std::getline(stream, str);
		const auto& splitted = ::split(str, ' ');
		PLYPoint point;
		for (size_t j = 0; j < ply.properties.size(); ++j) {
			auto type = (ply.properties[j].type);
			if (type == PLYType::FLOAT) {
				if (j >= splitted.size()) {
					fprintf(stderr, "PLYFileReader: vertex line has too few fields, aborting: %s\n", str.c_str());
					return false;
				}
				float value;
				if (!parseFloatVal(splitted[j], value)) {
					fprintf(stderr, "PLYFileReader: invalid vertex value '%s', aborting\n", splitted[j].c_str());
					return false;
				}
				point.values.push_back(value);
			}
		}
		ply.vertices.push_back(point);
	}
	for (unsigned int i = 0; i < faceCount; ++i) {
		std::getline(stream, str);
		const auto& splitted = ::split(str, ' ');
		if (splitted.empty()) {
			fprintf(stderr, "PLYFileReader: empty face line, aborting\n");
			return false;
		}
		int vertexNum;
		if (!parseInt(splitted[0], vertexNum)) {
			fprintf(stderr, "PLYFileReader: invalid face vertex count '%s', aborting\n", splitted[0].c_str());
			return false;
		}
		std::vector<unsigned int> indices;
		for (int j = 1; j <= vertexNum; ++j) {
			if (static_cast<size_t>(j) >= splitted.size()) {
				fprintf(stderr, "PLYFileReader: face line has too few indices, aborting: %s\n", str.c_str());
				return false;
			}
			unsigned int index;
			if (!parseUInt(splitted[j], index)) {
				fprintf(stderr, "PLYFileReader: invalid face index '%s', aborting\n", splitted[j].c_str());
				return false;
			}
			indices.push_back(index);
		}
		ply.faces.push_back(indices);
	}
	return true;
}

bool PLYFileReader::readBinaryData(std::istream& stream, const unsigned int count, const unsigned int faceCount)
{
	for (unsigned int i = 0; i < count; ++i) {
		PLYPoint point;
		for (int j = 0; j < ply.properties.size(); ++j) {
			auto type = (ply.properties[j].type);
			if (type == PLYType::FLOAT) {
				point.values.push_back(read_binary_as<float>(stream));
			}
		}
		ply.vertices.push_back(point);
	}
	for (unsigned int i = 0; i < faceCount; ++i) {
		uint8_t vertexNum = read_binary_as<uint8_t>(stream); // 通常はuchar
		std::vector<unsigned int> indices;
		for (int j = 0; j < vertexNum; ++j) {
			indices.push_back(read_binary_as<unsigned int>(stream)); // int型（仕様による）
		}
		ply.faces.push_back(indices);
	}

	return true;
}