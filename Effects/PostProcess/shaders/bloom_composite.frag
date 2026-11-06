#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UBO {
    float strength;
    float _pad[3];
} ubo;

layout(binding = 1) uniform sampler2D uScene;
layout(binding = 2) uniform sampler2D uBloom;

void main()
{
    vec4 scene = texture(uScene, inUV);
    scene.a = clamp(scene.a, 0.0, 1.0);
    vec3 bloom = texture(uBloom, inUV).rgb;
    // Bloom changes color within the input matte; it does not invent background coverage.
    outColor = vec4(scene.rgb + bloom * ubo.strength * scene.a, scene.a);
}
