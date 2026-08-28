"""Generate minimal glTF test models for GltfViewer scenario tests."""
import struct
import base64
import json
import math
import os

OUT = os.path.dirname(__file__)

def b64(data: bytes) -> str:
    return base64.b64encode(data).decode('ascii')

def pack_verts(vertices):
    """Pack list of (x,y,z) tuples as little-endian float32."""
    data = b''
    for v in vertices:
        data += struct.pack('<fff', *v)
    return data

# ---------------------------------------------------------------------------
# triangle.gltf
# 1 mesh, 1 node, 1 scene, 0 material, 0 texture
# ---------------------------------------------------------------------------
tri_verts = [(-0.5, -0.5, 0.0), (0.5, -0.5, 0.0), (0.0, 0.5, 0.0)]
tri_data  = pack_verts(tri_verts)
tri_b64   = b64(tri_data)
tri_len   = len(tri_data)  # 36

triangle = {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{
        "name": "Triangle",
        "primitives": [{"attributes": {"POSITION": 0}}]
    }],
    "accessors": [{
        "bufferView": 0,
        "componentType": 5126,
        "count": 3,
        "type": "VEC3",
        "min": [-0.5, -0.5, 0.0],
        "max": [0.5, 0.5, 0.0]
    }],
    "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": tri_len}],
    "buffers": [{"byteLength": tri_len,
                 "uri": "data:application/octet-stream;base64," + tri_b64}]
}

with open(os.path.join(OUT, "triangle.gltf"), "w", encoding="utf-8") as f:
    json.dump(triangle, f, indent=2)
print("triangle.gltf  MeshCount=1 NodeCount=1 SceneCount=1 MaterialCount=0 TextureCount=0")

# ---------------------------------------------------------------------------
# cube_material.gltf
# 1 mesh (2 triangles = 1 quad face, just 2 primitives reusing same accessor),
# 1 node, 1 scene, 1 material, 0 texture
# For simplicity: same triangle geometry, 1 primitive with a material.
# ---------------------------------------------------------------------------
mat_data = pack_verts(tri_verts)
mat_b64  = b64(mat_data)
mat_len  = len(mat_data)

cube_material = {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{
        "name": "CubeMaterial",
        "primitives": [{"attributes": {"POSITION": 0}, "material": 0}]
    }],
    "materials": [{
        "name": "RedMaterial",
        "pbrMetallicRoughness": {
            "baseColorFactor": [1.0, 0.0, 0.0, 1.0],
            "metallicFactor": 0.0,
            "roughnessFactor": 0.5
        }
    }],
    "accessors": [{
        "bufferView": 0,
        "componentType": 5126,
        "count": 3,
        "type": "VEC3",
        "min": [-0.5, -0.5, 0.0],
        "max": [0.5, 0.5, 0.0]
    }],
    "bufferViews": [{"buffer": 0, "byteOffset": 0, "byteLength": mat_len}],
    "buffers": [{"byteLength": mat_len,
                 "uri": "data:application/octet-stream;base64," + mat_b64}]
}

with open(os.path.join(OUT, "cube_material.gltf"), "w", encoding="utf-8") as f:
    json.dump(cube_material, f, indent=2)
print("cube_material.gltf  MeshCount=1 MaterialCount=1 TextureCount=0")

# ---------------------------------------------------------------------------
# multi_mesh.gltf
# 3 meshes, 3 nodes, 1 scene, 0 material, 0 texture, PrimitiveCount=3
# Each mesh has 1 primitive. Nodes are at different positions via translation.
# ---------------------------------------------------------------------------
v0 = pack_verts([(-0.5, -0.5, 0.0), (0.5, -0.5, 0.0), (0.0, 0.5, 0.0)])
v1 = pack_verts([(-0.5, -0.5, 1.0), (0.5, -0.5, 1.0), (0.0, 0.5, 1.0)])
v2 = pack_verts([(-0.5, -0.5, 2.0), (0.5, -0.5, 2.0), (0.0, 0.5, 2.0)])

mm_data = v0 + v1 + v2
mm_b64  = b64(mm_data)
mm_len  = len(mm_data)  # 108

