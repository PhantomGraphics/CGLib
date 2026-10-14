#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UBO {
    float exposure;
    int   vignetteEnabled;
    float vignetteStrength;
    float _pad;
} ubo;

layout(binding = 1) uniform sampler2D uInput;

// ACES filmic fit (Krzysztof Narkowicz / Stephen Hill 2015 approximation).
vec3 acesFilm(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr = texture(uInput, inUV).rgb * ubo.exposure;
    vec3 mapped = acesFilm(hdr);
    mapped = pow(mapped, vec3(1.0 / 2.2));

    if (ubo.vignetteEnabled != 0) {
        float d = distance(inUV, vec2(0.5));
        float falloff = mix(1.0, 1.0 - smoothstep(0.25, 0.75, d), ubo.vignetteStrength);
        mapped *= falloff;
    }

    outColor = vec4(mapped, 1.0);
}
