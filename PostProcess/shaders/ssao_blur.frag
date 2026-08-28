#version 450

// 4x4 box blur (matches the 4x4 noise tile size) to remove the SSAO pass's per-pixel
// rotation-noise pattern, then multiplies the result onto the original scene color.

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UBO {
    vec2  texelSize;
    float strength;
    float _pad;
} ubo;

layout(binding = 1) uniform sampler2D uRawAO;
layout(binding = 2) uniform sampler2D uScene;

void main()
{
    float sum = 0.0;
    for (int x = -2; x < 2; ++x)
        for (int y = -2; y < 2; ++y)
            sum += texture(uRawAO, inUV + vec2(x, y) * ubo.texelSize).r;
    float ao = sum / 16.0;

    vec3 scene = texture(uScene, inUV).rgb;
    outColor = vec4(scene * mix(1.0, ao, ubo.strength), 1.0);
}
