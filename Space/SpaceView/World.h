#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace VKSpace {

/// @brief Holds algorithm output buffers (wireframe lines and sample points).
struct SpaceResult {
    std::vector<float>    linePositions; ///< Interleaved x,y,z per vertex (3 floats).
    std::vector<float>    lineColors;    ///< Interleaved r,g,b,a per vertex (4 floats).
    std::vector<uint32_t> lineIndices;   ///< Index pairs for VK_PRIMITIVE_TOPOLOGY_LINE_LIST.

    std::vector<float>    pointPositions; ///< Interleaved x,y,z per point (3 floats).
    std::vector<float>    pointColors;    ///< Interleaved r,g,b,a per point (4 floats).
    std::vector<float>    pointSizes;     ///< Size per point (1 float).

    bool dirty = false;

    void addLine(const glm::vec3& a, const glm::vec3& b, const glm::vec4& color);
    void addBoxWireframe(const glm::vec3& mn,
                        const glm::vec3& mx,
                        const glm::vec4& color = {1.f, 1.f, 1.f, 1.f});
    void addPoint(const glm::vec3& pos,
                  const glm::vec4& color,
                  float size = 0.05f);

    void clear();
};

/// @brief Lightweight world that holds the current algorithm result.
///
/// No OpenGL dependency.  Panels write into getResult() and call markDirty()
/// so the renderer can upload new data to the GPU on the next frame.
class World {
public:
    SpaceResult& getResult() { return result_; }
    const SpaceResult& getResult() const { return result_; }

    void markDirty() { result_.dirty = true; }

private:
    SpaceResult result_;
};

} // namespace VKSpace