multi_mesh = {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0, 1, 2]}],
    "nodes": [
        {"mesh": 0},
        {"mesh": 1},
        {"mesh": 2}
    ],
    "meshes": [
        {"name": "Mesh0", "primitives": [{"attributes": {"POSITION": 0}}]},
        {"name": "Mesh1", "primitives": [{"attributes": {"POSITION": 1}}]},
        {"name": "Mesh2", "primitives": [{"attributes": {"POSITION": 2}}]}
    ],
    "accessors": [
        {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": [-0.5, -0.5, 0.0], "max": [0.5, 0.5, 0.0]},
        {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": [-0.5, -0.5, 1.0], "max": [0.5, 0.5, 1.0]},
        {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": [-0.5, -0.5, 2.0], "max": [0.5, 0.5, 2.0]}
    ],
    "bufferViews": [
        {"buffer": 0, "byteOffset":  0, "byteLength": 36},
        {"buffer": 0, "byteOffset": 36, "byteLength": 36},
        {"buffer": 0, "byteOffset": 72, "byteLength": 36}
    ],
    "buffers": [{"byteLength": mm_len,
                 "uri": "data:application/octet-stream;base64," + mm_b64}]
}

with open(os.path.join(OUT, "multi_mesh.gltf"), "w", encoding="utf-8") as f:
    json.dump(multi_mesh, f, indent=2)
print("multi_mesh.gltf  MeshCount=3 NodeCount=3 PrimitiveCount=3")

# ---------------------------------------------------------------------------
# Phase 4 (docs/todo/PLAN_scenario_test_synthetic_assets.md): synthetic
# replacements for Khronos glTF-Sample-Assets models (DamagedHelmet/WaterBottle/
# AntiqueCamera/Avocado/Corset/Duck/CesiumMan/RiggedFigure/RiggedSimple/
# MultiUVTest/NegativeScaleTest/NormalTangentTest/NormalTangentMirrorTest/
# OrientationTest/TextureCoordinateTest/BoxVertexColors/AlphaBlendModeTest/
# EmissiveStrengthTest/UnlitTest). Khronos's own samples exist to exercise glTF
# *features* (PBR materials, UV sets, skinning, extensions), not to look
# realistic, so procedural checkerboard/gradient/flat-normal PNG textures over
# minimal geometry cover the same ground without any third-party asset.
# Real-data versions of the 19 replaced scenarios still exist for reinforcement
# testing under ThirdPartyScenarios/GltfViewer/ (non-public, root-repo-only).
# ---------------------------------------------------------------------------
import zlib

FLOAT, UBYTE, USHORT = 5126, 5121, 5123


def make_png(width, height, pixel_fn):
    """Minimal from-scratch RGBA PNG encoder (no external imaging deps)."""
    raw = bytearray()
    for y in range(height):
        raw.append(0)  # filter type: none
        for x in range(width):
            raw.extend(pixel_fn(x, y))

    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data) & 0xffffffff)

    ihdr = struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)  # 8-bit RGBA
    idat = zlib.compress(bytes(raw), 9)
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', ihdr) + chunk(b'IDAT', idat) + chunk(b'IEND', b'')


