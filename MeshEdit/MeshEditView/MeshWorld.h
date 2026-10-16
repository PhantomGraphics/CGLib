#pragma once

// MeshWorld: the state behind MeshEditView -- one EditableMesh with its modifier stack, the edit-session selection, an undo
// history, and the CPU-built draw data (faces / edges / vertices) the renderer uploads. No ImGui and no Vulkan, so
// MeshEditTest can exercise everything the viewer does without a window. Every editing call returns an empty string on
// success and the reason on failure; a failed call changes nothing.

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "CGLib/Math/Core/Vector3d.h"

#include "../Core/MeshEditPicking.h"
#include "../Core/MeshEditState.h"
#include "../Core/MeshBoolean.h"

#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Phantom::MeshView {

enum class Shape { Box, Plane };

struct CutterTransform {
    std::array<float, 3> position{0.35f, 0.2f, 0.f};
    std::array<float, 3> rotation{0.f, 0.f, 0.f};
    std::array<float, 3> scale{1.f, 1.f, 1.f};
    bool operator==(const CutterTransform&) const = default;
};

struct Cutter {
    std::shared_ptr<const Phantom::MeshEdit::EditableMesh> mesh;
    CutterTransform transform;
    bool visible = false;
    bool copied = false;
};

struct Display {
    bool faces = true;         // shaded faces
    bool edges = true;         // wireframe
    bool vertices = true;      // points (edit mode)
    bool normals = false;      // a line along each face normal
    bool orientation = false;  // front faces (seen from the camera, counter-clockwise) blue, back faces red
};

// What the renderer uploads. `dirty` is set whenever any array changes.
struct MeshDraw {
    std::vector<float> triPositions, triColors;     // 3 / 4 floats per vertex (one vertex per corner, flat colour)
    std::vector<uint32_t> triIndices;
    std::vector<float> linePositions, lineColors;
    std::vector<uint32_t> lineIndices;
    std::vector<float> pointPositions, pointColors, pointSizes;
    bool dirty = true;
};

struct Counts {
    size_t vertices = 0, edges = 0, faces = 0;
};

// Triangles of the evaluated mesh seen from `eye`: front = counter-clockwise from the eye (the renderer's front face).
struct Facing {
    size_t front = 0, back = 0;
};

class MeshWorld {
public:
    MeshWorld();

    // ---- the mesh ----
    void newMesh(Shape shape, int divs);
    const Phantom::MeshEdit::EditableMesh& base() const { return *mesh_; }
    Counts baseCounts() const;
    Counts evaluatedCounts() const;     // after the modifiers (equals baseCounts() without any)
    const std::vector<Phantom::MeshEdit::MeshModifier>& modifiers() const { return modifiers_; }
    std::string describe() const;       // one line for the UI

    // ---- edit mode and selection ----
    const Phantom::MeshEdit::MeshEditState& edit() const { return edit_; }
    bool editMode() const { return edit_.editMode; }
    std::string setEditMode(bool on);
    std::string setElementKind(Phantom::MeshEdit::ElementKind kind);
    // ids: "0,1,4-6" | "all" | "none" in the current element kind.
    std::string select(const std::string& ids, Phantom::MeshEdit::SelectMode mode);
    std::string pick(const Phantom::MeshEdit::MeshPickView& view, double x, double y, Phantom::MeshEdit::SelectMode mode);
    std::string boxSelect(const Phantom::MeshEdit::MeshPickView& view, double x0, double y0, double x1, double y1,
                          Phantom::MeshEdit::SelectMode mode);
    void setXray(bool on) { edit_.xray = on; }

    // ---- shape operations on the selection (edit mode) ----
    std::string extrude(float distance);
    std::string inset(float thickness, float depth);
    std::string bevel(float width);
    std::string loopCut(float factor);
    std::string shade(bool smooth, float angleDeg);
    std::string merge(Phantom::MeshEdit::MergeTarget target);
    std::string deleteSelected();
    std::string moveSelected(const std::array<float, 3>& delta);

    // ---- modifiers ----
    std::string addModifier(const Phantom::MeshEdit::MeshModifier& modifier);
    std::string clearModifiers();
    std::string applyModifiers();       // bakes the stack into the base mesh

    // ---- Boolean cutter (object mode; previews affect only draw data) ----
    const Cutter& cutter() const { return cutter_; }
    std::string setCutter(bool copyEvaluated);
    std::string setCutterTransform(const CutterTransform& transform, bool preview = false);
    void setCutterVisible(bool visible);
    std::string applyBoolean(Phantom::MeshEdit::BooleanOperation operation);

    // ---- history ----
    bool undo();
    bool redo();
    size_t undoDepth() const { return undo_.size(); }
    size_t redoDepth() const { return redo_.size(); }

    // ---- display and draw data ----
    const Display& display() const { return display_; }
    void setDisplay(const Display& d);
    // The camera position the orientation view colours against; ignored while the orientation view is off.
    void setEye(const glm::vec3& eye);
    MeshDraw& draw();                   // rebuilds when stale
    Facing facing(const glm::vec3& eye) const;

    // Signed volume of the evaluated mesh by the divergence theorem: positive for a closed surface wound counter-clockwise
    // from outside (the library's contract), negative when it is inside out. `closed` (optional) tells whether every edge
    // is shared by exactly two faces; an open surface (a plane) has no meaningful volume.
    double signedVolume(bool* closed = nullptr) const;

private:
    struct Snapshot {
        std::shared_ptr<const Phantom::MeshEdit::EditableMesh> mesh;
        std::vector<Phantom::MeshEdit::MeshModifier> modifiers;
        Cutter cutter;
    };

    // Installs `next` as the base mesh (one undo step) and drops selection that no longer exists.
    void commit(Phantom::MeshEdit::EditableMesh next);
    void pushUndo();
    void pruneSelection();
    void invalidate() { stale_ = true; }
    bool requireEditMode(std::string& error) const;
    bool evaluate(Phantom::MeshEdit::EditableMesh& out, std::string* error) const;
    void rebuild();
    Phantom::Math::Vector3df cutterPoint(const Phantom::MeshEdit::EditableVertex& vertex,
                                         const CutterTransform& transform) const;

    std::shared_ptr<const Phantom::MeshEdit::EditableMesh> mesh_;
    std::vector<Phantom::MeshEdit::MeshModifier> modifiers_;
    Phantom::MeshEdit::MeshEditState edit_;
    std::vector<Snapshot> undo_, redo_;
    Display display_;
    Cutter cutter_;
    std::optional<CutterTransform> cutterPreview_;
    glm::vec3 eye_{ 0.f, 0.f, 5.f };
    MeshDraw draw_;
    bool stale_ = true;
};

}  // namespace MeshView
