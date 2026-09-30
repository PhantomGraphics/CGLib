#version 450

// Ensemble average -> premultiplied (rgb = L, a = 1 - T), composited with ONE / ONE_MINUS_SRC_ALPHA.
layout(location = 0) in vec2 vNdc;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform sampler2D uAccum;

layout(push_constant) uniform Push {
    float invCount;
} pc;

void main()
{
    outColor = texelFetch(uAccum, ivec2(gl_FragCoord.xy), 0) * pc.invCount;
}
