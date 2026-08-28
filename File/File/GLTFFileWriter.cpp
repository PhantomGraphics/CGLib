#include "GLTFFileWriter.h"

#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>

using namespace Phantom::File;

// ============================================================
// Internal utilities
// ============================================================
namespace {

   // Base64 encode
	static const char kBase64Table[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

	std::string base64Encode(const uint8_t* data, size_t size)
	{
		std::string result;
		result.reserve(((size + 2) / 3) * 4);
		for (size_t i = 0; i < size; i += 3) {
			const uint8_t b0 = data[i];
			const uint8_t b1 = (i + 1 < size) ? data[i + 1] : 0u;
			const uint8_t b2 = (i + 2 < size) ? data[i + 2] : 0u;
			result += kBase64Table[(b0 >> 2) & 0x3F];
			result += kBase64Table[((b0 << 4) | (b1 >> 4)) & 0x3F];
			result += (i + 1 < size) ? kBase64Table[((b1 << 2) | (b2 >> 6)) & 0x3F] : '=';
			result += (i + 2 < size) ? kBase64Table[b2 & 0x3F] : '=';
		}
		return result;
	}

    // Pad binary buffer to 4-byte alignment
	void pad4(std::vector<uint8_t>& buf)
	{
		while (buf.size() % 4 != 0) {
			buf.push_back(0u);
		}
	}

  // JSON helpers
	std::string quoted(const std::string& s) { return "\"" + s + "\""; }

	std::string floatArray(const float* v, size_t n)
	{
		std::ostringstream ss;
		ss << "[";
		for (size_t i = 0; i < n; ++i) {
			if (i > 0) ss << ", ";
			ss << v[i];
		}
		ss << "]";
		return ss.str();
	}

 // Accessor / BufferView records
	struct BufferViewRec {
		size_t byteOffset;
		size_t byteLength;
		int    target; // 34962 = ARRAY_BUFFER, 34963 = ELEMENT_ARRAY_BUFFER
	};

	struct AccessorRec {
		int         bufferViewIndex;
		int         componentType; // 5126=FLOAT, 5125=UNSIGNED_INT
		size_t      count;
		std::string type;          // "SCALAR","VEC2","VEC3","VEC4"
		bool        hasMinMax   = false;
		float       minVal[3]   = {};
		float       maxVal[3]   = {};
		size_t      minMaxCount = 0;
	};

  // Accessor indices grouped by primitive
	struct PrimAccessors {
		int pos  = -1;
		int norm = -1;
		int tex  = -1;
		int tan  = -1;
		int idx  = -1;
	};

} // anonymous namespace

// ============================================================
// GLTFFileWriter implementation
// ============================================================

bool GLTFFileWriter::write(const std::filesystem::path& filename, const GLTFFile& gltf)
{
	std::ofstream stream(filename);
	if (!stream.is_open()) {
		return false;
	}
	return write(stream, gltf);
}

bool GLTFFileWriter::write(std::ostream& stream, const GLTFFile& gltf)
{
   // Build binary buffer / accessors / bufferViews
	std::vector<uint8_t>     binData;
	std::vector<BufferViewRec> bufferViews;
	std::vector<AccessorRec>   accessors;

  // Accessor index table per mesh and primitive
	std::vector<std::vector<PrimAccessors>> meshPrimAcc(gltf.meshes.size());

 // Add binary data and return the bufferView index
	auto addBufferView = [&](const void* data, size_t bytes, int target) -> int {
		pad4(binData);
		const size_t offset = binData.size();
		const auto* src = reinterpret_cast<const uint8_t*>(data);
		binData.insert(binData.end(), src, src + bytes);
		BufferViewRec bv;
		bv.byteOffset = offset;
		bv.byteLength = bytes;
		bv.target     = target;
		bufferViews.push_back(bv);
		return static_cast<int>(bufferViews.size()) - 1;
	};

	for (size_t mi = 0; mi < gltf.meshes.size(); ++mi) {
		const GLTFMesh& mesh = gltf.meshes[mi];
		meshPrimAcc[mi].resize(mesh.primitives.size());

		for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
			const GLTFPrimitive& prim = mesh.primitives[pi];
			PrimAccessors& pa = meshPrimAcc[mi][pi];

           // POSITION (VEC3 FLOAT)
			if (!prim.positions.empty()) {
				const int bvIdx = addBufferView(
					prim.positions.data(),
					prim.positions.size() * sizeof(Math::Vector3df),
					34962);

				float minV[3] = { prim.positions[0].x, prim.positions[0].y, prim.positions[0].z };
				float maxV[3] = { prim.positions[0].x, prim.positions[0].y, prim.positions[0].z };
				for (const auto& v : prim.positions) {
					minV[0] = std::min(minV[0], v.x); maxV[0] = std::max(maxV[0], v.x);
					minV[1] = std::min(minV[1], v.y); maxV[1] = std::max(maxV[1], v.y);
					minV[2] = std::min(minV[2], v.z); maxV[2] = std::max(maxV[2], v.z);
				}

				AccessorRec acc;
				acc.bufferViewIndex = bvIdx;
				acc.componentType   = 5126; // FLOAT
				acc.count           = prim.positions.size();
				acc.type            = "VEC3";
				acc.hasMinMax       = true;
				std::copy(minV, minV + 3, acc.minVal);
				std::copy(maxV, maxV + 3, acc.maxVal);
				acc.minMaxCount     = 3;
				pa.pos = static_cast<int>(accessors.size());
				accessors.push_back(acc);
			}

			// NORMAL (VEC3 FLOAT)
			if (!prim.normals.empty()) {
				const int bvIdx = addBufferView(
					prim.normals.data(),
					prim.normals.size() * sizeof(Math::Vector3df),
					34962);
				AccessorRec acc;
				acc.bufferViewIndex = bvIdx;
				acc.componentType   = 5126;
				acc.count           = prim.normals.size();
				acc.type            = "VEC3";
				pa.norm = static_cast<int>(accessors.size());
				accessors.push_back(acc);
			}

			// TEXCOORD_0 (VEC2 FLOAT)
			if (!prim.texCoords.empty()) {
				const int bvIdx = addBufferView(
					prim.texCoords.data(),
					prim.texCoords.size() * sizeof(Math::Vector2df),
					34962);
				AccessorRec acc;
				acc.bufferViewIndex = bvIdx;
				acc.componentType   = 5126;
				acc.count           = prim.texCoords.size();
				acc.type            = "VEC2";
				pa.tex = static_cast<int>(accessors.size());
				accessors.push_back(acc);
			}

			// TANGENT (VEC4 FLOAT)
			if (!prim.tangents.empty()) {
				const int bvIdx = addBufferView(
					prim.tangents.data(),
					prim.tangents.size() * sizeof(Math::Vector4df),
					34962);
				AccessorRec acc;
				acc.bufferViewIndex = bvIdx;
				acc.componentType   = 5126;
				acc.count           = prim.tangents.size();
				acc.type            = "VEC4";
				pa.tan = static_cast<int>(accessors.size());
				accessors.push_back(acc);
			}

			// INDICES (SCALAR UNSIGNED_INT)
			if (!prim.indices.empty()) {
				const int bvIdx = addBufferView(
					prim.indices.data(),
					prim.indices.size() * sizeof(unsigned int),
					34963);
				AccessorRec acc;
				acc.bufferViewIndex = bvIdx;
				acc.componentType   = 5125; // UNSIGNED_INT
				acc.count           = prim.indices.size();
				acc.type            = "SCALAR";
				pa.idx = static_cast<int>(accessors.size());
				accessors.push_back(acc);
			}
		}
	}

