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
    vec3 scene = texture(uScene, inUV).rgb;
    vec3 bloom = texture(uBloom, inUV).rgb;
    outColor = vec4(scene + bloom * ubo.strength, 1.0);
}
