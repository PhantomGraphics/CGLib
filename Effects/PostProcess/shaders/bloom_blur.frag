#version 450

// Single Kawase blur iteration. Sampled at 4 diagonal taps whose offset grows with the
// push-constant `offset` value across successive calls (see BloomEffect::apply()), which
// approximates a wide Gaussian blur in a handful of same-resolution passes.

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec2  texelSize;
    float offset;
    float _pad;
} pc;

layout(binding = 0) uniform sampler2D uInput;

void main()
{
    vec2 o = pc.texelSize * pc.offset;
    vec3 sum = texture(uInput, inUV + vec2( o.x,  o.y)).rgb
             + texture(uInput, inUV + vec2(-o.x,  o.y)).rgb
             + texture(uInput, inUV + vec2( o.x, -o.y)).rgb
             + texture(uInput, inUV + vec2(-o.x, -o.y)).rgb;
    outColor = vec4(sum * 0.25, 1.0);
}
