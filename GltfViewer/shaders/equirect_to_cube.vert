#version 450

// Identical to irradiance.vert (same cube-geometry vertex buffer, same view/proj push constant
// layout, driven by GltfIBLPrecomputer's shared renderCubeFaces()) -- kept as its own file
// rather than reused across modules because this project keeps each app's shaders self-contained
// (this pair is also duplicated verbatim in CGApp/Universe/shaders/, for the same reason).

layout(location = 0) in vec3 inPos;

layout(push_constant) uniform PC {
    mat4 view;
    mat4 proj;
} pc;

layout(location = 0) out vec3 outLocalPos;

void main() {
    outLocalPos = inPos;
    gl_Position = pc.proj * pc.view * vec4(inPos, 1.0);
}
