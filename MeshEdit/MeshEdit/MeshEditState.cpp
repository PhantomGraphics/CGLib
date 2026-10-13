#include "MeshEditState.h"

#include <algorithm>
#include <cctype>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <set>
#include <sstream>

namespace Phantom::MeshEdit {

namespace {

std::string lowered(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return s;
}

bool parseInt(const std::string& s, int& out) {
	char* end = nullptr;
	const long v = std::strtol(s.c_str(), &end, 10);
	if (end == s.c_str() || *end != '\0' || v < INT_MIN || v > INT_MAX) return false;
	out = static_cast<int>(v);
	return true;
}

}  // namespace

bool selectModeFromName(const std::string& name, SelectMode& out) {
	const std::string n = lowered(name);
	if (n == "replace") out = SelectMode::Replace;
	else if (n == "add") out = SelectMode::Add;
	else if (n == "toggle") out = SelectMode::Toggle;
	else return false;
	return true;
}

void MeshEditState::applySelection(ElementKind newKind, const std::vector<uint32_t>& requested, SelectMode mode, bool clear,
                                   bool explicitIds) {
	if (newKind != kind) { ids.clear(); kind = newKind; }
	std::set<uint32_t> result(ids.begin(), ids.end());
	if (clear) result.clear();
	else if (mode == SelectMode::Replace) { if (explicitIds) result = std::set<uint32_t>(requested.begin(), requested.end()); }
	else if (mode == SelectMode::Add) result.insert(requested.begin(), requested.end());
	else for (uint32_t id : requested) { if (!result.erase(id)) result.insert(id); }
	ids.assign(result.begin(), result.end());
}

bool parseElementIds(const std::string& text, const EditableMesh& mesh, ElementKind kind, ElementIdRequest& out, MeshArgError& error) {
	out = ElementIdRequest{};
	const std::string lower = lowered(text);
	if (lower == "none" || text.empty()) {
		out.clear = true;
		return true;
	}
	if (lower == "all") {
		out.ids = allElementIds(mesh, kind);
	} else {
		std::stringstream stream(text);
		std::string part;
		while (std::getline(stream, part, ',')) {
			int a = 0, b = 0;
			const size_t dash = part.find('-', 1);
			if (dash != std::string::npos) {
				if (!parseInt(part.substr(0, dash), a) || !parseInt(part.substr(dash + 1), b) || a < 0 || b < a || b - a > 10000000) {
					error = { "invalid_argument", "invalid id range '" + part + "'" };
					return false;
				}
				for (int v = a; v <= b; ++v) out.ids.push_back(static_cast<uint32_t>(v));
			} else {
				if (!parseInt(part, a) || a < 0) {
					error = { "invalid_argument", "invalid element id '" + part + "'" };
					return false;
				}
				out.ids.push_back(static_cast<uint32_t>(a));
			}
		}
	}
	for (uint32_t id : out.ids)
		if (!hasElement(mesh, kind, id)) {
			error = { "not_found", std::string(elementKindName(kind)) + " " + std::to_string(id) + " does not exist" };
			return false;
		}
	return true;
}

bool parseNumberList(const std::string& text, size_t count, std::vector<double>& out, MeshArgError& error) {
	out.clear();
	std::stringstream stream(text);
	std::string part;
	while (std::getline(stream, part, ',')) {
		char* end = nullptr;
		const double v = std::strtod(part.c_str(), &end);
		if (part.empty() || *end != 0 || !std::isfinite(v)) {
			error = { "invalid_argument", "invalid pixel coordinate '" + part + "'" };
			return false;
		}
		out.push_back(v);
	}
	if (out.size() != count) {
		error = { "invalid_argument", "expected " + std::to_string(count) + " comma-separated numbers" };
		return false;
	}
	return true;
}

bool parseMirrorAxes(const std::string& text, std::array<bool, 3>& out, MeshArgError& error) {
	out = { false, false, false };
	const std::string axes = text.empty() ? std::string("x") : lowered(text);
	for (char c : axes) {
		if (c == ',' || c == ' ') continue;
		if (c < 'x' || c > 'z') {
			error = { "invalid_argument", "axis must be x, y, z or a combination such as xy" };
			return false;
		}
		out[c - 'x'] = true;
	}
	return true;
}

}  // namespace Phantom::MeshEdit
