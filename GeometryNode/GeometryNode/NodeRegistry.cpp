#include "NodeRegistry.h"

#include "FieldNodes.h"
#include "GeometryOps.h"

namespace Phantom::GeometryNode {

// --- NodeEvalContext ---------------------------------------------------------

const Value* NodeEvalContext::first(const std::string& id) const {
    auto it = inputs.find(id);
    if (it == inputs.end() || it->second.empty()) return nullptr;
    return &it->second.front();
}

float NodeEvalContext::getFloat(const std::string& id) const {
    const Value* v = first(id);
    if (v) {
        if (const float* f = std::get_if<float>(v)) return *f;
    }
    return 0.0f;
}

int32_t NodeEvalContext::getInt(const std::string& id) const {
    const Value* v = first(id);
    if (v) {
        if (const int32_t* i = std::get_if<int32_t>(v)) return *i;
    }
    return 0;
}

bool NodeEvalContext::getBool(const std::string& id) const {
    const Value* v = first(id);
    if (v) {
        if (const bool* b = std::get_if<bool>(v)) return *b;
    }
    return false;
}

Vec3 NodeEvalContext::getVec3(const std::string& id) const {
    const Value* v = first(id);
    if (v) {
        if (const Vec3* p = std::get_if<Vec3>(v)) return *p;
    }
    return Vec3{};
}

GeometryPtr NodeEvalContext::getGeometry(const std::string& id) const {
    const Value* v = first(id);
    if (v) {
        if (const GeometryPtr* g = std::get_if<GeometryPtr>(v)) return *g;
    }
    return nullptr;
}

FieldPtr NodeEvalContext::getField(const std::string& id) const {
    const Value* v = first(id);
    if (v) {
        if (const FieldPtr* f = std::get_if<FieldPtr>(v)) return *f;
    }
    return nullptr;
}

std::vector<GeometryPtr> NodeEvalContext::getGeometries(const std::string& id) const {
    std::vector<GeometryPtr> out;
    auto it = inputs.find(id);
    if (it == inputs.end()) return out;
    for (const Value& v : it->second) {
        if (const GeometryPtr* g = std::get_if<GeometryPtr>(&v)) out.push_back(*g);
    }
    return out;
}

void NodeEvalContext::fail(DiagCode code, std::string message, std::string socket) {
    failed_ = true;
    Diagnostic d;
    d.severity = Severity::Error;
    d.code = code;
    d.node = node_;
    d.socket = std::move(socket);
    d.message = std::move(message);
    diagnostics_.push_back(std::move(d));
}

// --- NodeDefinition / NodeRegistry --------------------------------------------

const SocketDef* NodeDefinition::findInput(const std::string& id) const {
    for (const SocketDef& s : inputs)
        if (s.id == id) return &s;
    return nullptr;
}

const SocketDef* NodeDefinition::findOutput(const std::string& id) const {
    for (const SocketDef& s : outputs)
        if (s.id == id) return &s;
    return nullptr;
}

bool NodeRegistry::add(NodeDefinition def) {
    if (!def.evaluate || def.typeId.empty() || find(def.typeId)) return false;
    defs_.push_back(std::move(def));
    return true;
}

const NodeDefinition* NodeRegistry::find(const std::string& typeId) const {
    for (const NodeDefinition& d : defs_)
        if (d.typeId == typeId) return &d;
    return nullptr;
}

// --- Built-in nodes -------------------------------------------------------------

namespace {

SocketDef sock(const char* id, const char* name, SocketType type, Value def = {}, const char* desc = "") {
    SocketDef s;
    s.id = id;
    s.displayName = name;
    s.type = type;
    s.defaultValue = std::move(def);
    s.description = desc;
    return s;
}

SocketDef geomIn(const char* id, const char* name, bool multi, const char* desc) {
    SocketDef s = sock(id, name, SocketType::Geometry, {}, desc);
    s.required = !multi;
    s.multi = multi;
    return s;
}

// Maps a geometry-op status to a node failure. Returns true when ok.
bool noInstances(NodeEvalContext& c, const GeometryPtr& g, const char* what) {
    if (!g || g->instances.empty()) return true;
    c.fail(DiagCode::InvalidGeometry, std::string(what) + ": geometry has unrealized instances; add Realize Instances first", "Geometry");
    return false;
}

bool check(NodeEvalContext& c, OpStatus s, const char* what) {
    switch (s) {
        case OpStatus::Ok: return true;
        case OpStatus::InvalidArgument: c.fail(DiagCode::InvalidParameter, std::string(what) + ": invalid parameter value"); break;
        case OpStatus::LimitExceeded: c.fail(DiagCode::LimitExceeded, std::string(what) + ": result exceeds the resource limits"); break;
        case OpStatus::SingularTransform: c.fail(DiagCode::SingularTransform, std::string(what) + ": singular transform (zero scale)"); break;
        case OpStatus::InvalidMesh: c.fail(DiagCode::InvalidGeometry, std::string(what) + ": invalid or non-finite geometry"); break;
    }
    return false;
}

NodeRegistry makeBuiltin() {
    NodeRegistry r;

    {
        NodeDefinition d;
        d.typeId = "Box";
        d.displayName = "Box";
        d.category = "Mesh Primitives";
        d.description = "Axis-aligned box centred on the origin (24 vertices, flat normals).";
        d.inputs = {sock("Size", "Size", SocketType::Vector3, Vec3{1, 1, 1}, "Extent along X, Y, Z (>= 0).")};
        d.outputs = {sock("Geometry", "Geometry", SocketType::Geometry)};
        d.evaluate = [](NodeEvalContext& c) {
            Mesh m;
            if (!check(c, makeBox(c.getVec3("Size"), c.limits(), m), "Box")) return;
            c.setOutput("Geometry", GeometryPtr(std::make_shared<const Mesh>(std::move(m))));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "Grid";
        d.displayName = "Grid";
        d.category = "Mesh Primitives";
        d.description = "Plane on XZ, +Y up, centred on the origin.";
        d.inputs = {sock("SizeX", "Size X", SocketType::Float, 1.0f), sock("SizeZ", "Size Z", SocketType::Float, 1.0f),
                    sock("VerticesX", "Vertices X", SocketType::Int, int32_t(3), "Vertex count along X (>= 2)."),
                    sock("VerticesZ", "Vertices Z", SocketType::Int, int32_t(3), "Vertex count along Z (>= 2).")};
        d.outputs = {sock("Geometry", "Geometry", SocketType::Geometry)};
        d.evaluate = [](NodeEvalContext& c) {
            Mesh m;
            if (!check(c, makeGrid(c.getFloat("SizeX"), c.getFloat("SizeZ"), c.getInt("VerticesX"),
                                   c.getInt("VerticesZ"), c.limits(), m), "Grid")) return;
            c.setOutput("Geometry", GeometryPtr(std::make_shared<const Mesh>(std::move(m))));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "Terrain";
        d.displayName = "Terrain";
        d.category = "Mesh Primitives";
        d.description = "Height-field terrain on XZ (+Y up, centred): seeded gradient-noise fBm, normalised by the "
                        "amplitude sum so Height Scale keeps its meaning. Reproduces the former Terrain generator v1.";
        d.inputs = {sock("Width", "Width", SocketType::Float, 10.0f, "X extent (> 0)."),
                    sock("Depth", "Depth", SocketType::Float, 10.0f, "Z extent (> 0)."),
                    sock("SegmentsX", "Segments X", SocketType::Int, int32_t(128), "Quad columns (1..4096)."),
                    sock("SegmentsZ", "Segments Z", SocketType::Int, int32_t(128), "Quad rows (1..4096)."),
                    sock("HeightScale", "Height Scale", SocketType::Float, 2.0f, "fBm amplitude (>= 0)."),
                    sock("Frequency", "Frequency", SocketType::Float, 0.15f, "Base spatial frequency (> 0)."),
                    sock("Octaves", "Octaves", SocketType::Int, int32_t(5), "fBm octaves (1..12)."),
                    sock("Lacunarity", "Lacunarity", SocketType::Float, 2.0f, "Per-octave frequency multiplier (> 1)."),
                    sock("Persistence", "Persistence", SocketType::Float, 0.5f, "Per-octave amplitude multiplier (0..1)."),
                    sock("Seed", "Seed", SocketType::Int, int32_t(0), "Noise permutation seed (a 32-bit seed is stored as its int32 bit pattern)."),
                    sock("HeightOffset", "Height Offset", SocketType::Float, 0.0f, "Added to every height.")};
        d.outputs = {sock("Geometry", "Geometry", SocketType::Geometry)};
        d.evaluate = [](NodeEvalContext& c) {
            TerrainParams p;
            p.width = c.getFloat("Width");
            p.depth = c.getFloat("Depth");
            p.segmentsX = c.getInt("SegmentsX");
            p.segmentsZ = c.getInt("SegmentsZ");
            p.heightScale = c.getFloat("HeightScale");
            p.frequency = c.getFloat("Frequency");
            p.octaves = c.getInt("Octaves");
            p.lacunarity = c.getFloat("Lacunarity");
            p.persistence = c.getFloat("Persistence");
            p.seed = c.getInt("Seed");
            p.heightOffset = c.getFloat("HeightOffset");
            Mesh m;
            if (!check(c, makeTerrain(p, c.limits(), m), "Terrain")) return;
            c.setOutput("Geometry", GeometryPtr(std::make_shared<const Mesh>(std::move(m))));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "TransformGeometry";
        d.displayName = "Transform Geometry";
        d.category = "Geometry";
        d.description = "p' = T * R * S * p. Rotation is Euler XYZ in degrees.";
        d.inputs = {geomIn("Geometry", "Geometry", false, "Geometry to transform."),
                    sock("Translation", "Translation", SocketType::Vector3, Vec3{0, 0, 0}),
                    sock("Rotation", "Rotation", SocketType::Vector3, Vec3{0, 0, 0}, "Degrees, Euler XYZ."),
                    sock("Scale", "Scale", SocketType::Vector3, Vec3{1, 1, 1}, "Negative values mirror; zero is rejected.")};
        d.outputs = {sock("Geometry", "Geometry", SocketType::Geometry)};
        d.evaluate = [](NodeEvalContext& c) {
            const GeometryPtr in = c.getGeometry("Geometry");
            if (!in) {
                c.fail(DiagCode::MissingInput, "Transform Geometry: no geometry input", "Geometry");
                return;
            }
            if (!noInstances(c, in, "Transform Geometry")) return;
            Mesh m;
            if (!check(c, transformMesh(*in, c.getVec3("Translation"), c.getVec3("Rotation"), c.getVec3("Scale"), m),
                       "Transform Geometry")) return;
            c.setOutput("Geometry", GeometryPtr(std::make_shared<const Mesh>(std::move(m))));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "JoinGeometry";
        d.displayName = "Join Geometry";
        d.category = "Geometry";
        d.description = "Concatenates every linked geometry in link order. No input yields an empty mesh.";
        d.inputs = {geomIn("Geometry", "Geometry", true, "Any number of geometries.")};
        d.outputs = {sock("Geometry", "Geometry", SocketType::Geometry)};
        d.evaluate = [](NodeEvalContext& c) {
            const std::vector<GeometryPtr> inputs = c.getGeometries("Geometry");
            for (const GeometryPtr& g : inputs)
                if (!noInstances(c, g, "Join Geometry")) return;
            Mesh m;
            if (!check(c, joinMeshes(inputs, c.limits(), m), "Join Geometry")) return;
            c.setOutput("Geometry", GeometryPtr(std::make_shared<const Mesh>(std::move(m))));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "FloatValue";
        d.displayName = "Float";
        d.category = "Input";
        d.description = "A constant float.";
        d.inputs = {sock("Value", "Value", SocketType::Float, 0.0f)};
        d.outputs = {sock("Value", "Value", SocketType::Float)};
        d.evaluate = [](NodeEvalContext& c) { c.setOutput("Value", c.getFloat("Value")); };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "CombineXYZ";
        d.displayName = "Combine XYZ";
        d.category = "Input";
        d.description = "Builds a Vector3 from three floats (the explicit Float -> Vector3 conversion).";
        d.inputs = {sock("X", "X", SocketType::Float, 0.0f), sock("Y", "Y", SocketType::Float, 0.0f),
                    sock("Z", "Z", SocketType::Float, 0.0f)};
        d.outputs = {sock("Vector", "Vector", SocketType::Vector3)};
        d.evaluate = [](NodeEvalContext& c) {
            c.setOutput("Vector", Vec3{c.getFloat("X"), c.getFloat("Y"), c.getFloat("Z")});
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "Output";
        d.displayName = "Output";
        d.category = "Output";
        d.description = "The graph result. Exactly one per graph.";
        d.inputs = {geomIn("Geometry", "Geometry", false, "Final geometry (object-local).")};
        d.isOutput = true;
        d.evaluate = [](NodeEvalContext& c) {
            const GeometryPtr in = c.getGeometry("Geometry");
            if (!in) {
                c.fail(DiagCode::MissingInput, "Output: no geometry input", "Geometry");
                return;
            }
            if (!noInstances(c, in, "Output")) return;
            c.setOutput("Geometry", in);
        };
        r.add(std::move(d));
    }
    registerFieldNodes(r);
    return r;
}

}  // namespace

const NodeRegistry& NodeRegistry::builtin() {
    static const NodeRegistry registry = makeBuiltin();
    return registry;
}

}  // namespace Phantom::GeometryNode
