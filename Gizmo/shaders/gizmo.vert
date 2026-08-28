#version 450

layout(set = 0, binding = 0) uniform GizmoCameraUBO {
    mat4  view;
    mat4  proj;
    vec3  camPos; float _pad0;
    float vpWidth;
    float vpHeight;
    float _pad1;
    float _pad2;
} cam;

layout(push_constant) uniform PC {
    mat4 model;
    vec4 color;
} pc;

layout(location = 0) in vec3 inPos;

void main() {
    gl_Position = cam.proj * cam.view * pc.model * vec4(inPos, 1.0);
}
