"""Generate minimal synthetic VRM 0.x / VRM 1.0 test models for GltfViewer scenario tests.

A VRM file is an ordinary glTF/glb document plus a root "extensions.VRM" (0.x) or
"extensions.VRMC_vrm" (1.0) object carrying a humanoid bone map and named facial expressions
(built from the document's own glTF morph targets) -- see CGLib/GltfRenderer/Vrm/VrmReader.h.
No real character mesh/texture is needed to exercise that: a handful of empty "bone" nodes plus
one small mesh with 2 morph targets is enough. This synthetic pair backs
load_vrm0_sample.json / load_vrm1_sample.json without any third-party character asset.
"""
import struct
import base64
import json
import os

OUT = os.path.dirname(__file__)

def b64(data: bytes) -> str:
    return base64.b64encode(data).decode('ascii')

def pack_vec3s(vecs):
    data = b''
    for v in vecs:
        data += struct.pack('<fff', *v)
    return data

# ---------------------------------------------------------------------------
# Shared geometry: a small "head" triangle + 2 morph targets (blink / smile),
# owned by a 7-node simplified humanoid bone hierarchy.
# ---------------------------------------------------------------------------
BASE_VERTS = [(-0.1, -0.1, 0.0), (0.1, -0.1, 0.0), (0.0, 0.15, 0.0)]
BLINK_DELTA = [(0.0, -0.03, 0.0), (0.0, -0.03, 0.0), (0.0, -0.03, 0.0)]
SMILE_DELTA = [(0.02, 0.0, 0.0), (-0.02, 0.0, 0.0), (0.0, 0.02, 0.0)]

base_bytes  = pack_vec3s(BASE_VERTS)
blink_bytes = pack_vec3s(BLINK_DELTA)
smile_bytes = pack_vec3s(SMILE_DELTA)
buf_bytes   = base_bytes + blink_bytes + smile_bytes
buf_b64     = b64(buf_bytes)

# hips(0) -> spine(1) -> head(2, owns the mesh); hips also parents 2 arms + 2 legs (flat, no elbows/knees)
NODES = [
    {"name": "hips",          "translation": [0.0, 0.9, 0.0], "children": [1, 3, 4, 5, 6]},
    {"name": "spine",         "translation": [0.0, 0.2, 0.0], "children": [2]},
    {"name": "head",          "translation": [0.0, 0.3, 0.0], "mesh": 0, "skin": 0},
    {"name": "leftUpperArm",  "translation": [-0.2, 0.1, 0.0]},
    {"name": "rightUpperArm", "translation": [0.2, 0.1, 0.0]},
    {"name": "leftUpperLeg",  "translation": [-0.1, -0.9, 0.0]},
    {"name": "rightUpperLeg", "translation": [0.1, -0.9, 0.0]},
]
HUMAN_BONE_NODES = {
    "hips": 0, "spine": 1, "head": 2,
    "leftUpperArm": 3, "rightUpperArm": 4,
    "leftUpperLeg": 5, "rightUpperLeg": 6,
}

def base_document():
    return {
        "asset": {"version": "2.0"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": NODES,
        "meshes": [{
            "name": "Head",
            "primitives": [{
                "attributes": {"POSITION": 0},
                "targets": [{"POSITION": 1}, {"POSITION": 2}]
            }]
        }],
        "skins": [{"name": "Humanoid", "joints": [0, 1, 2, 3, 4, 5, 6], "skeletonRoot": 0}],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": 3, "type": "VEC3",
             "min": [-0.1, -0.1, 0.0], "max": [0.1, 0.15, 0.0]},
            {"bufferView": 1, "componentType": 5126, "count": 3, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": 3, "type": "VEC3"},
        ],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0,  "byteLength": len(base_bytes)},
            {"buffer": 0, "byteOffset": len(base_bytes), "byteLength": len(blink_bytes)},
            {"buffer": 0, "byteOffset": len(base_bytes) + len(blink_bytes), "byteLength": len(smile_bytes)},
        ],
        "buffers": [{"byteLength": len(buf_bytes),
                     "uri": "data:application/octet-stream;base64," + buf_b64}],
    }

# ---------------------------------------------------------------------------
# vrm0_synthetic.vrm -- extensions.VRM (VrmSpecVersion::V0)
# ---------------------------------------------------------------------------
vrm0 = base_document()
vrm0["extensionsUsed"] = ["VRM"]
vrm0["extensions"] = {
    "VRM": {
        "exporterVersion": "gen_vrm_test_models.py",
        "meta": {
            "title": "SyntheticVRM0Avatar",
            "version": "1.0",
            "author": "synthetic-generator",
            "licenseName": "CC0-1.0"
        },
        "humanoid": {
            "humanBones": [{"bone": name, "node": idx} for name, idx in HUMAN_BONE_NODES.items()]
        },
        "blendShapeMaster": {
            "blendShapeGroups": [
                {"name": "Blink", "presetName": "blink", "isBinary": False,
                 "binds": [{"mesh": 2, "index": 0, "weight": 100.0}]},
                {"name": "Custom_Smile", "presetName": "", "isBinary": False,
                 "binds": [{"mesh": 2, "index": 1, "weight": 100.0}]}
            ]
        }
    }
}

with open(os.path.join(OUT, "vrm0_synthetic.vrm"), "w", encoding="utf-8") as f:
    json.dump(vrm0, f, indent=2)
print("vrm0_synthetic.vrm  SpecVersion=0 HumanBoneCount=%d ExpressionCount=2" % len(HUMAN_BONE_NODES))

# ---------------------------------------------------------------------------
# vrm1_synthetic.vrm -- extensions.VRMC_vrm (VrmSpecVersion::V1)
# ---------------------------------------------------------------------------
vrm1 = base_document()
vrm1["extensionsUsed"] = ["VRMC_vrm"]
vrm1["extensions"] = {
    "VRMC_vrm": {
        "specVersion": "1.0",
        "meta": {
            "name": "SyntheticVRM1Avatar",
            "version": "1.0",
            "authors": ["synthetic-generator"],
            "licenseUrl": "https://creativecommons.org/publicdomain/zero/1.0/"
        },
        "humanoid": {
            "humanBones": {name: {"node": idx} for name, idx in HUMAN_BONE_NODES.items()}
        },
        "expressions": {
            "preset": {
                "blink": {"morphTargetBinds": [{"node": 2, "index": 0, "weight": 1.0}], "isBinary": False}
            },
            "custom": {
                "smile": {"morphTargetBinds": [{"node": 2, "index": 1, "weight": 1.0}], "isBinary": False}
            }
        }
    }
}

with open(os.path.join(OUT, "vrm1_synthetic.vrm"), "w", encoding="utf-8") as f:
    json.dump(vrm1, f, indent=2)
print("vrm1_synthetic.vrm  SpecVersion=1 HumanBoneCount=%d ExpressionCount=2" % len(HUMAN_BONE_NODES))