def checkerboard_png(size=8, cell=2, color_a=(214, 64, 64, 255), color_b=(255, 255, 255, 255)):
    return make_png(size, size, lambda x, y: color_a if ((x // cell) + (y // cell)) % 2 == 0 else color_b)


def gradient_png(size=8):
    return make_png(size, size, lambda x, y: (int(255 * x / (size - 1)), int(255 * y / (size - 1)), 128, 255))


def flat_normal_png(size=4):
    return make_png(size, size, lambda x, y: (128, 128, 255, 255))


def png_data_uri(png_bytes):
    return "data:image/png;base64," + b64(png_bytes)


class Buf:
    """Accumulates one glTF buffer's worth of bytes plus matching bufferViews/accessors."""

    def __init__(self):
        self.data = b''
        self.bufferViews = []
        self.accessors = []

    def _add(self, packed_bytes, component_type, accessor_type, count, minv=None, maxv=None):
        offset = len(self.data)
        self.data += packed_bytes
        self.bufferViews.append({"buffer": 0, "byteOffset": offset, "byteLength": len(packed_bytes)})
        acc = {"bufferView": len(self.bufferViews) - 1, "componentType": component_type,
               "count": count, "type": accessor_type}
        if minv is not None:
            acc["min"] = minv
        if maxv is not None:
            acc["max"] = maxv
        self.accessors.append(acc)
        return len(self.accessors) - 1

    def add_vec3(self, vecs, minmax=False):
        data = b''.join(struct.pack('<fff', *v) for v in vecs)
        minv = [min(v[i] for v in vecs) for i in range(3)] if minmax else None
        maxv = [max(v[i] for v in vecs) for i in range(3)] if minmax else None
        return self._add(data, FLOAT, "VEC3", len(vecs), minv, maxv)

    def add_vec4(self, vecs):
        data = b''.join(struct.pack('<ffff', *v) for v in vecs)
        return self._add(data, FLOAT, "VEC4", len(vecs))

    def add_vec2(self, vecs):
        data = b''.join(struct.pack('<ff', *v) for v in vecs)
        return self._add(data, FLOAT, "VEC2", len(vecs))

    def add_ushort_vec4(self, vecs):
        data = b''.join(struct.pack('<HHHH', *v) for v in vecs)
        return self._add(data, USHORT, "VEC4", len(vecs))

    def add_indices(self, idx):
        data = b''.join(struct.pack('<H', i) for i in idx)
        return self._add(data, USHORT, "SCALAR", len(idx))

    def buffer_entry(self):
        return {"byteLength": len(self.data), "uri": "data:application/octet-stream;base64," + b64(self.data)}


def write_gltf(filename, doc):
    with open(os.path.join(OUT, filename), "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=2)


DEFAULT_SAMPLER = {"magFilter": 9729, "minFilter": 9987, "wrapS": 10497, "wrapT": 10497}

QUAD_POS     = [(-0.5, -0.5, 0.0), (0.5, -0.5, 0.0), (0.5, 0.5, 0.0), (-0.5, 0.5, 0.0)]
QUAD_NORMAL  = [(0.0, 0.0, 1.0)] * 4
QUAD_UV0     = [(0.0, 1.0), (1.0, 1.0), (1.0, 0.0), (0.0, 0.0)]
QUAD_TANGENT = [(1.0, 0.0, 0.0, 1.0)] * 4
QUAD_INDICES = [0, 1, 2, 0, 2, 3]

TRI_POS    = [(-0.4, -0.4, 0.0), (0.4, -0.4, 0.0), (0.0, 0.4, 0.0)]
TRI_NORMAL = [(0.0, 0.0, 1.0)] * 3
TRI_INDICES = [0, 1, 2]

# ---------------------------------------------------------------------------
# pbr_basic.gltf -- replaces DamagedHelmet/WaterBottle (exact MeshCount=1
# MaterialCount=1 SceneCount=1 PrimitiveCount=1 + texture)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(QUAD_POS, minmax=True)
nrm_acc = buf.add_vec3(QUAD_NORMAL)
uv_acc = buf.add_vec2(QUAD_UV0)
idx_acc = buf.add_indices(QUAD_INDICES)
png = checkerboard_png()
write_gltf("pbr_basic.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{"name": "PbrBasicQuad", "primitives": [{
        "attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc, "TEXCOORD_0": uv_acc},
        "indices": idx_acc, "material": 0}]}],
    "materials": [{"name": "CheckerPbr", "pbrMetallicRoughness": {
        "baseColorTexture": {"index": 0}, "metallicFactor": 0.1, "roughnessFactor": 0.6}}],
    "textures": [{"sampler": 0, "source": 0}],
    "samplers": [DEFAULT_SAMPLER],
    "images": [{"uri": png_data_uri(png), "mimeType": "image/png"}],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("pbr_basic.gltf  MeshCount=1 MaterialCount=1 SceneCount=1 PrimitiveCount=1 TextureCount=1")

# ---------------------------------------------------------------------------
# pbr_multi_material.gltf -- replaces AntiqueCamera/Avocado/Corset/Duck
# (MeshCount/MaterialCount/PrimitiveCount/TextureCount > 0, SceneCount=1)
# ---------------------------------------------------------------------------
buf = Buf()
pos_a = [(x - 0.6, y, z) for (x, y, z) in QUAD_POS]
pos_b = [(x + 0.6, y, z) for (x, y, z) in QUAD_POS]
pos_a_acc = buf.add_vec3(pos_a, minmax=True)
nrm_a_acc = buf.add_vec3(QUAD_NORMAL)
uv_a_acc = buf.add_vec2(QUAD_UV0)
idx_a_acc = buf.add_indices(QUAD_INDICES)
pos_b_acc = buf.add_vec3(pos_b, minmax=True)
nrm_b_acc = buf.add_vec3(QUAD_NORMAL)
uv_b_acc = buf.add_vec2(QUAD_UV0)
idx_b_acc = buf.add_indices(QUAD_INDICES)
png_checker = checkerboard_png()
png_gradient = gradient_png()
write_gltf("pbr_multi_material.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0, 1]}],
    "nodes": [{"name": "QuadA", "mesh": 0}, {"name": "QuadB", "mesh": 1}],
    "meshes": [
        {"name": "QuadA", "primitives": [{
            "attributes": {"POSITION": pos_a_acc, "NORMAL": nrm_a_acc, "TEXCOORD_0": uv_a_acc},
            "indices": idx_a_acc, "material": 0}]},
        {"name": "QuadB", "primitives": [{
            "attributes": {"POSITION": pos_b_acc, "NORMAL": nrm_b_acc, "TEXCOORD_0": uv_b_acc},
            "indices": idx_b_acc, "material": 1}]},
    ],
    "materials": [
        {"name": "Checker", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "roughnessFactor": 0.8}},
        {"name": "Gradient", "pbrMetallicRoughness": {"baseColorTexture": {"index": 1}, "metallicFactor": 0.8, "roughnessFactor": 0.2}},
    ],
    "textures": [{"sampler": 0, "source": 0}, {"sampler": 0, "source": 1}],
    "samplers": [DEFAULT_SAMPLER],
    "images": [
        {"uri": png_data_uri(png_checker), "mimeType": "image/png"},
        {"uri": png_data_uri(png_gradient), "mimeType": "image/png"},
    ],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("pbr_multi_material.gltf  MeshCount=2 MaterialCount=2 SceneCount=1 PrimitiveCount=2 TextureCount=2")

# ---------------------------------------------------------------------------
# skin_biped_textured.gltf / skin_biped_simple.gltf -- replaces
# CesiumMan (textured) / RiggedFigure+RiggedSimple (untextured)
# ---------------------------------------------------------------------------
JOINTS0 = [(0, 0, 0, 0)] * 4
WEIGHTS0 = [(1.0, 0.0, 0.0, 0.0)] * 4


def build_skin_doc(with_texture):
    buf = Buf()
    pos_acc = buf.add_vec3(QUAD_POS, minmax=True)
    nrm_acc = buf.add_vec3(QUAD_NORMAL)
    uv_acc = buf.add_vec2(QUAD_UV0)
    joints_acc = buf.add_ushort_vec4(JOINTS0)
    weights_acc = buf.add_vec4(WEIGHTS0)
    idx_acc = buf.add_indices(QUAD_INDICES)
    prim = {
        "attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc, "TEXCOORD_0": uv_acc,
                        "JOINTS_0": joints_acc, "WEIGHTS_0": weights_acc},
        "indices": idx_acc,
    }
    doc = {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [
            {"name": "hips", "children": [1]},
            {"name": "head", "mesh": 0, "skin": 0, "translation": [0.0, 0.5, 0.0]},
        ],
        "skins": [{"name": "Biped", "joints": [0, 1], "skeletonRoot": 0}],
        "meshes": [{"name": "Head", "primitives": [prim]}],
        "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    }
    if with_texture:
        png = checkerboard_png()
        prim["material"] = 0
        doc["materials"] = [{"name": "Checker", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}]
        doc["textures"] = [{"sampler": 0, "source": 0}]
        doc["samplers"] = [DEFAULT_SAMPLER]
        doc["images"] = [{"uri": png_data_uri(png), "mimeType": "image/png"}]
    doc["buffers"] = [buf.buffer_entry()]
    return doc


write_gltf("skin_biped_textured.gltf", build_skin_doc(True))
print("skin_biped_textured.gltf  MeshCount=1 SkinCount=1 NodeCount=2 TextureCount=1")
write_gltf("skin_biped_simple.gltf", build_skin_doc(False))
print("skin_biped_simple.gltf  MeshCount=1 SkinCount=1 NodeCount=2 TextureCount=0")

# ---------------------------------------------------------------------------
# multi_uv.gltf -- replaces MultiUVTest (TEXCOORD_0 + TEXCOORD_1, only the
# former is consumed by this renderer's baseColorTexture)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(QUAD_POS, minmax=True)
nrm_acc = buf.add_vec3(QUAD_NORMAL)
uv0_acc = buf.add_vec2(QUAD_UV0)
uv1_acc = buf.add_vec2([(0.0, 0.0), (2.0, 0.0), (2.0, 2.0), (0.0, 2.0)])
idx_acc = buf.add_indices(QUAD_INDICES)
png = checkerboard_png()
write_gltf("multi_uv.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{"name": "MultiUVQuad", "primitives": [{
        "attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc, "TEXCOORD_0": uv0_acc, "TEXCOORD_1": uv1_acc},
        "indices": idx_acc, "material": 0}]}],
    "materials": [{"name": "Checker", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
    "textures": [{"sampler": 0, "source": 0}],
    "samplers": [DEFAULT_SAMPLER],
    "images": [{"uri": png_data_uri(png), "mimeType": "image/png"}],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("multi_uv.gltf  MeshCount=1 MaterialCount=1 PrimitiveCount=1 TextureCount=1 (TEXCOORD_0+TEXCOORD_1)")

# ---------------------------------------------------------------------------
# negative_scale.gltf -- replaces NegativeScaleTest (same mesh instanced with
# a positive-scale node and a negative-X-scale node -- winding/normal check)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(TRI_POS, minmax=True)
nrm_acc = buf.add_vec3(TRI_NORMAL)
idx_acc = buf.add_indices(TRI_INDICES)
write_gltf("negative_scale.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0, 1]}],
    "nodes": [
        {"name": "Positive", "mesh": 0, "translation": [-0.6, 0.0, 0.0]},
        {"name": "NegativeX", "mesh": 0, "translation": [0.6, 0.0, 0.0], "scale": [-1.0, 1.0, 1.0]},
    ],
    "meshes": [{"name": "Tri", "primitives": [{"attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc}, "indices": idx_acc}]}],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("negative_scale.gltf  MeshCount=1 NodeCount=2 PrimitiveCount=1")

# ---------------------------------------------------------------------------
# normal_tangent.gltf -- replaces NormalTangentTest/NormalTangentMirrorTest
# (regular tangent.w=+1 quad + mirrored tangent.w=-1 quad, shared normal map)
# ---------------------------------------------------------------------------
buf = Buf()
pos_a = [(x - 0.6, y, z) for (x, y, z) in QUAD_POS]
pos_b = [(x + 0.6, y, z) for (x, y, z) in QUAD_POS]
pos_a_acc = buf.add_vec3(pos_a, minmax=True)
nrm_a_acc = buf.add_vec3(QUAD_NORMAL)
uv_a_acc = buf.add_vec2(QUAD_UV0)
tan_a_acc = buf.add_vec4(QUAD_TANGENT)
idx_a_acc = buf.add_indices(QUAD_INDICES)
pos_b_acc = buf.add_vec3(pos_b, minmax=True)
nrm_b_acc = buf.add_vec3(QUAD_NORMAL)
uv_b_acc = buf.add_vec2(QUAD_UV0)
tan_b_acc = buf.add_vec4([(1.0, 0.0, 0.0, -1.0)] * 4)  # mirrored: bitangent sign flipped
idx_b_acc = buf.add_indices(QUAD_INDICES)
png = flat_normal_png()
write_gltf("normal_tangent.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0, 1]}],
    "nodes": [{"name": "NormalRegular", "mesh": 0}, {"name": "NormalMirrored", "mesh": 1}],
    "meshes": [
        {"name": "NormalRegular", "primitives": [{
            "attributes": {"POSITION": pos_a_acc, "NORMAL": nrm_a_acc, "TEXCOORD_0": uv_a_acc, "TANGENT": tan_a_acc},
            "indices": idx_a_acc, "material": 0}]},
        {"name": "NormalMirrored", "primitives": [{
            "attributes": {"POSITION": pos_b_acc, "NORMAL": nrm_b_acc, "TEXCOORD_0": uv_b_acc, "TANGENT": tan_b_acc},
            "indices": idx_b_acc, "material": 0}]},
    ],
    "materials": [{"name": "NormalMapped", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.8, 0.8, 1.0]},
                    "normalTexture": {"index": 0}}],
    "textures": [{"sampler": 0, "source": 0}],
    "samplers": [DEFAULT_SAMPLER],
    "images": [{"uri": png_data_uri(png), "mimeType": "image/png"}],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("normal_tangent.gltf  MeshCount=2 MaterialCount=1 SceneCount=1 PrimitiveCount=2 TextureCount=1")

# ---------------------------------------------------------------------------
# orientation.gltf -- replaces OrientationTest (same mesh instanced across
# nodes with different Z-axis rotations)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(TRI_POS, minmax=True)
nrm_acc = buf.add_vec3(TRI_NORMAL)
idx_acc = buf.add_indices(TRI_INDICES)


def quat_z(deg):
    half = math.radians(deg) / 2.0
    return [0.0, 0.0, math.sin(half), math.cos(half)]


orientation_nodes = [
    {"name": f"Rot{deg}", "mesh": 0, "translation": [(i - 1.5) * 0.5, 0.0, 0.0], "rotation": quat_z(deg)}
    for i, deg in enumerate([0, 90, 180, 270])
]
write_gltf("orientation.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": list(range(len(orientation_nodes)))}],
    "nodes": orientation_nodes,
    "meshes": [{"name": "Tri", "primitives": [{"attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc}, "indices": idx_acc}]}],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("orientation.gltf  MeshCount=1 NodeCount=4 PrimitiveCount=1")

# ---------------------------------------------------------------------------
# texture_coordinate.gltf -- replaces TextureCoordinateTest (UVs outside
# [0,1] with a REPEAT sampler, to exercise UV wrap/tile)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(QUAD_POS, minmax=True)
nrm_acc = buf.add_vec3(QUAD_NORMAL)
uv_acc = buf.add_vec2([(0.0, 2.0), (2.0, 2.0), (2.0, 0.0), (0.0, 0.0)])
idx_acc = buf.add_indices(QUAD_INDICES)
png = checkerboard_png(size=8, cell=1)
write_gltf("texture_coordinate.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{"name": "TileQuad", "primitives": [{
        "attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc, "TEXCOORD_0": uv_acc},
        "indices": idx_acc, "material": 0}]}],
    "materials": [{"name": "Tiled", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}}}],
    "textures": [{"sampler": 0, "source": 0}],
    "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 10497, "wrapT": 10497}],
    "images": [{"uri": png_data_uri(png), "mimeType": "image/png"}],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("texture_coordinate.gltf  MeshCount=1 MaterialCount=1 PrimitiveCount=1 TextureCount=1 (tiled UVs)")

# ---------------------------------------------------------------------------
# box_vertex_colors.gltf -- replaces BoxVertexColors (COLOR_0 attribute this
# renderer does not consume -- must load without crashing)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(QUAD_POS, minmax=True)
nrm_acc = buf.add_vec3(QUAD_NORMAL)
col_acc = buf.add_vec4([(1.0, 0.0, 0.0, 1.0), (0.0, 1.0, 0.0, 1.0), (0.0, 0.0, 1.0, 1.0), (1.0, 1.0, 0.0, 1.0)])
idx_acc = buf.add_indices(QUAD_INDICES)
write_gltf("box_vertex_colors.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{"name": "VertexColorQuad", "primitives": [{
        "attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc, "COLOR_0": col_acc},
        "indices": idx_acc, "material": 0}]}],
    "materials": [{"name": "Plain", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.8, 0.8, 1.0]}}],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("box_vertex_colors.gltf  MeshCount=1 PrimitiveCount=1 (COLOR_0 present, ignored by this renderer)")

