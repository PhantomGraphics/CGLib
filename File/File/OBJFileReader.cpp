#include "OBJFileReader.h"

#include <fstream>
#include <cassert>
#include <vector>
#include <charconv>
#include <cstdio>

#include "CGLib/Math/Vector3d.h"
//#include "../Scene/PolygonMesh.h"

#include "OBJSyntaxParser.h"
#include "MTLFileReader.h"

#include "Helper.h"

#include <string>
#include <sstream>

using namespace Phantom::Math;
using namespace Phantom::Graphics;
using namespace Phantom::File;

namespace {
	bool parseFloatToken(const std::string& s, float& out)
	{
		const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
		return ec == std::errc{};
	}
}

bool OBJFileReader::read(const std::filesystem::path& filePath)
{
	std::ifstream stream;
	stream.open(filePath);

	if (!stream.is_open()) {
		return false;
	}

	if (!read(stream)) {
		return false;
	}

	// mtllib はカレントディレクトリではなく OBJ ファイルと同じディレクトリからの相対パスとして解決する。
	const auto baseDir = filePath.parent_path();
	for (const auto& mtllibName : obj.mtllibs) {
		MTLFileReader mtlReader;
		if (mtlReader.read(baseDir / mtllibName)) {
			const auto& loaded = mtlReader.getMTL().materials;
			obj.mtl.materials.insert(obj.mtl.materials.end(), loaded.begin(), loaded.end());
		}
		else {
			fprintf(stderr, "OBJFileReader: failed to load mtllib '%s'\n", mtllibName.c_str());
		}
	}

	return true;
}

bool OBJFileReader::read(std::istream& stream)
{
	obj = OBJFile();

	std::string currentMtllibName;
	auto currentGroup = OBJGroup();

	while (!stream.eof()) {
		std::string line;
		std::getline(stream, line);
		const auto strs = Helper::split(line, ' ');
		if (strs.empty()) {
			continue;
		}

		const auto header = strs.front();
		if (header == "#") {
		}
		else if (header == "v") {
			if (const auto v = readVertices(strs)) {
				obj.positions.push_back(*v);
			}
			else {
				fprintf(stderr, "OBJFileReader: skipping malformed 'v' line: %s\n", line.c_str());
			}
		}
		else if (header == "vt") {
			if (const auto v = readVector2d(strs)) {
				obj.texCoords.push_back(*v);
			}
			else {
				fprintf(stderr, "OBJFileReader: skipping malformed 'vt' line: %s\n", line.c_str());
			}
		}
		else if (header == "vn" || header == "-vn") {
			if (const auto v = readVector3d(strs)) {
				obj.normals.push_back(*v);
			}
			else {
				fprintf(stderr, "OBJFileReader: skipping malformed 'vn' line: %s\n", line.c_str());
			}
		}
		else if (header == "mtllib") {
			currentMtllibName = strs[1];
			obj.mtllibs.push_back(currentMtllibName);
		}
		else if (header == "usemtl") {
			currentGroup.usemtl = strs[1];
		}
		else if (header == "f") {
			if (const auto face = OBJSyntaxParser::parseFaceLine(line)) {
				currentGroup.faces.push_back(*face);
			}
			else {
				fprintf(stderr, "OBJFileReader: skipping malformed 'f' line: %s\n", line.c_str());
			}
		}
		else if (header == "g") {
			// 先に現在グループを確定
			if (!currentGroup.faces.empty() || !currentGroup.name.empty() || !currentGroup.usemtl.empty()) {
				obj.groups.push_back(currentGroup);
			}

			// 新しいグループを開始
			currentGroup = OBJGroup();
			if (strs.size() >= 2) {
				currentGroup.name = strs[1];
			}
		}
	}

	// 最終グループを確定
	if (!currentGroup.faces.empty() || !currentGroup.name.empty() || !currentGroup.usemtl.empty()) {
		obj.groups.push_back(currentGroup);
	}

	return true;
}


std::optional<Vector3df> OBJFileReader::readVertices(const std::vector<std::string>& strs)
{
	if (strs.size() < 4) {
		return std::nullopt;
	}
	float x, y, z;
	if (!parseFloatToken(strs[1], x) || !parseFloatToken(strs[2], y) || !parseFloatToken(strs[3], z)) {
		return std::nullopt;
	}
	return Vector3df(x, y, z);
}

std::optional<Vector3df> OBJFileReader::readVector3d(const std::vector<std::string>& strs)
{
	if (strs.size() < 3) {
		return std::nullopt;
	}
	float u, v;
	if (!parseFloatToken(strs[1], u) || !parseFloatToken(strs[2], v)) {
		return std::nullopt;
	}
	if (strs.size() == 4) {
		float w;
		if (!parseFloatToken(strs[3], w)) {
			return std::nullopt;
		}
		return Vector3df(u, v, w);
	}
	else {
		return Vector3df(u, v, 0.0f);
	}
}

std::optional<Vector2df> OBJFileReader::readVector2d(const std::vector<std::string>& strs)
{
	if (strs.size() < 3) {
		return std::nullopt;
	}
	float u, v;
	if (!parseFloatToken(strs[1], u) || !parseFloatToken(strs[2], v)) {
		return std::nullopt;
	}
	return Vector2df(u, v);
}