    // Build JSON sections and join with commas
	std::vector<std::pair<std::string, std::string>> sections;

	// asset
	sections.push_back({ "asset", R"({"version": "2.0", "generator": "Crystal"})" });

	// scene
	if (!gltf.scenes.empty()) {
		sections.push_back({ "scene", std::to_string(gltf.defaultScene) });
	}

	// scenes
	if (!gltf.scenes.empty()) {
		std::ostringstream ss;
		ss << "[\n";
		for (size_t si = 0; si < gltf.scenes.size(); ++si) {
			const GLTFScene& scene = gltf.scenes[si];
			ss << "    {";
			if (!scene.name.empty()) ss << "\"name\": " << quoted(scene.name) << ", ";
			ss << "\"nodes\": [";
			for (size_t ni = 0; ni < scene.nodes.size(); ++ni) {
				if (ni > 0) ss << ", ";
				ss << scene.nodes[ni];
			}
			ss << "]}";
			if (si + 1 < gltf.scenes.size()) ss << ",";
			ss << "\n";
		}
		ss << "  ]";
		sections.push_back({ "scenes", ss.str() });
	}

	// nodes
	if (!gltf.nodes.empty()) {
		std::ostringstream ss;
		ss << "[\n";
		for (size_t ni = 0; ni < gltf.nodes.size(); ++ni) {
			const GLTFNode& node = gltf.nodes[ni];
			ss << "    {";
			bool first = true;
			auto sep = [&]() { if (!first) ss << ", "; first = false; };

			if (!node.name.empty()) { sep(); ss << "\"name\": " << quoted(node.name); }
			if (node.meshIndex >= 0) { sep(); ss << "\"mesh\": " << node.meshIndex; }

			const bool hasT = (node.translation[0] != 0.0f || node.translation[1] != 0.0f || node.translation[2] != 0.0f);
			if (hasT) { sep(); ss << "\"translation\": " << floatArray(node.translation.data(), 3); }

			const bool hasR = (node.rotation[0] != 0.0f || node.rotation[1] != 0.0f || node.rotation[2] != 0.0f || node.rotation[3] != 1.0f);
			if (hasR) { sep(); ss << "\"rotation\": " << floatArray(node.rotation.data(), 4); }

			const bool hasS = (node.scale[0] != 1.0f || node.scale[1] != 1.0f || node.scale[2] != 1.0f);
			if (hasS) { sep(); ss << "\"scale\": " << floatArray(node.scale.data(), 3); }

			if (!node.children.empty()) {
				sep();
				ss << "\"children\": [";
				for (size_t ci = 0; ci < node.children.size(); ++ci) {
					if (ci > 0) ss << ", ";
					ss << node.children[ci];
				}
				ss << "]";
			}
			ss << "}";
			if (ni + 1 < gltf.nodes.size()) ss << ",";
			ss << "\n";
		}
		ss << "  ]";
		sections.push_back({ "nodes", ss.str() });
	}

