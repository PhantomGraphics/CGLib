#include "PrimitiveMesh.h"

#include "../../Math/Core/glm.h"

#include <cmath>
#include <limits>
#include <utility>

namespace Phantom::Graphics {

namespace {
	constexpr double M_PI_D = 3.14159265358979323846;
}

MeshValidationResult validate(const MeshData& md) {
	MeshValidationResult result;
	const auto& idx = md.indices;
	const auto& vtx = md.vertices;
	if (idx.empty()) return result;

	result.totalTriangles = static_cast<int>(idx.size() / 3);
	const float eps = std::numeric_limits<float>::epsilon() * 10.0f;

	for (size_t i = 0; i + 2 < idx.size(); i += 3) {
		const uint32_t i0 = idx[i + 0];
		const uint32_t i1 = idx[i + 1];
		const uint32_t i2 = idx[i + 2];
		const int triIndex = static_cast<int>(i / 3);

		if (i0 >= vtx.size() || i1 >= vtx.size() || i2 >= vtx.size()) {
			++result.inconsistentTriangles;
			result.badIndices.push_back(triIndex);
			continue;
		}

		const glm::vec3 p0(vtx[i0].pos[0], vtx[i0].pos[1], vtx[i0].pos[2]);
		const glm::vec3 p1(vtx[i1].pos[0], vtx[i1].pos[1], vtx[i1].pos[2]);
		const glm::vec3 p2(vtx[i2].pos[0], vtx[i2].pos[1], vtx[i2].pos[2]);

		const glm::vec3 e1 = p1 - p0;
		const glm::vec3 e2 = p2 - p0;
		const glm::vec3 geom = glm::cross(e1, e2);
		const float area2 = glm::length(geom);

		if (area2 <= eps) {
			++result.degenerateTriangles;
			++result.inconsistentTriangles;
			result.badIndices.push_back(triIndex);
			continue;
		}

		const glm::vec3 geomN = glm::normalize(geom);
		const glm::vec3 d0(vtx[i0].nrm[0], vtx[i0].nrm[1], vtx[i0].nrm[2]);
		const glm::vec3 d1(vtx[i1].nrm[0], vtx[i1].nrm[1], vtx[i1].nrm[2]);
		const glm::vec3 d2(vtx[i2].nrm[0], vtx[i2].nrm[1], vtx[i2].nrm[2]);
		glm::vec3 declared = d0 + d1 + d2;

		if (glm::length(declared) <= eps) {
			++result.inconsistentTriangles;
			result.badIndices.push_back(triIndex);
			continue;
		}

		declared = glm::normalize(declared);
		const float dotValue = glm::dot(geomN, declared);
		const bool ok = dotValue > 0.0f;

		if (!ok) {
			++result.inconsistentTriangles;
			result.badIndices.push_back(triIndex);
		}
	}

	return result;
}

// ---------------------------------------------------------------------------
// Box
// ---------------------------------------------------------------------------
MeshData buildBox(float h) {
	// 6 faces ﾃ・4 vertices
	static const float faces[6][3][3] = {
		// +X
		{{ h,-h,-h},{ h,-h, h},{ h, h, h}},
		// -X
		{{-h,-h, h},{-h,-h,-h},{-h, h,-h}},
		// +Y
		{{-h, h,-h},{ h, h,-h},{ h, h, h}},
		// -Y
		{{-h,-h, h},{ h,-h, h},{ h,-h,-h}},
		// +Z
		{{-h,-h, h},{-h, h, h},{ h, h, h}},
		// -Z
		{{ h,-h,-h},{ h, h,-h},{-h, h,-h}},
	};
	static const float normals[6][3] = {
		{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
	};

	MeshData md;
	for (int f = 0; f < 6; ++f) {
		uint32_t base = static_cast<uint32_t>(md.vertices.size());
		// 4 corners of each face quad
		float v0[3] = { faces[f][0][0], faces[f][0][1], faces[f][0][2] };
		float v1[3] = { faces[f][1][0], faces[f][1][1], faces[f][1][2] };
		float v2[3] = { faces[f][2][0], faces[f][2][1], faces[f][2][2] };
		// 4th = v0 + (v2 - v1)
		float v3[3] = { v0[0] + v2[0] - v1[0],
						v0[1] + v2[1] - v1[1],
						v0[2] + v2[2] - v1[2] };
		float n[3] = { normals[f][0], normals[f][1], normals[f][2] };

		md.vertices.push_back({ {v0[0],v0[1],v0[2]}, {n[0],n[1],n[2]}, {0.f,0.f} });
		md.vertices.push_back({ {v1[0],v1[1],v1[2]}, {n[0],n[1],n[2]}, {1.f,0.f} });
		md.vertices.push_back({ {v2[0],v2[1],v2[2]}, {n[0],n[1],n[2]}, {1.f,1.f} });
		md.vertices.push_back({ {v3[0],v3[1],v3[2]}, {n[0],n[1],n[2]}, {0.f,1.f} });

		// Two triangles
		// Two triangles, counter-clockwise seen from outside (cross product = the declared outward normal). The face corner
		// tables above are listed the other way round, hence (0,2,1) / (0,3,2).
		md.indices.push_back(base+0); md.indices.push_back(base+2); md.indices.push_back(base+1);
		md.indices.push_back(base+0); md.indices.push_back(base+3); md.indices.push_back(base+2);
	}
	return md;
}

// ---------------------------------------------------------------------------
// Sphere (UV sphere)
// ---------------------------------------------------------------------------
MeshData buildSphere(int stacks, int slices, float r) {
	MeshData md;

	for (int i = 0; i <= stacks; ++i) {
		float phi = static_cast<float>(M_PI_D) * i / stacks;
		float v   = static_cast<float>(i) / stacks;
		for (int j = 0; j <= slices; ++j) {
			float theta = 2.f * static_cast<float>(M_PI_D) * j / slices;
			float x = std::sin(phi) * std::cos(theta);
			float y = std::cos(phi);
			float z = std::sin(phi) * std::sin(theta);
			float u = static_cast<float>(j) / slices;
			md.vertices.push_back({ {r*x, r*y, r*z}, {x, y, z}, {u, v} });
		}
	}

	for (int i = 0; i < stacks; ++i) {
		for (int j = 0; j < slices; ++j) {
			uint32_t a = i*(slices+1)+j;
			uint32_t b = a+1;
			uint32_t c = (i+1)*(slices+1)+j;
			uint32_t d = c+1;
			// Counter-clockwise seen from outside, like every other primitive (a,b,c)/(b,d,c); the (row=i, col=j) grid of the
			// plane and the cylinder side is the other way round, hence their (a,c,b)/(b,c,d).
			md.indices.push_back(a); md.indices.push_back(b); md.indices.push_back(c);
			md.indices.push_back(b); md.indices.push_back(d); md.indices.push_back(c);
		}
	}
	return md;
}

// ---------------------------------------------------------------------------
// Plane (XZ plane, facing +Y)
// ---------------------------------------------------------------------------
MeshData buildPlane(float h, int divs) {
	MeshData md;
	float step = (2.f * h) / divs;
	for (int i = 0; i <= divs; ++i) {
		float v = static_cast<float>(i) / divs;
		for (int j = 0; j <= divs; ++j) {
			float x = -h + j * step;
			float z = -h + i * step;
			float u = static_cast<float>(j) / divs;
			md.vertices.push_back({ {x, 0.f, z}, {0.f, 1.f, 0.f}, {u, v} });
		}
	}
	int w = divs + 1;
	for (int i = 0; i < divs; ++i) {
		for (int j = 0; j < divs; ++j) {
			uint32_t a = i*w+j, b = a+1, c = (i+1)*w+j, d = c+1;
			md.indices.push_back(a); md.indices.push_back(c); md.indices.push_back(b);
			md.indices.push_back(b); md.indices.push_back(c); md.indices.push_back(d);
		}
	}
	return md;
}

// ---------------------------------------------------------------------------
// Cylinder
// ---------------------------------------------------------------------------
MeshData buildCylinder(float radius, float height, int segments) {
	MeshData md;
	const float halfH = height * 0.5f;

	// Side: two rings (bottom i=0, top i=1), same (row=i, col=j) grid pattern as
	// buildSphere()/buildPlane() -- this pattern's (a,c,b)/(b,c,d) winding happens to
	// already match a radial declared normal (verified analytically), so no flip needed here.
	for (int i = 0; i <= 1; ++i) {
		float y = -halfH + i * height;
		float v = static_cast<float>(i);
		for (int j = 0; j <= segments; ++j) {
			float theta = 2.f * static_cast<float>(M_PI_D) * j / segments;
			float cx = std::cos(theta), cz = std::sin(theta);
			float u = static_cast<float>(j) / segments;
			md.vertices.push_back({ {radius * cx, y, radius * cz}, {cx, 0.f, cz}, {u, v} });
		}
	}
	int w = segments + 1;
	for (int j = 0; j < segments; ++j) {
		uint32_t a = j, b = a + 1, c = w + j, d = c + 1;
		md.indices.push_back(a); md.indices.push_back(c); md.indices.push_back(b);
		md.indices.push_back(b); md.indices.push_back(c); md.indices.push_back(d);
	}

	// Top cap (fan, normal +Y). Naive (center,j,j+1) fan winding evaluates to a -Y cross
	// product for increasing theta, so the last two indices are swapped to match +Y.
	{
		uint32_t centerIdx = static_cast<uint32_t>(md.vertices.size());
		md.vertices.push_back({ {0.f, halfH, 0.f}, {0.f, 1.f, 0.f}, {0.5f, 0.5f} });
		uint32_t ringStart = centerIdx + 1;
		for (int j = 0; j <= segments; ++j) {
			float theta = 2.f * static_cast<float>(M_PI_D) * j / segments;
			float cx = std::cos(theta), cz = std::sin(theta);
			md.vertices.push_back({ {radius * cx, halfH, radius * cz}, {0.f, 1.f, 0.f},
			                         {0.5f + 0.5f * cx, 0.5f + 0.5f * cz} });
		}
		for (int j = 0; j < segments; ++j) {
			md.indices.push_back(centerIdx);
			md.indices.push_back(ringStart + j + 1);
			md.indices.push_back(ringStart + j);
		}
	}

	// Bottom cap (fan, normal -Y). Naive (center,j,j+1) fan winding already matches -Y.
	{
		uint32_t centerIdx = static_cast<uint32_t>(md.vertices.size());
		md.vertices.push_back({ {0.f, -halfH, 0.f}, {0.f, -1.f, 0.f}, {0.5f, 0.5f} });
		uint32_t ringStart = centerIdx + 1;
		for (int j = 0; j <= segments; ++j) {
			float theta = 2.f * static_cast<float>(M_PI_D) * j / segments;
			float cx = std::cos(theta), cz = std::sin(theta);
			md.vertices.push_back({ {radius * cx, -halfH, radius * cz}, {0.f, -1.f, 0.f},
			                         {0.5f + 0.5f * cx, 0.5f + 0.5f * cz} });
		}
		for (int j = 0; j < segments; ++j) {
			md.indices.push_back(centerIdx);
			md.indices.push_back(ringStart + j);
			md.indices.push_back(ringStart + j + 1);
		}
	}

	return md;
}

// ---------------------------------------------------------------------------
// Cone
// ---------------------------------------------------------------------------
MeshData buildCone(float radius, float height, int segments) {
	MeshData md;
	const float halfH = height * 0.5f;
	const float slant = std::sqrt(radius * radius + height * height);
	const float nY  = radius / slant;
	const float nXZ = height / slant;

	// Side: apex duplicated per column (like buildSphere()'s pole ring) so each triangle
	// gets its own slant normal, base ring below. One fan triangle per segment (apex, base[j],
	// base[j+1]) -- naive winding evaluates opposite of the declared slant normal, so the last
	// two indices are swapped below.
	for (int j = 0; j <= segments; ++j) {
		float theta = 2.f * static_cast<float>(M_PI_D) * j / segments;
		float cx = std::cos(theta), cz = std::sin(theta);
		float u = static_cast<float>(j) / segments;
		md.vertices.push_back({ {0.f, halfH, 0.f}, {cx * nXZ, nY, cz * nXZ}, {u, 1.f} });
		md.vertices.push_back({ {radius * cx, -halfH, radius * cz}, {cx * nXZ, nY, cz * nXZ}, {u, 0.f} });
	}
	for (int j = 0; j < segments; ++j) {
		uint32_t apexIdx  = static_cast<uint32_t>(2 * j);
		uint32_t baseIdxA = apexIdx + 1;
		uint32_t baseIdxB = static_cast<uint32_t>(2 * (j + 1) + 1);
		md.indices.push_back(apexIdx);
		md.indices.push_back(baseIdxB);
		md.indices.push_back(baseIdxA);
	}

	// Base cap (fan, normal -Y). Same as buildCylinder()'s bottom cap.
	{
		uint32_t centerIdx = static_cast<uint32_t>(md.vertices.size());
		md.vertices.push_back({ {0.f, -halfH, 0.f}, {0.f, -1.f, 0.f}, {0.5f, 0.5f} });
		uint32_t ringStart = centerIdx + 1;
		for (int j = 0; j <= segments; ++j) {
			float theta = 2.f * static_cast<float>(M_PI_D) * j / segments;
			float cx = std::cos(theta), cz = std::sin(theta);
			md.vertices.push_back({ {radius * cx, -halfH, radius * cz}, {0.f, -1.f, 0.f},
			                         {0.5f + 0.5f * cx, 0.5f + 0.5f * cz} });
		}
		for (int j = 0; j < segments; ++j) {
			md.indices.push_back(centerIdx);
			md.indices.push_back(ringStart + j);
			md.indices.push_back(ringStart + j + 1);
		}
	}

	return md;
}

// ---------------------------------------------------------------------------
// Torus
// ---------------------------------------------------------------------------
MeshData buildTorus(float majorRadius, float minorRadius,
                                       int majorSegments, int minorSegments) {
	MeshData md;

	for (int i = 0; i <= majorSegments; ++i) {
		float u  = 2.f * static_cast<float>(M_PI_D) * i / majorSegments;
		float cu = std::cos(u), su = std::sin(u);
		float uu = static_cast<float>(i) / majorSegments;
		for (int j = 0; j <= minorSegments; ++j) {
			float v  = 2.f * static_cast<float>(M_PI_D) * j / minorSegments;
			float cv = std::cos(v), sv = std::sin(v);
			float ringR = majorRadius + minorRadius * cv;
			float x = ringR * cu;
			float y = minorRadius * sv;
			float z = ringR * su;
			float vv = static_cast<float>(j) / minorSegments;
			md.vertices.push_back({ {x, y, z}, {cv * cu, sv, cv * su}, {uu, vv} });
		}
	}

	// Same (row=i major, col=j minor) grid as buildSphere()/buildPlane(), but this
	// parametrization's (a,c,b)/(b,c,d) winding evaluates opposite of the declared normal
	// (verified analytically at u=v=0), so it's flipped here ((a,b,c)/(b,d,c)).
	int w = minorSegments + 1;
	for (int i = 0; i < majorSegments; ++i) {
		for (int j = 0; j < minorSegments; ++j) {
			uint32_t a = i * w + j, b = a + 1, c = (i + 1) * w + j, d = c + 1;
			md.indices.push_back(a); md.indices.push_back(b); md.indices.push_back(c);
			md.indices.push_back(b); md.indices.push_back(d); md.indices.push_back(c);
		}
	}
	return md;
}

// ---------------------------------------------------------------------------
// Mirror
// ---------------------------------------------------------------------------
MeshData mirror(const MeshData& src, int axis) {
	MeshData md = src;
	for (auto& v : md.vertices) {
		v.pos[axis] = -v.pos[axis];
		v.nrm[axis] = -v.nrm[axis];
		v.tangent[axis] = -v.tangent[axis];
		v.tangent[3] = -v.tangent[3];
	}
	// Flip triangle winding so the mirrored mesh isn't back-face culled
	// (see CGApp/PhantomStudio/CLAUDE.md, winding convention).
	for (size_t i = 0; i + 2 < md.indices.size(); i += 3)
		std::swap(md.indices[i + 1], md.indices[i + 2]);
	return md;
}

} // namespace Phantom::Graphics
