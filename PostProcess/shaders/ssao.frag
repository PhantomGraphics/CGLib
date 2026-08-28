#version 450

// Hemisphere-sampling SSAO (Crysis / LearnOpenGL style). Reconstructs view-space position
// from a linear-depth input + the projection matrix (valid for symmetric perspective
// frustums, i.e. no projection skew), rather than requiring a full inverse-projection
// matrix or a dedicated view-space-position G-buffer target.

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outAO;

#define MAX_KERNEL 32

layout(binding = 0) uniform UBO {
    mat4  proj;
    vec4  kernel[MAX_KERNEL];
    vec2  noiseScale;
    int   kernelSize;
    float radius;
    float bias;
} ubo;

layout(binding = 1) uniform sampler2D uDepth;  // linear view-space depth (positive distance)
layout(binding = 2) uniform sampler2D uNormal; // view-space normal, xyz in rgb
layout(binding = 3) uniform sampler2D uNoise;  // tangent-space rotation vector, xy

vec3 reconstructViewPos(vec2 uv, float linearDepth)
{
    vec2 ndc = uv * 2.0 - 1.0;
    float viewX = ndc.x * linearDepth / ubo.proj[0][0];
    float viewY = ndc.y * linearDepth / ubo.proj[1][1];
    return vec3(viewX, viewY, -linearDepth);
}

void main()
{
    float depth = texture(uDepth, inUV).r;
    if (depth <= 0.0) { outAO = vec4(1.0); return; }

    vec3 fragPos    = reconstructViewPos(inUV, depth);
    vec3 normal     = normalize(texture(uNormal, inUV).xyz);
    vec3 randomVec  = normalize(vec3(texture(uNoise, inUV * ubo.noiseScale).xy, 0.0));

    vec3 tangent   = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN       = mat3(tangent, bitangent, normal);

    int n = max(ubo.kernelSize, 1);
    float occlusion = 0.0;
    for (int i = 0; i < n; ++i) {
        vec3 samplePos = fragPos + (TBN * ubo.kernel[i].xyz) * ubo.radius;

        vec4 offset = ubo.proj * vec4(samplePos, 1.0);
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;

        float sampleDepth  = texture(uDepth, offset.xy).r;
        float sampleViewZ  = -sampleDepth;
        float rangeCheck   = smoothstep(0.0, 1.0, ubo.radius / max(abs(fragPos.z - sampleViewZ), 1e-4));
        occlusion += ((sampleViewZ >= samplePos.z + ubo.bias) ? 1.0 : 0.0) * rangeCheck;
    }

    outAO = vec4(vec3(1.0 - occlusion / float(n)), 1.0);
}
