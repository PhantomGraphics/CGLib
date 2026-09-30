#version 450
#extension GL_GOOGLE_include_directive : require
#include "volume_pbvr_common.glsl"

layout(set = 0, binding = 3, std430) readonly buffer Particles { Particle particles[]; };

layout(location = 0) out vec3 vColor;

void main()
{
    Particle p = particles[gl_VertexIndex];
    gl_Position = u.viewProj * vec4(p.posSize.xyz, 1.0);
    // World diameter -> pixels at this particle's own depth (clip.w).
    gl_PointSize = max(p.posSize.w * u.pbvr.w / max(gl_Position.w, 1.0e-6), 1.0);
    vColor = p.color.rgb;
}
