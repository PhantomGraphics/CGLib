#define CGLTF_IMPLEMENTATION
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "CGLib/File/ThirdParty/cgltf/cgltf.h"

#include "GLTFFileReader.h"

#include <string>
#include <cstring>

using namespace Phantom::File;

bool GLTFFileReader::read(const std::filesystem::path& filename)
{
	cgltf_options options = {};
	cgltf_data* data = nullptr;

	const std::string path = filename.string();
	cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
	if (result != cgltf_result_success) {
		return false;
	}

	result = cgltf_load_buffers(&options, data, path.c_str());
	if (result != cgltf_result_success) {
		cgltf_free(data);
		return false;
	}

	gltf = GLTFFile{};

 // Meshes
	for (cgltf_size mi = 0; mi < data->meshes_count; ++mi) {
		const cgltf_mesh& cmesh = data->meshes[mi];
		GLTFMesh mesh;
		mesh.name = cmesh.name ? cmesh.name : "";

		for (cgltf_size pi = 0; pi < cmesh.primitives_count; ++pi) {
			const cgltf_primitive& cprim = cmesh.primitives[pi];
			GLTFPrimitive prim;
			// cgltf_primitive_type starts at 0=invalid, so values differ from the glTF spec.
			// Map explicitly instead of casting directly.
			switch (cprim.type) {
			case cgltf_primitive_type_points:         prim.mode = GLTFPrimitiveMode::Points;        break;
			case cgltf_primitive_type_lines:          prim.mode = GLTFPrimitiveMode::Lines;         break;
			case cgltf_primitive_type_line_loop:      prim.mode = GLTFPrimitiveMode::LineLoop;      break;
			case cgltf_primitive_type_line_strip:     prim.mode = GLTFPrimitiveMode::LineStrip;     break;
			case cgltf_primitive_type_triangles:      prim.mode = GLTFPrimitiveMode::Triangles;     break;
			case cgltf_primitive_type_triangle_strip: prim.mode = GLTFPrimitiveMode::TriangleStrip; break;
			case cgltf_primitive_type_triangle_fan:   prim.mode = GLTFPrimitiveMode::TriangleFan;   break;
			default:                                  prim.mode = GLTFPrimitiveMode::Triangles;     break;
			}

			if (cprim.material) {
				prim.materialIndex = static_cast<int>(cprim.material - data->materials);
			}

			for (cgltf_size ai = 0; ai < cprim.attributes_count; ++ai) {
				const cgltf_attribute& attr = cprim.attributes[ai];
				const cgltf_accessor* acc = attr.data;

				if (attr.type == cgltf_attribute_type_position) {
					prim.positions.resize(acc->count);
					for (cgltf_size vi = 0; vi < acc->count; ++vi) {
						float v[3] = {};
						cgltf_accessor_read_float(acc, vi, v, 3);
						prim.positions[vi] = { v[0], v[1], v[2] };
					}
				}
				else if (attr.type == cgltf_attribute_type_normal) {
					prim.normals.resize(acc->count);
					for (cgltf_size vi = 0; vi < acc->count; ++vi) {
						float v[3] = {};
						cgltf_accessor_read_float(acc, vi, v, 3);
						prim.normals[vi] = { v[0], v[1], v[2] };
					}
				}
				else if (attr.type == cgltf_attribute_type_texcoord) {
					prim.texCoords.resize(acc->count);
					for (cgltf_size vi = 0; vi < acc->count; ++vi) {
						float v[2] = {};
						cgltf_accessor_read_float(acc, vi, v, 2);
						prim.texCoords[vi] = { v[0], v[1] };
					}
				}
				else if (attr.type == cgltf_attribute_type_tangent) {
					prim.tangents.resize(acc->count);
					for (cgltf_size vi = 0; vi < acc->count; ++vi) {
						float v[4] = {};
						cgltf_accessor_read_float(acc, vi, v, 4);
						prim.tangents[vi] = { v[0], v[1], v[2], v[3] };
					}
				}
				else if (attr.type == cgltf_attribute_type_joints) {
					prim.joints.resize(acc->count);
					for (cgltf_size vi = 0; vi < acc->count; ++vi) {
						cgltf_uint v[4] = {};
						cgltf_accessor_read_uint(acc, vi, v, 4);
						prim.joints[vi] = {
							static_cast<int>(v[0]), static_cast<int>(v[1]),
							static_cast<int>(v[2]), static_cast<int>(v[3])
						};
					}
				}
				else if (attr.type == cgltf_attribute_type_weights) {
					prim.weights.resize(acc->count);
					for (cgltf_size vi = 0; vi < acc->count; ++vi) {
						float v[4] = {};
						cgltf_accessor_read_float(acc, vi, v, 4);
						prim.weights[vi] = { v[0], v[1], v[2], v[3] };
					}
				}
			}

			if (cprim.indices) {
				const cgltf_accessor* idxAcc = cprim.indices;
				prim.indices.resize(idxAcc->count);
				for (cgltf_size ii = 0; ii < idxAcc->count; ++ii) {
					prim.indices[ii] = static_cast<unsigned int>(cgltf_accessor_read_index(idxAcc, ii));
				}
			}

			// Morph targets: each target only ever carries a POSITION displacement in practice
			// (this reader ignores NORMAL/TANGENT morph deltas, matching GLTFPrimitive's own
			// POSITION-only convention -- see GLTFFile.h).
			for (cgltf_size ti = 0; ti < cprim.targets_count; ++ti) {
				const cgltf_morph_target& ctarget = cprim.targets[ti];
				std::vector<Math::Vector3df> deltas;
				for (cgltf_size ai = 0; ai < ctarget.attributes_count; ++ai) {
					const cgltf_attribute& attr = ctarget.attributes[ai];
					if (attr.type != cgltf_attribute_type_position) continue;
					const cgltf_accessor* acc = attr.data;
					deltas.resize(acc->count);
					for (cgltf_size vi = 0; vi < acc->count; ++vi) {
						float v[3] = {};
						cgltf_accessor_read_float(acc, vi, v, 3);
						deltas[vi] = { v[0], v[1], v[2] };
					}
					break;
				}
				prim.targets.push_back(std::move(deltas));
			}

			mesh.primitives.push_back(std::move(prim));
		}

		mesh.morphWeights.assign(cmesh.weights, cmesh.weights + cmesh.weights_count);

		gltf.meshes.push_back(std::move(mesh));
	}

    // Materials
	for (cgltf_size mi = 0; mi < data->materials_count; ++mi) {
		const cgltf_material& cmat = data->materials[mi];
		GLTFMaterial mat;
		mat.name        = cmat.name ? cmat.name : "";
		mat.doubleSided = cmat.double_sided != 0;
		mat.emissiveFactor = {
			cmat.emissive_factor[0],
			cmat.emissive_factor[1],
			cmat.emissive_factor[2]
		};

		if (cmat.has_pbr_metallic_roughness) {
			const cgltf_pbr_metallic_roughness& pbr = cmat.pbr_metallic_roughness;
			mat.pbrMetallicRoughness.baseColorFactor = {
				pbr.base_color_factor[0],
				pbr.base_color_factor[1],
				pbr.base_color_factor[2],
				pbr.base_color_factor[3]
			};
			mat.pbrMetallicRoughness.metallicFactor  = pbr.metallic_factor;
			mat.pbrMetallicRoughness.roughnessFactor = pbr.roughness_factor;

			if (pbr.base_color_texture.texture) {
				mat.pbrMetallicRoughness.baseColorTextureIndex =
					static_cast<int>(pbr.base_color_texture.texture - data->textures);
			}
			if (pbr.metallic_roughness_texture.texture) {
				mat.pbrMetallicRoughness.metallicRoughnessTextureIndex =
					static_cast<int>(pbr.metallic_roughness_texture.texture - data->textures);
			}
		}

		if (cmat.normal_texture.texture) {
			mat.normalTextureIndex =
				static_cast<int>(cmat.normal_texture.texture - data->textures);
		}
		if (cmat.emissive_texture.texture) {
			mat.emissiveTextureIndex =
				static_cast<int>(cmat.emissive_texture.texture - data->textures);
		}

		for (cgltf_size ei = 0; ei < cmat.extensions_count; ++ei) {
			const cgltf_extension& ext = cmat.extensions[ei];
			if (!ext.name) continue;
			mat.extensionsJson.emplace_back(ext.name, ext.data ? ext.data : "");
		}

		gltf.materials.push_back(std::move(mat));
	}

    // Textures
	for (cgltf_size ti = 0; ti < data->textures_count; ++ti) {
		const cgltf_texture& ctex = data->textures[ti];
		GLTFTexture tex;
		tex.name = ctex.name ? ctex.name : "";
		if (ctex.image) {
			tex.imageIndex = static_cast<int>(ctex.image - data->images);
		}
		gltf.textures.push_back(std::move(tex));
	}

   // Images
	for (cgltf_size ii = 0; ii < data->images_count; ++ii) {
		const cgltf_image& cimg = data->images[ii];
		GLTFImage img;
		img.name     = cimg.name      ? cimg.name      : "";
		img.uri      = cimg.uri       ? cimg.uri       : "";
		img.mimeType = cimg.mime_type ? cimg.mime_type : "";
		// Extract GLB-embedded image data from buffer view
		if (cimg.buffer_view && cimg.buffer_view->buffer &&
		    cimg.buffer_view->buffer->data)
		{
			const uint8_t* ptr =
				static_cast<const uint8_t*>(cimg.buffer_view->buffer->data)
				+ cimg.buffer_view->offset;
			img.data.assign(ptr, ptr + cimg.buffer_view->size);
		}
		gltf.images.push_back(std::move(img));
	}

  // Nodes
	for (cgltf_size ni = 0; ni < data->nodes_count; ++ni) {
		const cgltf_node& cnode = data->nodes[ni];
		GLTFNode node;
		node.name = cnode.name ? cnode.name : "";

		if (cnode.has_translation) {
			node.translation = {
				cnode.translation[0],
				cnode.translation[1],
				cnode.translation[2]
			};
		}
		if (cnode.has_rotation) {
			node.rotation = {
				cnode.rotation[0],
				cnode.rotation[1],
				cnode.rotation[2],
				cnode.rotation[3]
			};
		}
		if (cnode.has_scale) {
			node.scale = { cnode.scale[0], cnode.scale[1], cnode.scale[2] };
		}
		if (cnode.has_matrix) {
			// glTF spec: a node has either "matrix" or TRS, never both -- has_translation/
			// has_rotation/has_scale are false in this case, so translation/rotation/scale above
			// keep their identity defaults. Column-major, matching glTF's own storage order.
			node.hasMatrix = true;
			for (int i = 0; i < 16; ++i) {
				node.matrix[i] = cnode.matrix[i];
			}
		}

		if (cnode.mesh) {
			node.meshIndex = static_cast<int>(cnode.mesh - data->meshes);
		}
		if (cnode.skin) {
			node.skin = static_cast<int>(cnode.skin - data->skins);
		}
		for (cgltf_size ci = 0; ci < cnode.children_count; ++ci) {
			node.children.push_back(static_cast<int>(cnode.children[ci] - data->nodes));
		}

		node.morphWeights.assign(cnode.weights, cnode.weights + cnode.weights_count);

		gltf.nodes.push_back(std::move(node));
	}

  // Skins
	for (cgltf_size si = 0; si < data->skins_count; ++si) {
		const cgltf_skin& cskin = data->skins[si];
		GLTFSkin skin;
		skin.name = cskin.name ? cskin.name : "";

		skin.joints.resize(cskin.joints_count);
		for (cgltf_size ji = 0; ji < cskin.joints_count; ++ji) {
			skin.joints[ji] = static_cast<int>(cskin.joints[ji] - data->nodes);
		}

		if (cskin.skeleton) {
			skin.skeletonRoot = static_cast<int>(cskin.skeleton - data->nodes);
		}

		if (cskin.inverse_bind_matrices) {
			const cgltf_accessor* acc = cskin.inverse_bind_matrices;
			skin.inverseBindMatrices.resize(acc->count);
			for (cgltf_size mi = 0; mi < acc->count; ++mi) {
				cgltf_accessor_read_float(acc, mi, skin.inverseBindMatrices[mi].data(), 16);
			}
		} else {
			// glTF spec: undefined inverseBindMatrices means identity for every joint.
			std::array<float, 16> identity = {
				1.f, 0.f, 0.f, 0.f,
				0.f, 1.f, 0.f, 0.f,
				0.f, 0.f, 1.f, 0.f,
				0.f, 0.f, 0.f, 1.f
			};
			skin.inverseBindMatrices.assign(cskin.joints_count, identity);
		}

		gltf.skins.push_back(std::move(skin));
	}

  // Scenes
	for (cgltf_size si = 0; si < data->scenes_count; ++si) {
		const cgltf_scene& cscene = data->scenes[si];
		GLTFScene scene;
		scene.name = cscene.name ? cscene.name : "";
		for (cgltf_size ni = 0; ni < cscene.nodes_count; ++ni) {
			scene.nodes.push_back(static_cast<int>(cscene.nodes[ni] - data->nodes));
		}
		gltf.scenes.push_back(std::move(scene));
	}

	if (data->scene) {
		gltf.defaultScene = static_cast<int>(data->scene - data->scenes);
	}

  // Root-level extensions (e.g. "VRM" / "VRMC_vrm" for VRM avatar profiles)
	for (cgltf_size ei = 0; ei < data->data_extensions_count; ++ei) {
		const cgltf_extension& ext = data->data_extensions[ei];
		if (!ext.name) continue;
		gltf.rootExtensionsJson.emplace_back(ext.name, ext.data ? ext.data : "");
	}

	cgltf_free(data);
	return true;
}
