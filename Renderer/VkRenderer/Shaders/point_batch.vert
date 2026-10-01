#version 450

// VkPointBatchRenderer: world-radius point sprites, one draw per batch (instance).
layout(location = 0) in vec3  inPos;
layout(location = 1) in vec3  inColor;
layout(location = 2) in float inRadius;

layout(push_constant) uniform PC {
    mat4 mvp;      // viewProj * model
    vec4 tint;     // rgb: uniform colour, w > 0.5 => use it instead of the per-point colour
    vec4 params;   // x: pixels per world unit at clip w = 1, y: radius scale, z: min point size (px),
                   // w > 0.5 => encode linear -> sRGB in the fragment shader
} pc;

layout(location = 0) out vec3 fragColor;
layout(location = 1) flat out float fragSrgb;

void main() {
    float r = inRadius * pc.params.y;
    vec4 p = pc.mvp * vec4(inPos, 1.0);
    gl_Position = p;
    if (!(r > 0.0)) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0); // radius 0 is not drawn (clipped)
        gl_PointSize = 1.0;
    } else {
        gl_PointSize = max(pc.params.z, 2.0 * r * pc.params.x / max(p.w, 1.0e-4));
    }
    fragColor = pc.tint.w > 0.5 ? pc.tint.rgb : inColor;
    fragSrgb  = pc.params.w;
}
