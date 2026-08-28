#version 450

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(binding = 0) uniform UBO {
    float threshold;
    float _pad[3];
} ubo;

layout(binding = 1) uniform sampler2D uInput;

void main()
{
    vec3 c = texture(uInput, inUV).rgb;
    float luma = dot(c, vec3(0.2126, 0.7152, 0.0722));
    float weight = max(luma - ubo.threshold, 0.0) / max(luma, 1e-4);
    outColor = vec4(c * weight, 1.0);
}
