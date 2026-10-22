#include "MeshWorld.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <unordered_map>
#include <glm/gtc/matrix_transform.hpp>
#include "CGLib/Math/Core/Matrix4d.h"
#include "CGLib/Math/Core/Vector4d.h"

using namespace Phantom::MeshEdit;

namespace Phantom::MeshView {

namespace {

constexpr size_t kMaxUndo = 50;

glm::vec3 pos(const EditableVertex& v) { return { v.pos[0], v.pos[1], v.pos[2] }; }

// Face normal by Newell's method; zero vector for a face with a missing vertex.
glm::vec3 faceNormal(const EditableMesh& m, const EditableFace& f) {
    float n[3];
    const float len = newellNormal(m, f, n);
    if (len <= 1e-12f) return glm::vec3(0.f);
    return glm::vec3(n[0], n[1], n[2]) / len;
}

// Area-weighted average normal of the faces around every vertex (used to lift lines and points off the surface).
std::unordered_map<uint32_t, glm::vec3> vertexNormals(const EditableMesh& m) {
    std::unordered_map<uint32_t, glm::vec3> sum;
    for (const auto& f : m.faces) {
        float n[3];
        if (newellNormal(m, f, n) <= 1e-12f) continue;
        for (const auto& c : f.loop) sum[c.vertex] += glm::vec3(n[0], n[1], n[2]);
    }
    for (auto& [id, n] : sum) {
        const float len = glm::length(n);
        n = len > 1e-12f ? n / len : glm::vec3(0.f, 1.f, 0.f);
    }
    return sum;
}

// Lit grey with a little ambient; the viewer has no lights, this just makes the shape readable.
glm::vec3 litColor(const glm::vec3& base, const glm::vec3& normal) {
    static const glm::vec3 light = glm::normalize(glm::vec3(0.5f, 1.0f, 0.7f));
    const float k = 0.38f + 0.62f * std::max(glm::dot(normal, light), 0.f);
    return base * k;
}

void addColor(std::vector<float>& out, const glm::vec3& c, float a = 1.f) {
    out.push_back(c.r); out.push_back(c.g); out.push_back(c.b); out.push_back(a);
}
void addPos(std::vector<float>& out, const glm::vec3& p) { out.push_back(p.x); out.push_back(p.y); out.push_back(p.z); }

float extent(const EditableMesh& m) {
    if (m.vertices.empty()) return 1.f;
    glm::vec3 lo(1e30f), hi(-1e30f);
    for (const auto& v : m.vertices) { lo = glm::min(lo, pos(v)); hi = glm::max(hi, pos(v)); }
    return std::max(glm::length(hi - lo), 1e-3f);
}

bool contains(const std::vector<uint32_t>& ids, uint32_t id) { return std::binary_search(ids.begin(), ids.end(), id); }

}  // namespace

MeshWorld::MeshWorld() { newMesh(Shape::Box, 1); }

// ------------------------------------------------------------------------------------------------ mesh

void MeshWorld::newMesh(Shape shape, int divs) {
    EditableMesh m = shape == Shape::Box ? makeEditableBox(0.5f) : makeEditablePlane(1.f, std::max(divs, 1));
    mesh_ = std::make_shared<const EditableMesh>(std::move(m));
    modifiers_.clear();
    edit_ = MeshEditState{};
    cutter_ = Cutter{};
    cutter_.mesh = std::make_shared<const EditableMesh>(makeEditableBox());
    cutterPreview_.reset();
    undo_.clear();
    redo_.clear();
    invalidate();
}

Counts MeshWorld::baseCounts() const { return { mesh_->vertices.size(), mesh_->edges.size(), mesh_->faces.size() }; }

Counts MeshWorld::evaluatedCounts() const {
    EditableMesh e;
    if (!evaluate(e, nullptr)) return baseCounts();
    return { e.vertices.size(), e.edges.size(), e.faces.size() };
}

std::string MeshWorld::describe() const {
    const Counts b = baseCounts(), e = evaluatedCounts();
    std::ostringstream s;
    s << b.vertices << " verts / " << b.edges << " edges / " << b.faces << " faces";
    if (!modifiers_.empty()) s << "  ->  " << e.vertices << " / " << e.edges << " / " << e.faces << " (" << modifiers_.size() << " modifier(s))";
    return s.str();
}

bool MeshWorld::evaluate(EditableMesh& out, std::string* error) const {
    if (modifiers_.empty()) { out = *mesh_; return true; }
    return evaluateModifiers(*mesh_, modifiers_, out, error);
}

// ------------------------------------------------------------------------------------------------ history

void MeshWorld::pushUndo() {
    undo_.push_back({mesh_, modifiers_, cutter_});
    if (undo_.size() > kMaxUndo) undo_.erase(undo_.begin());
    redo_.clear();
}

void MeshWorld::commit(EditableMesh next) {
    pushUndo();
    mesh_ = std::make_shared<const EditableMesh>(std::move(next));
    pruneSelection();
    invalidate();
}

void MeshWorld::pruneSelection() {
    edit_.ids.erase(std::remove_if(edit_.ids.begin(), edit_.ids.end(),
                                   [&](uint32_t id) { return !hasElement(*mesh_, edit_.kind, id); }),
                    edit_.ids.end());
}

bool MeshWorld::undo() {
    if (undo_.empty()) return false;
    redo_.push_back({mesh_, modifiers_, cutter_});
    mesh_ = undo_.back().mesh;
    modifiers_ = undo_.back().modifiers;
    cutter_ = undo_.back().cutter;
    cutterPreview_.reset();
    undo_.pop_back();
    pruneSelection();
    invalidate();
    return true;
}

bool MeshWorld::redo() {
    if (redo_.empty()) return false;
    undo_.push_back({mesh_, modifiers_, cutter_});
    mesh_ = redo_.back().mesh;
    modifiers_ = redo_.back().modifiers;
    cutter_ = redo_.back().cutter;
    cutterPreview_.reset();
    redo_.pop_back();
    pruneSelection();
    invalidate();
    return true;
}

// ------------------------------------------------------------------------------------------------ edit mode / selection

bool MeshWorld::requireEditMode(std::string& error) const {
    if (edit_.editMode) return true;
    error = "not in edit mode (SetMode:edit)";
    return false;
}

std::string MeshWorld::setEditMode(bool on) {
    cutterPreview_.reset();
    edit_.editMode = on;
    if (!on) edit_.ids.clear();
    invalidate();
    return {};
}

std::string MeshWorld::setElementKind(ElementKind kind) {
    edit_.applySelection(kind, {}, SelectMode::Replace, false, false);
    invalidate();
    return {};
}

std::string MeshWorld::select(const std::string& ids, SelectMode mode) {
    std::string error;
    if (!requireEditMode(error)) return error;
    ElementIdRequest request;
    MeshArgError argError;
    if (!parseElementIds(ids, *mesh_, edit_.kind, request, argError)) return argError.message;
    edit_.applySelection(edit_.kind, request.ids, mode, request.clear, true);
    invalidate();
    return {};
}

std::string MeshWorld::pick(const MeshPickView& view, double x, double y, SelectMode mode) {
    std::string error;
    if (!requireEditMode(error)) return error;
    MeshPickOptions options;
    options.kind = edit_.kind;
    options.xray = edit_.xray;
    std::vector<uint32_t> hit;
    uint32_t id = 0;
    if (pickMeshElement(*mesh_, view, x, y, options, id)) hit.push_back(id);
    // Clicking empty space clears a replace selection; add / toggle leave it alone.
    edit_.applySelection(edit_.kind, hit, mode, hit.empty() && mode == SelectMode::Replace, true);
    invalidate();
    return {};
}

std::string MeshWorld::boxSelect(const MeshPickView& view, double x0, double y0, double x1, double y1, SelectMode mode) {
    std::string error;
    if (!requireEditMode(error)) return error;
    MeshPickOptions options;
    options.kind = edit_.kind;
    options.xray = edit_.xray;
    const auto hit = boxSelectMeshElements(*mesh_, view, x0, y0, x1, y1, options);
    edit_.applySelection(edit_.kind, hit, mode, false, true);
    invalidate();
    return {};
}

// ------------------------------------------------------------------------------------------------ operations

std::string MeshWorld::extrude(float distance) {
    std::string error;
    if (!requireEditMode(error)) return error;
    if (edit_.kind != ElementKind::Face) return "extrude works on faces (SelectElement:face)";
    if (edit_.ids.empty()) return "no faces are selected";
    EditableMesh out;
    if (!extrudeFaces(*mesh_, edit_.ids, distance, out, &error)) return error;
    commit(std::move(out));
    return {};
}

std::string MeshWorld::inset(float thickness, float depth) {
    std::string error;
    if (!requireEditMode(error)) return error;
    if (edit_.kind != ElementKind::Face) return "inset works on faces (SelectElement:face)";
    if (edit_.ids.empty()) return "no faces are selected";
    EditableMesh out;
    if (!insetFaces(*mesh_, edit_.ids, thickness, depth, out, &error)) return error;
    commit(std::move(out));
    return {};
}

std::string MeshWorld::bevel(float width) {
    std::string error;
    if (!requireEditMode(error)) return error;
    if (edit_.kind != ElementKind::Edge) return "bevel works on edges (SelectElement:edge)";
    if (edit_.ids.empty()) return "no edges are selected";
    EditableMesh out;
    std::vector<uint32_t> faces;
    if (!bevelEdges(*mesh_, edit_.ids, width, out, &error, &faces)) return error;
    commit(std::move(out));
    edit_.kind = ElementKind::Face;       // the old edges are gone: select the new bevel faces
    edit_.ids = faces;
    std::sort(edit_.ids.begin(), edit_.ids.end());
    return {};
}

std::string MeshWorld::loopCut(float factor) {
    std::string error;
    if (!requireEditMode(error)) return error;
    if (edit_.kind != ElementKind::Edge) return "loop cut works on an edge (SelectElement:edge)";
    if (edit_.ids.size() != 1) return "loop cut needs exactly one selected edge";
    EditableMesh out;
    std::vector<uint32_t> created;
    if (!Phantom::MeshEdit::loopCut(*mesh_, edit_.ids[0], factor, out, &error, &created)) return error;
    commit(std::move(out));
    edit_.kind = ElementKind::Vertex;     // select the new loop, slide it with Move
    edit_.ids = created;
    std::sort(edit_.ids.begin(), edit_.ids.end());
    return {};
}

std::string MeshWorld::shade(bool smooth, float angleDeg) {
    std::string error;
    if (!requireEditMode(error)) return error;
    if (edit_.kind != ElementKind::Face) return "shading works on faces (SelectElement:face)";
    if (edit_.ids.empty()) return "no faces are selected";
    EditableMesh out;
    size_t changed = 0;
    if (!shadeFaces(*mesh_, edit_.ids, smooth, angleDeg, out, &error, &changed)) return error;
    if (changed == 0) return {};          // nothing changes: no history entry
    commit(std::move(out));
    return {};
}

std::string MeshWorld::merge(MergeTarget target) {
    std::string error;
    if (!requireEditMode(error)) return error;
    if (edit_.ids.empty()) return "nothing is selected";
    std::vector<uint32_t> vertices;
    if (!elementsToVertices(*mesh_, edit_.kind, edit_.ids, vertices, &error)) return error;
    if (vertices.size() < 2) return "merge needs at least two vertices";
    EditableMesh out;
    if (!mergeVertices(*mesh_, vertices, target, out, &error)) return error;
    commit(std::move(out));
    edit_.kind = ElementKind::Vertex;     // the merged vertices are one now
    edit_.ids = { vertices.front() };
    pruneSelection();
    return {};
}

std::string MeshWorld::deleteSelected() {
    std::string error;
    if (!requireEditMode(error)) return error;
    if (edit_.ids.empty()) return "nothing is selected";
    EditableMesh out;
    if (!deleteElements(*mesh_, edit_.kind, edit_.ids, out, &error)) return error;
    commit(std::move(out));
    edit_.ids.clear();
    return {};
}

std::string MeshWorld::moveSelected(const std::array<float, 3>& delta) {
    std::string error;
    if (!requireEditMode(error)) return error;
    if (edit_.ids.empty()) return "nothing is selected";
    std::vector<uint32_t> vertices;
    if (!elementsToVertices(*mesh_, edit_.kind, edit_.ids, vertices, &error)) return error;
    EditableMesh out;
    if (!translateVertices(*mesh_, vertices, delta, out, &error)) return error;
    commit(std::move(out));
    return {};
}

// ------------------------------------------------------------------------------------------------ modifiers

std::string MeshWorld::addModifier(const MeshModifier& modifier) {
    std::vector<MeshModifier> next = modifiers_;
    next.push_back(modifier);
    EditableMesh probe;
    std::string error;
    if (!evaluateModifiers(*mesh_, next, probe, &error)) return error;
    pushUndo();
    modifiers_ = std::move(next);
    invalidate();
    return {};
}

std::string MeshWorld::clearModifiers() {
    if (modifiers_.empty()) return {};
    pushUndo();
    modifiers_.clear();
    invalidate();
    return {};
}

std::string MeshWorld::applyModifiers() {
    if (modifiers_.empty()) return "there are no modifiers";
    EditableMesh out;
    std::string error;
    if (!evaluateModifiers(*mesh_, modifiers_, out, &error)) return error;
    pushUndo();
    mesh_ = std::make_shared<const EditableMesh>(std::move(out));
    modifiers_.clear();
    edit_.ids.clear();                    // the baked mesh has new ids
    invalidate();
    return {};
}

// ------------------------------------------------------------------------------------------------ Boolean

Math::Vector3df MeshWorld::cutterPoint(const EditableVertex& vertex, const CutterTransform& t) const {
    Math::Matrix4dd r(1.0);
    r = glm::rotate(r, glm::radians(double(t.rotation[1])), Math::Vector3dd(0, 1, 0));
    r = glm::rotate(r, glm::radians(double(t.rotation[0])), Math::Vector3dd(1, 0, 0));
    r = glm::rotate(r, glm::radians(double(t.rotation[2])), Math::Vector3dd(0, 0, 1));
    const Math::Vector3dd p(double(vertex.pos[0]) * t.scale[0], double(vertex.pos[1]) * t.scale[1],
                            double(vertex.pos[2]) * t.scale[2]);
    return Math::Vector3df(Math::Vector3dd(r * Math::Vector4dd(p, 0.0)) +
                           Math::Vector3dd(t.position[0], t.position[1], t.position[2]));
}

std::string MeshWorld::setCutter(bool copyEvaluated) {
    if (edit_.editMode)
        return "cutter setup requires object mode";
    EditableMesh next;
    std::string error;
    if (copyEvaluated) {
        if (!evaluate(next, &error))
            return error;
    } else
        next = makeEditableBox();
    pushUndo();
    cutter_.mesh = std::make_shared<const EditableMesh>(std::move(next));
    cutter_.transform = CutterTransform{};
    cutter_.copied = copyEvaluated;
    cutter_.visible = true;
    cutterPreview_.reset();
    invalidate();
    return {};
}

std::string MeshWorld::setCutterTransform(const CutterTransform& t, bool preview) {
    if (edit_.editMode)
        return "cutter setup requires object mode";
    for (int k = 0; k < 3; ++k) {
        if (!std::isfinite(t.position[k]) || !std::isfinite(t.rotation[k]) || !std::isfinite(t.scale[k]) ||
            t.scale[k] <= 0.f)
            return "cutter transform requires finite values and positive scale";
    }
    // Reject overflow before it can reach the preview draw buffers.
    for (const auto& v : cutter_.mesh->vertices) {
        const auto p = cutterPoint(v, t);
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return "cutter transform overflows";
    }
    if (preview)
        cutterPreview_ = t;
    else {
        if (!(t == cutter_.transform)) {
            pushUndo();
            cutter_.transform = t;
        }
        cutterPreview_.reset();
    }
    invalidate();
    return {};
}

void MeshWorld::setCutterVisible(bool visible) {
    if (visible == cutter_.visible)
        return;
    pushUndo();
    cutter_.visible = visible;
    invalidate();
}

std::string MeshWorld::applyBoolean(BooleanOperation operation) {
    if (edit_.editMode)
        return "Boolean requires object mode (SetMode:object)";
    EditableMesh a, b = *cutter_.mesh, result;
    std::string error;
    if (!evaluate(a, &error))
        return error;
    for (auto& v : b.vertices) {
        const auto p = cutterPoint(v, cutter_.transform);
        v.pos[0] = p.x;
        v.pos[1] = p.y;
        v.pos[2] = p.z;
    }
    // Transform stored corner normals by inverse scale followed by rotation.
    Math::Matrix4dd normalRotation(1.0);
    const auto& transform = cutter_.transform;
    normalRotation =
        glm::rotate(normalRotation, glm::radians(double(transform.rotation[1])), Math::Vector3dd(0, 1, 0));
    normalRotation =
        glm::rotate(normalRotation, glm::radians(double(transform.rotation[0])), Math::Vector3dd(1, 0, 0));
    normalRotation =
        glm::rotate(normalRotation, glm::radians(double(transform.rotation[2])), Math::Vector3dd(0, 0, 1));
    for (auto& f : b.faces)
        for (auto& c : f.loop)
            if (c.hasNormal) {
                Math::Vector3dd n(double(c.normal[0]) / transform.scale[0],
                                  double(c.normal[1]) / transform.scale[1],
                                  double(c.normal[2]) / transform.scale[2]);
                n = Math::Vector3dd(normalRotation * Math::Vector4dd(n, 0.0));
                const double len = glm::length(n);
                if (std::isfinite(len) && len > 0.0) {
                    n /= len;
                    for (int k = 0; k < 3; ++k)
                        c.normal[k] = static_cast<float>(n[k]);
                } else
                    c.hasNormal = false;
            }
    // Modifier evaluation may allocate its own id range. Preserve both ranges.
    a.nextVertexId = std::max(a.nextVertexId, mesh_->nextVertexId);
    a.nextFaceId = std::max(a.nextFaceId, mesh_->nextFaceId);
    a.nextEdgeId = std::max(a.nextEdgeId, mesh_->nextEdgeId);
    if (!booleanMeshes(a, b, operation, result, {}, &error))
        return error;
    pushUndo();
    mesh_ = std::make_shared<const EditableMesh>(std::move(result));
    modifiers_.clear();
    edit_.ids.clear();
    cutterPreview_.reset();
    invalidate();
    return {};
}

std::string MeshWorld::reduce(const MeshReductionOptions& options, MeshReductionReport& report) {
    if (edit_.editMode) return "Reduction requires object mode (SetMode:object)";
    EditableMesh input, output;
    std::string error;
    if (!evaluate(input, &error)) return error;
    MeshReductionReport next;
    if (!reduceMesh(input, options, output, next, &error)) return error;
    // A protected/no-op result must not bake modifiers or discard the redo branch.
    if (next.changed) {
        pushUndo();
        mesh_ = std::make_shared<const EditableMesh>(std::move(output));
        modifiers_.clear();
        edit_.ids.clear();
        cutterPreview_.reset();
        invalidate();
    }
    lastReduction_ = next;
    report = next;
    return {};
}

// ------------------------------------------------------------------------------------------------ display / draw data

void MeshWorld::setDisplay(const Display& d) {
    display_ = d;
    invalidate();
}

void MeshWorld::setEye(const glm::vec3& eye) {
    if (eye == eye_) return;
    eye_ = eye;
    if (display_.orientation) invalidate();   // only the orientation colours depend on the camera
}

MeshDraw& MeshWorld::draw() {
    if (stale_) rebuild();
    return draw_;
}

Facing MeshWorld::facing(const glm::vec3& eye) const {
    EditableMesh e;
    if (!evaluate(e, nullptr)) e = *mesh_;
    Facing result;
    for (const auto& f : e.faces) {
        if (f.loop.size() < 3) continue;
        const glm::vec3 a = pos(*e.findVertex(f.loop[0].vertex));
        for (size_t k = 1; k + 1 < f.loop.size(); ++k) {
            const glm::vec3 b = pos(*e.findVertex(f.loop[k].vertex));
            const glm::vec3 c = pos(*e.findVertex(f.loop[k + 1].vertex));
            const glm::vec3 n = glm::cross(b - a, c - a);
            if (glm::length(n) < 1e-12f) continue;
            (glm::dot(n, eye - (a + b + c) / 3.f) > 0.f ? result.front : result.back)++;
        }
    }
    return result;
}

double MeshWorld::signedVolume(bool* closed) const {
    EditableMesh e;
    if (!evaluate(e, nullptr)) e = *mesh_;
    if (closed) {
        std::map<std::pair<uint32_t, uint32_t>, int> uses;
        for (const auto& f : e.faces)
            for (size_t i = 0; i < f.loop.size(); ++i) {
                const uint32_t a = f.loop[i].vertex, b = f.loop[(i + 1) % f.loop.size()].vertex;
                ++uses[{ std::min(a, b), std::max(a, b) }];
            }
        bool all = !uses.empty();
        for (const auto& [edge, n] : uses) all = all && n == 2;
        *closed = all;
    }
    // Divergence theorem: a closed surface wound counter-clockwise from outside encloses a positive volume.
    double volume = 0.0;
    for (const auto& f : e.faces) {
        if (f.loop.size() < 3) continue;
        const glm::dvec3 a = glm::dvec3(pos(*e.findVertex(f.loop[0].vertex)));
        for (size_t k = 1; k + 1 < f.loop.size(); ++k) {
            const glm::dvec3 b = glm::dvec3(pos(*e.findVertex(f.loop[k].vertex)));
            const glm::dvec3 c = glm::dvec3(pos(*e.findVertex(f.loop[k + 1].vertex)));
            volume += glm::dot(a, glm::cross(b, c)) / 6.0;
        }
    }
    return volume;
}

void MeshWorld::rebuild() {
    stale_ = false;
    MeshDraw d;
    EditableMesh evaluated;
    std::string error;
    if (!evaluate(evaluated, &error)) evaluated = *mesh_;
    const bool modified = !modifiers_.empty();
    const EditableMesh& base = *mesh_;
    const float lift = 0.0025f * extent(evaluated);

    const glm::vec3 faceColor(0.72f, 0.74f, 0.78f);
    const glm::vec3 selectColor(1.0f, 0.62f, 0.12f);
    const glm::vec3 frontColor(0.15f, 0.40f, 1.0f), backColor(1.0f, 0.20f, 0.15f);
    const bool faceSelection = edit_.editMode && edit_.kind == ElementKind::Face && !modified;

    // ---- faces ----
    if (display_.faces || display_.orientation) {
        for (const auto& f : evaluated.faces) {
            if (f.loop.size() < 3) continue;
            const glm::vec3 fn = faceNormal(evaluated, f);
            if (fn == glm::vec3(0.f)) continue;
            const bool selected = faceSelection && contains(edit_.ids, f.id);
            const uint32_t first = static_cast<uint32_t>(d.triPositions.size() / 3);
            for (const auto& c : f.loop) {
                const glm::vec3 p = pos(*evaluated.findVertex(c.vertex));
                const glm::vec3 n = c.hasNormal ? glm::normalize(glm::vec3(c.normal[0], c.normal[1], c.normal[2])) : fn;
                addPos(d.triPositions, p);
                glm::vec3 color = selected ? litColor(selectColor, n) : litColor(faceColor, n);
                addColor(d.triColors, color);
            }
            for (uint32_t k = 1; k + 1 < f.loop.size(); ++k) {
                d.triIndices.push_back(first);
                d.triIndices.push_back(first + k);
                d.triIndices.push_back(first + k + 1);
            }
            if (display_.orientation) {
                // The renderer's front face: the triangle's cross product points at the eye. Flat per triangle, so a
                // quad is recoloured corner by corner from its first fan triangle.
                const glm::vec3 a = pos(*evaluated.findVertex(f.loop[0].vertex));
                const glm::vec3 b = pos(*evaluated.findVertex(f.loop[1].vertex));
                const glm::vec3 c2 = pos(*evaluated.findVertex(f.loop[2].vertex));
                const bool front = glm::dot(glm::cross(b - a, c2 - a), eye_ - (a + b + c2) / 3.f) > 0.f;
                const glm::vec3 lit = litColor(front ? frontColor : backColor, fn);
                for (size_t k = 0; k < f.loop.size(); ++k)
                    for (int ch = 0; ch < 3; ++ch) d.triColors[(first + k) * 4 + ch] = lit[ch];
            }
        }
    }

    // ---- edges ----
    const auto addLine = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& color) {
        const uint32_t i = static_cast<uint32_t>(d.linePositions.size() / 3);
        addPos(d.linePositions, a); addPos(d.linePositions, b);
        addColor(d.lineColors, color); addColor(d.lineColors, color);
        d.lineIndices.push_back(i); d.lineIndices.push_back(i + 1);
    };
    if (cutter_.visible && cutter_.mesh) {
        const auto& t = cutterPreview_ ? *cutterPreview_ : cutter_.transform;
        for (const auto& e : cutter_.mesh->edges) {
            const auto* a = cutter_.mesh->findVertex(e.a);
            const auto* b = cutter_.mesh->findVertex(e.b);
            if (a && b)
                addLine(cutterPoint(*a, t), cutterPoint(*b, t), Math::Vector3df(0.95f, 0.25f, 0.8f));
        }
    }
    std::set<uint32_t> selectedEdges;           // base edge ids drawn highlighted
    if (edit_.editMode && !modified) {
        if (edit_.kind == ElementKind::Edge) selectedEdges.insert(edit_.ids.begin(), edit_.ids.end());
        else if (edit_.kind == ElementKind::Face)
            for (const auto& f : base.faces)
                if (contains(edit_.ids, f.id))
                    for (size_t i = 0; i < f.loop.size(); ++i)
                        if (const EditableEdge* e = base.findEdge(f.loop[i].vertex, f.loop[(i + 1) % f.loop.size()].vertex))
                            selectedEdges.insert(e->id);
    }
    if (display_.edges) {
        const auto evalNormals = vertexNormals(evaluated);
        for (const auto& e : evaluated.edges) {
            const EditableVertex* a = evaluated.findVertex(e.a);
            const EditableVertex* b = evaluated.findVertex(e.b);
            if (!a || !b) continue;
            const bool sel = !modified && selectedEdges.count(e.id) != 0;
            glm::vec3 color = sel ? selectColor : (modified ? glm::vec3(0.30f, 0.32f, 0.36f) : glm::vec3(0.12f, 0.13f, 0.15f));
            const glm::vec3 na = evalNormals.count(e.a) ? evalNormals.at(e.a) : glm::vec3(0.f);
            const glm::vec3 nb = evalNormals.count(e.b) ? evalNormals.at(e.b) : glm::vec3(0.f);
            addLine(pos(*a) + na * lift, pos(*b) + nb * lift, color);
        }
        // The editing cage on top of a modified result.
        if (modified && edit_.editMode) {
            const auto baseNormals = vertexNormals(base);
            for (const auto& e : base.edges) {
                const EditableVertex* a = base.findVertex(e.a);
                const EditableVertex* b = base.findVertex(e.b);
                if (!a || !b) continue;
                const bool sel = edit_.kind == ElementKind::Edge && contains(edit_.ids, e.id);
                const glm::vec3 na = baseNormals.count(e.a) ? baseNormals.at(e.a) : glm::vec3(0.f);
                const glm::vec3 nb = baseNormals.count(e.b) ? baseNormals.at(e.b) : glm::vec3(0.f);
                addLine(pos(*a) + na * lift * 2.f, pos(*b) + nb * lift * 2.f, sel ? selectColor : glm::vec3(0.25f, 0.75f, 0.95f));
            }
        }
    }
    if (display_.normals) {
        for (const auto& f : evaluated.faces) {
            const glm::vec3 fn = faceNormal(evaluated, f);
            if (fn == glm::vec3(0.f)) continue;
            glm::vec3 centre(0.f);
            for (const auto& c : f.loop) centre += pos(*evaluated.findVertex(c.vertex));
            centre /= static_cast<float>(f.loop.size());
            addLine(centre, centre + fn * (0.12f * extent(evaluated)), glm::vec3(0.2f, 0.9f, 0.3f));
        }
    }

    // ---- vertices (edit mode) ----
    if (edit_.editMode && display_.vertices) {
        const auto baseNormals = vertexNormals(base);
        std::set<uint32_t> selectedVertices;
        if (edit_.kind == ElementKind::Vertex) selectedVertices.insert(edit_.ids.begin(), edit_.ids.end());
        else {
            std::vector<uint32_t> vs;
            std::string ignored;
            if (!edit_.ids.empty() && elementsToVertices(base, edit_.kind, edit_.ids, vs, &ignored))
                selectedVertices.insert(vs.begin(), vs.end());
        }
        for (const auto& v : base.vertices) {
            const bool sel = selectedVertices.count(v.id) != 0;
            const glm::vec3 n = baseNormals.count(v.id) ? baseNormals.at(v.id) : glm::vec3(0.f);
            addPos(d.pointPositions, pos(v) + n * lift * 3.f);
            addColor(d.pointColors, sel ? selectColor : glm::vec3(0.1f, 0.1f, 0.12f));
            d.pointSizes.push_back(sel ? 11.f : 7.f);
        }
    }

    d.dirty = true;
    draw_ = std::move(d);
}

}  // namespace MeshView
