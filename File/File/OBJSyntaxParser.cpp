#include "OBJSyntaxParser.h"
#include "Helper.h"

#include <charconv>

using namespace Phantom::File;

namespace {
	bool parseIntToken(const std::string& s, int& out)
	{
		const auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
		return ec == std::errc{};
	}
}

std::optional<OBJFace> OBJSyntaxParser::parseFace(std::vector< std::string >& strs)
{
	OBJFace face;
	for (size_t i = 1; i < strs.size(); ++i) {
		auto str = strs[i];
		if (str.empty()) {
			continue;
		}
		std::string::size_type pos(str.find("//"));
		if (pos != std::string::npos) {
			str.replace(pos, 2, "/ /");
		}

		std::vector<std::string> splitted = Helper::split(str, '/');
		if (splitted.empty()) {
			return std::nullopt;
		}

		int positionIndex;
		if (!parseIntToken(splitted[0], positionIndex)) {
			return std::nullopt;
		}
		face.positionIndices.push_back(positionIndex);

		if (splitted.size() >= 2 && splitted[1] != " ") {
			int texIndex;
			if (!parseIntToken(splitted[1], texIndex)) {
				return std::nullopt;
			}
			face.texCoordIndices.push_back(texIndex);
		}
		else {
			face.texCoordIndices.push_back(-1);
		}

		if (splitted.size() >= 3) {
			int normalIndex;
			if (!parseIntToken(splitted[2], normalIndex)) {
				return std::nullopt;
			}
			face.normalIndices.push_back(normalIndex);
		}
		else {
			face.normalIndices.push_back(-1);
		}
	}
	return face;
}


std::optional<OBJFace> OBJSyntaxParser::parseFaceLine(const std::string& line)
{
	std::vector< std::string > strs = Helper::split(line, ' ');
	return parseFace(strs);
}