	// meshes
	if (!gltf.meshes.empty()) {
		std::ostringstream ss;
		ss << "[\n";
		for (size_t mi = 0; mi < gltf.meshes.size(); ++mi) {
			const GLTFMesh& mesh = gltf.meshes[mi];
			ss << "    {";
			if (!mesh.name.empty()) ss << "\"name\": " << quoted(mesh.name) << ", ";
			ss << "\"primitives\": [\n";
			for (size_t pi = 0; pi < mesh.primitives.size(); ++pi) {
				const GLTFPrimitive& prim  = mesh.primitives[pi];
				const PrimAccessors&  pa   = meshPrimAcc[mi][pi];
				ss << "      {\"attributes\": {";
				bool firstAttr = true;
				auto attrSep = [&]() { if (!firstAttr) ss << ", "; firstAttr = false; };
				if (pa.pos  >= 0) { attrSep(); ss << "\"POSITION\": "   << pa.pos; }
				if (pa.norm >= 0) { attrSep(); ss << "\"NORMAL\": "     << pa.norm; }
				if (pa.tex  >= 0) { attrSep(); ss << "\"TEXCOORD_0\": " << pa.tex; }
				if (pa.tan  >= 0) { attrSep(); ss << "\"TANGENT\": "    << pa.tan; }
				ss << "}";
				if (pa.idx >= 0) ss << ", \"indices\": " << pa.idx;
				if (prim.materialIndex >= 0) ss << ", \"material\": " << prim.materialIndex;
				ss << ", \"mode\": " << static_cast<int>(prim.mode);
				ss << "}";
				if (pi + 1 < mesh.primitives.size()) ss << ",";
				ss << "\n";
			}
			ss << "    ]}";
			if (mi + 1 < gltf.meshes.size()) ss << ",";
			ss << "\n";
		}
		ss << "  ]";
		sections.push_back({ "meshes", ss.str() });
	}

	// materials
	if (!gltf.materials.empty()) {
		std::ostringstream ss;
		ss << "[\n";
		for (size_t mi = 0; mi < gltf.materials.size(); ++mi) {
			const GLTFMaterial& mat = gltf.materials[mi];
			ss << "    {";
			bool first = true;
			auto sep = [&]() { if (!first) ss << ", "; first = false; };

			if (!mat.name.empty()) { sep(); ss << "\"name\": " << quoted(mat.name); }

			// pbrMetallicRoughness
			{
				sep();
				const GLTFPBRMetallicRoughness& pbr = mat.pbrMetallicRoughness;
				ss << "\"pbrMetallicRoughness\": {";
				ss << "\"baseColorFactor\": " << floatArray(pbr.baseColorFactor.data(), 4);
				ss << ", \"metallicFactor\": "  << pbr.metallicFactor;
				ss << ", \"roughnessFactor\": " << pbr.roughnessFactor;
				if (pbr.baseColorTextureIndex >= 0) {
					ss << ", \"baseColorTexture\": {\"index\": " << pbr.baseColorTextureIndex << "}";
				}
				if (pbr.metallicRoughnessTextureIndex >= 0) {
					ss << ", \"metallicRoughnessTexture\": {\"index\": " << pbr.metallicRoughnessTextureIndex << "}";
				}
				ss << "}";
			}

			if (mat.doubleSided) { sep(); ss << "\"doubleSided\": true"; }

			const bool hasEmissive = (mat.emissiveFactor[0] != 0.0f || mat.emissiveFactor[1] != 0.0f || mat.emissiveFactor[2] != 0.0f);
			if (hasEmissive) { sep(); ss << "\"emissiveFactor\": " << floatArray(mat.emissiveFactor.data(), 3); }

			if (mat.normalTextureIndex >= 0) {
				sep();
				ss << "\"normalTexture\": {\"index\": " << mat.normalTextureIndex << "}";
			}
			if (mat.emissiveTextureIndex >= 0) {
				sep();
				ss << "\"emissiveTexture\": {\"index\": " << mat.emissiveTextureIndex << "}";
			}

			ss << "}";
			if (mi + 1 < gltf.materials.size()) ss << ",";
			ss << "\n";
		}
		ss << "  ]";
		sections.push_back({ "materials", ss.str() });
	}

