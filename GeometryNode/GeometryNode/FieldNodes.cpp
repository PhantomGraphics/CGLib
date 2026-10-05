#include "FieldNodes.h"

#include <algorithm>
#include <cmath>

#include "GeometryOps.h"

namespace Phantom::GeometryNode {

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

SocketDef required(SocketDef s) {
    s.required = true;
    return s;
}

template <class T>
void fit(std::vector<T>& v, size_t n) {
    v.resize(n);  // a field that produced the wrong number of values is padded/truncated; consumers check sizes first
}

// Float-field operand: the linked field, else the scalar broadcast.
std::vector<float> operand(const FieldPtr& f, float scalar, const FieldContext& ctx) {
    const size_t n = ctx.size();
    if (ctx.stopped()) return {};
    if (!f) return std::vector<float>(n, scalar);
    std::vector<float> v = evaluateFloats(f, ctx);
    if (ctx.stopped()) return {};
    fit(v, n);
    return v;
}

std::vector<float> optionalFloats(const FieldPtr& f, const FieldContext& ctx) {
    std::vector<float> v = f ? evaluateFloats(f, ctx) : std::vector<float>();
    if (ctx.stopped()) return {};
    fit(v, ctx.size());
    return v;
}

std::vector<Vec3> vectorsOf(const FieldPtr& f, const FieldContext& ctx) {
    std::vector<Vec3> v = evaluateVectors(f, ctx);
    if (ctx.stopped()) return {};
    fit(v, ctx.size());
    return v;
}

void addMath(NodeRegistry& r, const char* type, const char* name, float defaultValue, bool compare,
             float (*op)(float, float)) {
    NodeDefinition d;
    d.typeId = type;
    d.displayName = name;
    d.category = "Math";
    d.description = compare
        ? "Compares field A with field B (or the scalar Value) and returns a Bool field -- a Selection."
        : "Combines field A with field B (or the scalar Value when B is not linked), element by element.";
    d.inputs = {required(sock("A", "A", SocketType::FieldFloat)), sock("B", "B", SocketType::FieldFloat, {}, "Optional; overrides Value."),
                sock("Value", "Value", SocketType::Float, defaultValue, "Used when B is not linked.")};
    d.outputs = {sock("Result", "Result", compare ? SocketType::FieldBool : SocketType::FieldFloat)};
    d.evaluate = [compare, op](NodeEvalContext& c) {
        const FieldPtr a = c.getField("A"), b = c.getField("B");
        const float scalar = c.getFloat("Value");
        if (compare) {
            c.setOutput("Result", Value(makeBoolField([a, b, scalar, op](const FieldContext& ctx, std::vector<uint8_t>& out) {
                const auto x = operand(a, 0.0f, ctx);
                const auto y = operand(b, scalar, ctx);
                if (ctx.stopped()) return;
                out.resize(x.size());
                for (size_t i = 0; i < x.size() && !ctx.stopped(); ++i) out[i] = op(x[i], y[i]) != 0.0f ? 1 : 0;
            })));
        } else {
            c.setOutput("Result", Value(makeFloatField([a, b, scalar, op](const FieldContext& ctx, std::vector<float>& out) {
                const auto x = operand(a, 0.0f, ctx);
                const auto y = operand(b, scalar, ctx);
                if (ctx.stopped()) return;
                out.resize(x.size());
                for (size_t i = 0; i < x.size() && !ctx.stopped(); ++i) out[i] = op(x[i], y[i]);
            })));
        }
    };
    r.add(std::move(d));
}

// Replaces `v` by the geometry the node works on; reports a field/size mismatch.
bool sizeIs(NodeEvalContext& c, size_t got, size_t want, const char* socket) {
    if (got == want) return true;
    c.fail(DiagCode::FieldMismatch, std::string("field '") + socket + "' produced " + std::to_string(got) + " values for " +
                                        std::to_string(want) + " points", socket);
    return false;
}

bool fieldFailed(NodeEvalContext& c, const FieldContext& ctx) {
    if (!ctx.stopped()) return false;
    const bool cancelled = ctx.evaluation->cancelled;
    c.fail(cancelled ? DiagCode::Cancelled : DiagCode::LimitExceeded,
           cancelled ? "field evaluation cancelled" : "field workspace or expression depth limit exceeded");
    return true;
}

FieldContext consumerContext(NodeEvalContext& c, const Mesh& mesh, FieldEvaluation& state) {
    FieldContext ctx{&mesh, c.seed(), Domain::Point, &state, c.cancelFlag()};
    // Reserve space for the input, output mesh and the consumer's target/offset/normal arrays.
    const uint64_t meshBytes = estimateMeshBytes(mesh.positions.size(), mesh.indices.size());
    const uint64_t arrays = mesh.positions.size() * sizeof(Vec3) * 3;
    const uint64_t budget = c.limits().maxMemoryBytes;
    if (arrays > budget || meshBytes > (budget - arrays) / 2) {
        ctx.maxMemoryBytes = 0;
        state.limitExceeded = true;
    } else ctx.maxMemoryBytes = budget - arrays - meshBytes * 2;
    return ctx;
}

}  // namespace

