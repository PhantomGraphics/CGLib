#pragma once

#include "CGLib/Math/Matrix4d.h"
#include "CGLib/Math/Quaternion.h"
#include "CGLib/Math/Vector3d.h"

namespace Phantom::SceneRuntime {

// A node's local TRS. Kept as separate translation/rotation/scale (not baked into a single
// Matrix4df) because Blender/glTF export TRS components, and a future timeline (Phase 3's
// AnimationPlayerComponent) needs to animate them independently without decomposing a
// matrix back apart.
struct Transform {
    Phantom::Math::Vector3df translation{ 0.0f, 0.0f, 0.0f };
    Phantom::Math::Quaternion rotation{ 1.0f, 0.0f, 0.0f, 0.0f }; // glm::quat(w,x,y,z) -- identity
    Phantom::Math::Vector3df scale{ 1.0f, 1.0f, 1.0f };

    Phantom::Math::Matrix4df toMatrix() const;
};

bool operator==(const Transform& a, const Transform& b);
inline bool operator!=(const Transform& a, const Transform& b) { return !(a == b); }

} // namespace Phantom::SceneRuntime
