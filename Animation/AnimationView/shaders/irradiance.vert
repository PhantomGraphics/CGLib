#version 450

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
