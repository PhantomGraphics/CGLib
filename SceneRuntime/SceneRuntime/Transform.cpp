#include "Transform.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Phantom::SceneRuntime {

Phantom::Math::Matrix4df Transform::toMatrix() const
{
    // T * R * S, the same composition order used throughout CGLib/GltfRenderer
    // (e.g. GltfSceneRenderer::nodeLocalTransform()) so a node's local matrix means the
    // same thing here as it does everywhere else glTF-shaped TRS data flows.
    const glm::mat4 T = glm::translate(glm::mat4(1.0f), translation);
    const glm::mat4 R = glm::mat4_cast(rotation);
    const glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    return T * R * S;
}

bool operator==(const Transform& a, const Transform& b)
{
    return a.translation == b.translation && a.rotation == b.rotation && a.scale == b.scale;
}

} // namespace Phantom::SceneRuntime
