#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec4 lightClipPos;

// Layout must match Crystal::Volume::PBVRPipeline::UBO (PBVRPipeline.h).
layout(binding = 0) uniform UBO {
    mat4 mvp;
    float particleSize;
    float _pad0;
    float _pad1;
    float _pad2;
    mat4 lightVP;
    float sigma;
    float layerCount;
    float shadowEnabled;
} ubo;

// Opacity Shadow Map (Crystal::Volume::OpacityShadowMapPass). Only bound/sampled when
// shadowEnabled != 0; unused otherwise (see PBVRPipeline::create()'s enableShadowSampler).
layout(binding = 1) uniform sampler2DArray shadowMap;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    if (dot(coord, coord) > 1.0) {
        discard;
    }

    vec3 rgb = fragColor.rgb;

    if (ubo.shadowEnabled > 0.5) {
        vec3 ndc = lightClipPos.xyz / lightClipPos.w;
        vec2 uv  = ndc.xy * 0.5 + 0.5;
        float t  = clamp(ndc.z, 0.0, 1.0);

        // Layer i holds cumulative density from the light down to depth (i+1)/layerCount
        // (see opacity_shadow_deposit.vert); depth 0 itself has no stored layer (density 0
        // there by construction), so index -1 is a virtual all-zero sample.
        float layerF = t * ubo.layerCount - 1.0;
        float lo     = floor(layerF);
        float frac   = clamp(layerF - lo, 0.0, 1.0);
        float hi     = clamp(lo + 1.0, 0.0, ubo.layerCount - 1.0);

        float densityLo = (lo < 0.0) ? 0.0 : texture(shadowMap, vec3(uv, lo)).r;
        float densityHi = texture(shadowMap, vec3(uv, hi)).r;
        float density   = mix(densityLo, densityHi, frac);

        float transmittance = exp(-ubo.sigma * density);
        const vec3 ambientColor = vec3(0.15, 0.15, 0.2);
        const vec3 sunColor     = vec3(1.0, 0.95, 0.85);
        rgb *= mix(ambientColor, sunColor, transmittance);
    }

    outColor = vec4(rgb, fragColor.a);
}