# ---------------------------------------------------------------------------
# alpha_blend_mode.gltf -- replaces AlphaBlendModeTest (alphaMode/alphaCutoff
# this renderer does not implement -- must load without crashing)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(QUAD_POS, minmax=True)
nrm_acc = buf.add_vec3(QUAD_NORMAL)
idx_acc = buf.add_indices(QUAD_INDICES)
write_gltf("alpha_blend_mode.gltf", {
    "asset": {"version": "2.0"},
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{"name": "AlphaQuad", "primitives": [{
        "attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc}, "indices": idx_acc, "material": 1}]}],
    "materials": [
        {"name": "Opaque", "alphaMode": "OPAQUE", "pbrMetallicRoughness": {"baseColorFactor": [0.8, 0.2, 0.2, 1.0]}},
        {"name": "Blend", "alphaMode": "BLEND", "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.8, 0.2, 0.4]}},
        {"name": "Mask", "alphaMode": "MASK", "alphaCutoff": 0.5, "pbrMetallicRoughness": {"baseColorFactor": [0.2, 0.2, 0.8, 0.7]}},
    ],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("alpha_blend_mode.gltf  MeshCount=1 MaterialCount=3 PrimitiveCount=1")

# ---------------------------------------------------------------------------
# emissive_strength.gltf -- replaces EmissiveStrengthTest
# (KHR_materials_emissive_strength this renderer does not implement)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(QUAD_POS, minmax=True)
nrm_acc = buf.add_vec3(QUAD_NORMAL)
idx_acc = buf.add_indices(QUAD_INDICES)
write_gltf("emissive_strength.gltf", {
    "asset": {"version": "2.0"},
    "extensionsUsed": ["KHR_materials_emissive_strength"],
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{"name": "EmissiveQuad", "primitives": [{
        "attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc}, "indices": idx_acc, "material": 0}]}],
    "materials": [{
        "name": "Emissive", "pbrMetallicRoughness": {"baseColorFactor": [0.05, 0.05, 0.05, 1.0]},
        "emissiveFactor": [1.0, 0.5, 0.0],
        "extensions": {"KHR_materials_emissive_strength": {"emissiveStrength": 5.0}},
    }],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("emissive_strength.gltf  MeshCount=1 MaterialCount=1 PrimitiveCount=1")

# ---------------------------------------------------------------------------
# unlit.gltf -- replaces UnlitTest (KHR_materials_unlit this renderer does
# not implement -- must load and render via the normal PBR pipeline instead)
# ---------------------------------------------------------------------------
buf = Buf()
pos_acc = buf.add_vec3(QUAD_POS, minmax=True)
nrm_acc = buf.add_vec3(QUAD_NORMAL)
idx_acc = buf.add_indices(QUAD_INDICES)
write_gltf("unlit.gltf", {
    "asset": {"version": "2.0"},
    "extensionsUsed": ["KHR_materials_unlit"],
    "scene": 0,
    "scenes": [{"nodes": [0]}],
    "nodes": [{"mesh": 0}],
    "meshes": [{"name": "UnlitQuad", "primitives": [{
        "attributes": {"POSITION": pos_acc, "NORMAL": nrm_acc}, "indices": idx_acc, "material": 0}]}],
    "materials": [{
        "name": "Unlit", "pbrMetallicRoughness": {"baseColorFactor": [0.9, 0.9, 0.2, 1.0]},
        "extensions": {"KHR_materials_unlit": {}},
    }],
    "accessors": buf.accessors, "bufferViews": buf.bufferViews,
    "buffers": [buf.buffer_entry()],
})
print("unlit.gltf  MeshCount=1 MaterialCount=1 PrimitiveCount=1")