	// textures
	if (!gltf.textures.empty()) {
		std::ostringstream ss;
		ss << "[\n";
		for (size_t ti = 0; ti < gltf.textures.size(); ++ti) {
			const GLTFTexture& tex = gltf.textures[ti];
			ss << "    {";
			bool first = true;
			if (!tex.name.empty()) { ss << "\"name\": " << quoted(tex.name); first = false; }
			if (tex.imageIndex >= 0) {
				if (!first) ss << ", ";
				ss << "\"source\": " << tex.imageIndex;
			}
			ss << "}";
			if (ti + 1 < gltf.textures.size()) ss << ",";
			ss << "\n";
		}
		ss << "  ]";
		sections.push_back({ "textures", ss.str() });
	}

	// images
	if (!gltf.images.empty()) {
		std::ostringstream ss;
		ss << "[\n";
		for (size_t ii = 0; ii < gltf.images.size(); ++ii) {
			const GLTFImage& img = gltf.images[ii];
			ss << "    {";
			bool first = true;
			auto sep = [&]() { if (!first) ss << ", "; first = false; };
			if (!img.name.empty()) { sep(); ss << "\"name\": " << quoted(img.name); }
			if (!img.uri.empty())  { sep(); ss << "\"uri\": "  << quoted(img.uri); }
			if (!img.mimeType.empty()) { sep(); ss << "\"mimeType\": " << quoted(img.mimeType); }
			ss << "}";
			if (ii + 1 < gltf.images.size()) ss << ",";
			ss << "\n";
		}
		ss << "  ]";
		sections.push_back({ "images", ss.str() });
	}

	// accessors
	if (!accessors.empty()) {
		std::ostringstream ss;
		ss << "[\n";
		for (size_t ai = 0; ai < accessors.size(); ++ai) {
			const AccessorRec& acc = accessors[ai];
			ss << "    {"
				<< "\"bufferView\": " << acc.bufferViewIndex
				<< ", \"componentType\": " << acc.componentType
				<< ", \"count\": "  << acc.count
				<< ", \"type\": "   << quoted(acc.type);
			if (acc.hasMinMax) {
				ss << ", \"min\": " << floatArray(acc.minVal, acc.minMaxCount);
				ss << ", \"max\": " << floatArray(acc.maxVal, acc.minMaxCount);
			}
			ss << "}";
			if (ai + 1 < accessors.size()) ss << ",";
			ss << "\n";
		}
		ss << "  ]";
		sections.push_back({ "accessors", ss.str() });
	}

	// bufferViews
	if (!bufferViews.empty()) {
		std::ostringstream ss;
		ss << "[\n";
		for (size_t bvi = 0; bvi < bufferViews.size(); ++bvi) {
			const BufferViewRec& bv = bufferViews[bvi];
			ss << "    {"
				<< "\"buffer\": 0"
				<< ", \"byteOffset\": " << bv.byteOffset
				<< ", \"byteLength\": " << bv.byteLength
				<< ", \"target\": "     << bv.target
				<< "}";
			if (bvi + 1 < bufferViews.size()) ss << ",";
			ss << "\n";
		}
		ss << "  ]";
		sections.push_back({ "bufferViews", ss.str() });
	}

	// buffers
	if (!binData.empty()) {
		const std::string b64 = base64Encode(binData.data(), binData.size());
		std::ostringstream ss;
		ss << "[{\"byteLength\": " << binData.size()
			<< ", \"uri\": \"data:application/octet-stream;base64," << b64 << "\"}]";
		sections.push_back({ "buffers", ss.str() });
	}

   // Output JSON
	stream << "{\n";
	for (size_t i = 0; i < sections.size(); ++i) {
		stream << "  " << quoted(sections[i].first) << ": " << sections[i].second;
		if (i + 1 < sections.size()) stream << ",";
		stream << "\n";
	}
	stream << "}\n";

	return stream.good();
}
