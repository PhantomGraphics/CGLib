#include "MeshJoin.h"

#include <algorithm>
#include <map>

namespace Phantom::MeshEdit {

bool joinMeshes(const EditableMesh& base, const std::vector<const EditableMesh*>& parts, EditableMesh& out, std::string* error) {
	auto fail = [&](const std::string& why) { if (error) *error = why; return false; };
	if (!base.validate(error)) return false;
	for (const EditableMesh* part : parts) {
		if (!part) return fail("a part to join is missing");
		if (!part->validate(error)) return false;
	}

	EditableMesh joined = base;
	for (const EditableMesh* part : parts) {
		std::map<uint32_t, uint32_t> vertexOf;
		for (const EditableVertex& v : part->vertices) vertexOf[v.id] = joined.addVertex(v.pos[0], v.pos[1], v.pos[2]);
		for (const EditableFace& f : part->faces) {
			std::vector<EditableCorner> loop = f.loop;
			for (EditableCorner& c : loop) c.vertex = vertexOf.at(c.vertex);
			joined.addFace(loop);
		}
		for (const auto& seam : part->seams) {
			uint32_t a = vertexOf.at(seam.first), b = vertexOf.at(seam.second);
			if (a > b) std::swap(a, b);
			joined.seams.emplace_back(a, b);
		}
	}
	joined.rebuildEdges();
	std::sort(joined.seams.begin(), joined.seams.end());
	joined.seams.erase(std::unique(joined.seams.begin(), joined.seams.end()), joined.seams.end());
	joined.revision = nextEditableRevision();
	if (!joined.validate(error)) return false;
	out = std::move(joined);
	if (error) error->clear();
	return true;
}

}
