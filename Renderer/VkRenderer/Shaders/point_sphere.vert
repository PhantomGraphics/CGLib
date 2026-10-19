#version 450

// VkPointBatchRenderer sphere mode: every point is a sphere of the point's local radius under the object's model matrix
// (an ellipsoid when the model scales non-uniformly). The vertex shader only places a screen-space sprite big enough to
// cover the sphere; point_sphere.frag ray-casts the exact surface and writes its depth.
layout(location = 0) in vec3  inPos;
layout(location = 1) in vec3  inColor;
layout(location = 2) in float inRadius;

layout(push_constant) uniform PC {
    mat4 mv;       // view * model
    vec4 tint;     // rgb: colour (uniform colour if w > 0.5, otherwise multiplies the per-point colour)
    vec4 proj;     // projection P[0][0], P[1][1] (Vulkan Y flip included), P[2][2], P[3][2]
    vec4 viewport; // x, y, width, height of the viewport in pixels
    vec4 params;   // x: largest scale of mv's 3x3 (bounds the sprite), y: radius scale, z: min point size (px),
                   // w > 0.5 => encode linear -> sRGB in the fragment shader
} pc;

layout(location = 0) out vec3 fragColor;
layout(location = 1) flat out vec3 viewCenter;
layout(location = 2) flat out float localRadius;

void main() {
    float r = inRadius * pc.params.y;
    vec4 pv = pc.mv * vec4(inPos, 1.0);
    float w = -pv.z;
    gl_Position = vec4(pc.proj.x * pv.x, pc.proj.y * pv.y, pc.proj.z * pv.z + pc.proj.w, w);
    if (!(r > 0.0) || !(w > 1.0e-4)) {
        gl_Position = vec4(0.0, 0.0, -2.0, 1.0); // radius 0 and points behind the eye are not drawn (clipped)
        gl_PointSize = 1.0;
    } else {
        float pixelsPerUnit = 0.5 * pc.viewport.w * abs(pc.proj.y);
        // 15 % + 2 px margin: the projection of an off-axis sphere is wider than the on-axis estimate.
        gl_PointSize = max(pc.params.z, 2.0 * r * pc.params.x * pixelsPerUnit / w * 1.15 + 2.0);
    }
    fragColor = pc.tint.w > 0.5 ? pc.tint.rgb : inColor * pc.tint.rgb;
    viewCenter = pv.xyz;
    localRadius = r;
}
