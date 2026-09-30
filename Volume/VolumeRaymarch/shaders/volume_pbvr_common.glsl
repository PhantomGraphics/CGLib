// Shared by the volume PBVR shaders (docs/todo/SPEC_volume_raymarch.md "PBVR との対応").
// Same medium as volume_raymarch.frag: sigma = kExt * rho, source S = albedo*(E*phase*Tsun + ambient).

layout(set = 0, binding = 0, std140) uniform PbvrParams {
    mat4 viewProj;     // cloud space -> clip
    vec4 camPos;       // xyz, in cloud space
    vec4 gridOrigin;   // xyz origin, w cellSize
    vec4 gridDims;     // xyz nx,ny,nz
    vec4 sunDirIrr;    // xyz unit vector toward the sun, w sunIrradiance
    vec4 scatter;      // x extinction, y albedo, z phaseG, w ambient
    vec4 pbvr;         // x pixelAngle [rad/pixel], y minDiameterPx, z maxPerCell, w projScalePx
    uvec4 limits;      // x particle capacity
} u;

struct Particle {
    vec4 posSize;      // xyz cloud-space position, w world diameter
    vec4 color;        // rgb = source radiance
};

// PCG hash: cheap, stateless, reproducible from (cell, ensemble, particle).
uint pcgHash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand01(inout uint seed)
{
    seed = pcgHash(seed);
    return float(seed >> 8) * (1.0 / 16777216.0);
}
