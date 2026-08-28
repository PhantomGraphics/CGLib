#version 450

layout(location = 0) in vec3 inPos;

// Only push view + roughness; proj is hardcoded (90 deg, 1:1) to stay < 128 bytes
layout(push_constant) uniform PC {
    mat4  view;        // 64 bytes
    float roughness;   // 4 bytes
    float _pad[3];     // 12 bytes  -> total = 80 bytes
} pc;

layout(location = 0) out vec3 outLocalPos;

void main() {
    const mat4 proj = mat4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, -1.00002, -1,
        0, 0, -0.200002, 0
    );
    outLocalPos = inPos;
    gl_Position = proj * pc.view * vec4(inPos, 1.0);
}
