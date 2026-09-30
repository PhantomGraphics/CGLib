#version 450
#extension GL_GOOGLE_include_directive : require
#include "volume_common.glsl"

// Single-scattering raymarch over a density grid + sun-transmittance grid. Output is premultiplied
// (rgb = in-scattered radiance L, a = 1 - T) so it composites with ONE / ONE_MINUS_SRC_ALPHA.
layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0, std140) uniform Params {
    mat4 invViewProj;
    vec4 camPos;       // xyz
    vec4 gridOrigin;   // xyz origin, w cellSize
    vec4 gridDims;     // xyz nx,ny,nz
    vec4 sunDirIrr;    // xyz unit vector toward the sun, w sunIrradiance
    vec4 scatter;      // x extinction, y albedo, z phaseG, w ambient
    vec4 march;        // x stepLength, y maxSteps, z minTransmittance, w tMax (<=0: unlimited)
} u;
layout(set = 0, binding = 1) uniform sampler3D uDensity;   // border 0
layout(set = 0, binding = 2) uniform sampler3D uSunT;      // border 1

void main()
{
    vec4 nearP = u.invViewProj * vec4(vNdc, 0.0, 1.0);
    vec4 farP = u.invViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 origin = u.camPos.xyz;
    vec3 dir = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    vec3 extent = u.gridDims.xyz * u.gridOrigin.w;
    vec3 bmin = u.gridOrigin.xyz;
    float tn, tf;
    outColor = vec4(0.0);
    if (!intersectBox(origin, dir, bmin, bmin + extent, tn, tf)) return;
    if (u.march.w > 0.0) tf = min(tf, u.march.w);
    if (tf <= tn) return;

    float ds0 = u.march.x;
    float span = tf - tn;
    int wanted = max(1, int(ceil(span / ds0)));
    int n = min(wanted, int(u.march.y));
    float ds = span / float(wanted);
    float phase = henyeyGreenstein(dot(u.sunDirIrr.xyz, dir), u.scatter.z);

    float T = 1.0;
    vec3 L = vec3(0.0);
    for (int s = 0; s < n; ++s) {
        vec3 p = origin + dir * (tn + (float(s) + 0.5) * ds);
        vec3 uvw = (p - bmin) / extent;
        float sigma = u.scatter.x * texture(uDensity, uvw).r;
        if (sigma <= 0.0) continue;
        float stepT = exp(-sigma * ds);
        float src = u.scatter.y * (u.sunDirIrr.w * phase * texture(uSunT, uvw).r + u.scatter.w);
        L += vec3(T * (1.0 - stepT) * src);
        T *= stepT;
        if (T < u.march.z) break;
    }
    outColor = vec4(L, 1.0 - T);
}
