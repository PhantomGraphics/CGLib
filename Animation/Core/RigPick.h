#pragma once

#include "RigSkinning.h"

#include <cstdint>
#include <vector>

namespace Phantom::Animation {

// Screen-space targeting for weight editing (vertex picking, box selection and brush strokes). CPU only, no rendering dependency: the host passes
// its own camera matrices, so the module never needs to know which renderer draws the mesh.
//
// Weights belong to welded source vertices (RigMesh::sourceIndex); every function here indexes SOURCE vertices.
namespace RigPick {

// Screen state of every source vertex for one camera.
struct WeightView {
    std::vector<glm::vec2> pixel;    // viewport pixels (top-left based)
    std::vector<float>     depth;    // distance along the view direction (clip w)
    std::vector<uint8_t>   onScreen; // in front of the camera and inside the viewport
    std::vector<uint8_t>   visible;  // on screen and not hidden behind the mesh surface (= onScreen when `through`)
};

// The position of the first render vertex of each source vertex.
std::vector<glm::vec3> sourceOf(const RigMesh& mesh, const std::vector<glm::vec3>& perRender);

// Builds the view for `posedRender` (one rig-space position per RENDER vertex; pass the rest positions for the rest pose). `model` maps rig space
// to world space. Matrices are glm column-major, `proj` has the Vulkan Y flip convention of the host (pixel y = (ndc.y * 0.5 + 0.5) * height).
// Visibility comes from a coarse CPU depth buffer of the posed triangles; `through` skips it. A posedRender whose size differs from the mesh's
// render vertex count also skips it.
WeightView buildWeightView(const RigMesh& mesh, const std::vector<glm::vec3>& posedRender, const glm::mat4& model, const glm::mat4& view,
                           const glm::mat4& proj, float width, float height, bool through);

// The visible source vertex closest to a pixel (within maxPx), -1 when none. Within half a pixel the vertex nearer to the camera wins.
int nearestVisibleVertex(const WeightView& view, float x, float y, float maxPx = 30.f);

// Visible source vertices whose pixel lies in the rectangle (any corner order), ascending.
std::vector<uint32_t> visibleInRectangle(const WeightView& view, float x0, float y0, float x1, float y1);

// 8% of the rest mesh's bounding diagonal: the brush radius when none was chosen.
float autoBrushRadius(const RigMesh& mesh);

// Applies one dab per pixel. The dab centre is the visible vertex nearest to the pixel; the brush reaches the source vertices within `radius`
// (<= 0 = autoBrushRadius) of it measured on the REST mesh, and (unless `through`) only those that are visible. Pixels without a visible vertex
// are skipped. `affected` receives the number of samples applied. On failure `binding` may be partly changed: callers work on a copy.
RigResult brushStroke(RigBinding& binding, const AuthoredRig& rig, const RigMesh& mesh, const WeightView& view, BrushMode mode,
                      uint32_t boneId, float strength, float radius, bool through, const std::vector<glm::vec2>& pixels, int& affected);

} // namespace RigPick
} // namespace Phantom::Animation
