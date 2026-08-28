#version 450

// Depth-only shadow-caster pass. Mesh vertices are already world-space baked (see
// GlobalUBO::model comment in CameraUBO.h), so only the light's view-projection is needed.
layout(push_constant) uniform PushConstants {
    mat4 lightVP;
} pc;

layout(location = 0) in vec3 inPosition;

void main() {
    gl_Position = pc.lightVP * vec4(inPosition, 1.0);
}