void registerFieldNodes(NodeRegistry& r) {
    {
        NodeDefinition d;
        d.typeId = "Position";
        d.displayName = "Position";
        d.category = "Field";
        d.description = "The position of every point of the geometry it is evaluated on.";
        d.outputs = {sock("Position", "Position", SocketType::FieldVector3)};
        d.evaluate = [](NodeEvalContext& c) {
            c.setOutput("Position", Value(makeVectorField([](const FieldContext& ctx, std::vector<Vec3>& out) {
                if (ctx.mesh) out = ctx.mesh->positions;
            })));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "Index";
        d.displayName = "Index";
        d.category = "Field";
        d.description = "The index (0, 1, 2, ...) of every point.";
        d.outputs = {sock("Index", "Index", SocketType::FieldFloat)};
        d.evaluate = [](NodeEvalContext& c) {
            c.setOutput("Index", Value(makeFloatField([](const FieldContext& ctx, std::vector<float>& out) {
                out.resize(ctx.size());
                for (size_t i = 0; i < out.size() && !ctx.stopped(); ++i) out[i] = static_cast<float>(i);
            })));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "RandomValue";
        d.displayName = "Random Value";
        d.category = "Field";
        d.description = "A reproducible random float in [Min, Max) per point: a pure function of the graph seed, "
                        "this node's Seed and the point index (see Field.h).";
        d.inputs = {sock("Min", "Min", SocketType::Float, 0.0f), sock("Max", "Max", SocketType::Float, 1.0f),
                    sock("Seed", "Seed", SocketType::Int, int32_t(0), "Different seeds give independent values.")};
        d.outputs = {sock("Value", "Value", SocketType::FieldFloat)};
        d.evaluate = [](NodeEvalContext& c) {
            const float lo = c.getFloat("Min"), hi = c.getFloat("Max");
            const int32_t seed = c.getInt("Seed");
            c.setOutput("Value", Value(makeFloatField([lo, hi, seed](const FieldContext& ctx, std::vector<float>& out) {
                out.resize(ctx.size());
                for (size_t i = 0; i < out.size() && !ctx.stopped(); ++i)
                    out[i] = lo + (hi - lo) * randomUnit(ctx.seed, seed, static_cast<uint32_t>(i));
            })));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "FloatField";
        d.displayName = "Float Field";
        d.category = "Field";
        d.description = "The explicit conversion of one float into a field (the same value on every point).";
        d.inputs = {sock("Value", "Value", SocketType::Float, 0.0f)};
        d.outputs = {sock("Value", "Value", SocketType::FieldFloat)};
        d.evaluate = [](NodeEvalContext& c) {
            const float v = c.getFloat("Value");
            c.setOutput("Value", Value(makeFloatField([v](const FieldContext& ctx, std::vector<float>& out) { out.assign(ctx.size(), v); })));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "VectorField";
        d.displayName = "Vector Field";
        d.category = "Field";
        d.description = "The explicit conversion of one Vector3 into a field (the same vector on every point).";
        d.inputs = {sock("Vector", "Vector", SocketType::Vector3, Vec3{0, 0, 0})};
        d.outputs = {sock("Vector", "Vector", SocketType::FieldVector3)};
        d.evaluate = [](NodeEvalContext& c) {
            const Vec3 v = c.getVec3("Vector");
            c.setOutput("Vector", Value(makeVectorField([v](const FieldContext& ctx, std::vector<Vec3>& out) { out.assign(ctx.size(), v); })));
        };
        r.add(std::move(d));
    }

    addMath(r, "MathAdd", "Add", 0.0f, false, [](float a, float b) { return a + b; });
    addMath(r, "MathSubtract", "Subtract", 0.0f, false, [](float a, float b) { return a - b; });
    addMath(r, "MathMultiply", "Multiply", 1.0f, false, [](float a, float b) { return a * b; });
    addMath(r, "MathDivide", "Divide", 1.0f, false, [](float a, float b) { return b == 0.0f ? 0.0f : a / b; });
    addMath(r, "MathMinimum", "Minimum", 0.0f, false, [](float a, float b) { return std::min(a, b); });
    addMath(r, "MathMaximum", "Maximum", 0.0f, false, [](float a, float b) { return std::max(a, b); });
    addMath(r, "MathGreaterThan", "Greater Than", 0.5f, true, [](float a, float b) { return a > b ? 1.0f : 0.0f; });
    addMath(r, "MathLessThan", "Less Than", 0.5f, true, [](float a, float b) { return a < b ? 1.0f : 0.0f; });

    {
        NodeDefinition d;
        d.typeId = "CombineXYZField";
        d.displayName = "Combine XYZ (Field)";
        d.category = "Field";
        d.description = "Builds a Vector3 field from three float fields; a component that is not linked is 0.";
        d.inputs = {sock("X", "X", SocketType::FieldFloat), sock("Y", "Y", SocketType::FieldFloat), sock("Z", "Z", SocketType::FieldFloat)};
        d.outputs = {sock("Vector", "Vector", SocketType::FieldVector3)};
        d.evaluate = [](NodeEvalContext& c) {
            const FieldPtr x = c.getField("X"), y = c.getField("Y"), z = c.getField("Z");
            c.setOutput("Vector", Value(makeVectorField([x, y, z](const FieldContext& ctx, std::vector<Vec3>& out) {
                const auto fx = optionalFloats(x, ctx), fy = optionalFloats(y, ctx), fz = optionalFloats(z, ctx);
                if (ctx.stopped()) return;
                out.resize(fx.size());
                for (size_t i = 0; i < out.size() && !ctx.stopped(); ++i) out[i] = Vec3{fx[i], fy[i], fz[i]};
            })));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "SeparateXYZField";
        d.displayName = "Separate XYZ (Field)";
        d.category = "Field";
        d.description = "Splits a Vector3 field into its X, Y and Z float fields.";
        d.inputs = {required(sock("Vector", "Vector", SocketType::FieldVector3))};
        d.outputs = {sock("X", "X", SocketType::FieldFloat), sock("Y", "Y", SocketType::FieldFloat), sock("Z", "Z", SocketType::FieldFloat)};
        d.evaluate = [](NodeEvalContext& c) {
            const FieldPtr v = c.getField("Vector");
            auto component = [v](float Vec3::*member) {
                return makeFloatField([v, member](const FieldContext& ctx, std::vector<float>& out) {
                    const auto vec = vectorsOf(v, ctx);
                    if (ctx.stopped()) return;
                    out.resize(vec.size());
                    for (size_t i = 0; i < vec.size() && !ctx.stopped(); ++i) out[i] = vec[i].*member;
                });
            };
            c.setOutput("X", Value(component(&Vec3::x)));
            c.setOutput("Y", Value(component(&Vec3::y)));
            c.setOutput("Z", Value(component(&Vec3::z)));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "VectorAdd";
        d.displayName = "Vector Add";
        d.category = "Field";
        d.description = "A + B on Vector3 fields.";
        d.inputs = {required(sock("A", "A", SocketType::FieldVector3)), required(sock("B", "B", SocketType::FieldVector3))};
        d.outputs = {sock("Result", "Result", SocketType::FieldVector3)};
        d.evaluate = [](NodeEvalContext& c) {
            const FieldPtr a = c.getField("A"), b = c.getField("B");
            c.setOutput("Result", Value(makeVectorField([a, b](const FieldContext& ctx, std::vector<Vec3>& out) {
                const auto x = vectorsOf(a, ctx), y = vectorsOf(b, ctx);
                if (ctx.stopped()) return;
                out.resize(x.size());
                for (size_t i = 0; i < out.size() && !ctx.stopped(); ++i) out[i] = Vec3{x[i].x + y[i].x, x[i].y + y[i].y, x[i].z + y[i].z};
            })));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "VectorScale";
        d.displayName = "Vector Scale";
        d.category = "Field";
        d.description = "Multiplies a Vector3 field by a float field (or the scalar Value when Scale is not linked).";
        d.inputs = {required(sock("Vector", "Vector", SocketType::FieldVector3)), sock("Scale", "Scale", SocketType::FieldFloat),
                    sock("Value", "Value", SocketType::Float, 1.0f, "Used when Scale is not linked.")};
        d.outputs = {sock("Result", "Result", SocketType::FieldVector3)};
        d.evaluate = [](NodeEvalContext& c) {
            const FieldPtr v = c.getField("Vector"), s = c.getField("Scale");
            const float scalar = c.getFloat("Value");
            c.setOutput("Result", Value(makeVectorField([v, s, scalar](const FieldContext& ctx, std::vector<Vec3>& out) {
                const auto vec = vectorsOf(v, ctx);
                const auto k = operand(s, scalar, ctx);
                if (ctx.stopped()) return;
                out.resize(vec.size());
                for (size_t i = 0; i < out.size() && !ctx.stopped(); ++i) out[i] = Vec3{vec[i].x * k[i], vec[i].y * k[i], vec[i].z * k[i]};
            })));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "SetPosition";
        d.displayName = "Set Position";
        d.category = "Geometry";
        d.description = "Moves the selected points: new = (Position field, else the current position) + Offset. "
                        "Selection defaults to every point. Normals are recomputed from the new positions.";
        SocketDef geometry = sock("Geometry", "Geometry", SocketType::Geometry, {}, "Geometry to deform.");
        geometry.required = true;
        d.inputs = {geometry, sock("Selection", "Selection", SocketType::FieldBool, {}, "Optional; default: all points."),
                    sock("Position", "Position", SocketType::FieldVector3, {}, "Optional absolute positions."),
                    sock("Offset", "Offset", SocketType::FieldVector3, {}, "Optional displacement.")};
        d.outputs = {sock("Geometry", "Geometry", SocketType::Geometry)};
        d.evaluate = [](NodeEvalContext& c) {
            const GeometryPtr in = c.getGeometry("Geometry");
            if (!in) {
                c.fail(DiagCode::MissingInput, "Set Position: no geometry input", "Geometry");
                return;
            }
            const size_t n = in->positions.size();
            FieldEvaluation state;
            const FieldContext ctx = consumerContext(c, *in, state);
            if (fieldFailed(c, ctx)) return;
            std::vector<uint8_t> selected(n, 1);
            if (const FieldPtr f = c.getField("Selection")) {
                selected = evaluateBools(f, ctx);
                if (fieldFailed(c, ctx)) return;
                if (!sizeIs(c, selected.size(), n, "Selection")) return;
            }
            std::vector<Vec3> target = in->positions;
            if (const FieldPtr f = c.getField("Position")) {
                target = evaluateVectors(f, ctx);
                if (fieldFailed(c, ctx)) return;
                if (!sizeIs(c, target.size(), n, "Position")) return;
            }
            std::vector<Vec3> offset;
            if (const FieldPtr f = c.getField("Offset")) {
                offset = evaluateVectors(f, ctx);
                if (fieldFailed(c, ctx)) return;
                if (!sizeIs(c, offset.size(), n, "Offset")) return;
            }
            Mesh m = *in;
            for (size_t i = 0; i < n; ++i) {
                if ((i & 1023) == 0 && fieldFailed(c, ctx)) return;
                if (!selected[i]) continue;
                Vec3 p = target[i];
                if (!offset.empty()) p = Vec3{p.x + offset[i].x, p.y + offset[i].y, p.z + offset[i].z};
                m.positions[i] = p;
            }
            if (!m.normals.empty()) m.normals = computeSmoothNormals(m);
            c.setOutput("Geometry", GeometryPtr(std::make_shared<const Mesh>(std::move(m))));
        };
        r.add(std::move(d));
    }
    {
        NodeDefinition d;
        d.typeId = "DeleteGeometry";
        d.displayName = "Delete Geometry";
        d.category = "Geometry";
        d.description = "Removes the selected points and every triangle that uses one of them.";
        SocketDef geometry = sock("Geometry", "Geometry", SocketType::Geometry);
        geometry.required = true;
        d.inputs = {geometry, required(sock("Selection", "Selection", SocketType::FieldBool, {}, "True = delete."))};
        d.outputs = {sock("Geometry", "Geometry", SocketType::Geometry)};
        d.evaluate = [](NodeEvalContext& c) {
            const GeometryPtr in = c.getGeometry("Geometry");
            if (!in) {
                c.fail(DiagCode::MissingInput, "Delete Geometry: no geometry input", "Geometry");
                return;
            }
            const size_t n = in->positions.size();
            FieldEvaluation state;
            const FieldContext ctx = consumerContext(c, *in, state);
            if (fieldFailed(c, ctx)) return;
            const std::vector<uint8_t> del = evaluateBools(c.getField("Selection"), ctx);
            if (fieldFailed(c, ctx)) return;
            if (!sizeIs(c, del.size(), n, "Selection")) return;
            std::vector<uint32_t> remap(n, 0xFFFFFFFFu);
            Mesh m;
            for (size_t i = 0; i < n; ++i) {
                if ((i & 1023) == 0 && fieldFailed(c, ctx)) return;
                if (del[i]) continue;
                remap[i] = static_cast<uint32_t>(m.positions.size());
                m.positions.push_back(in->positions[i]);
                if (!in->normals.empty()) m.normals.push_back(in->normals[i]);
                if (!in->uvs.empty()) m.uvs.push_back(in->uvs[i]);
            }
            for (size_t t = 0; t + 2 < in->indices.size(); t += 3) {
                if ((t & 1023) == 0 && fieldFailed(c, ctx)) return;
                const uint32_t a = remap[in->indices[t]], b = remap[in->indices[t + 1]], d2 = remap[in->indices[t + 2]];
                if (a == 0xFFFFFFFFu || b == 0xFFFFFFFFu || d2 == 0xFFFFFFFFu) continue;
                m.indices.insert(m.indices.end(), {a, b, d2});
            }
            c.setOutput("Geometry", GeometryPtr(std::make_shared<const Mesh>(std::move(m))));
        };
        r.add(std::move(d));
    }
}

}  // namespace Phantom::GeometryNode
