#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in vec2 inLocalUV;

layout(location = 0) out vec4 outColor;

void main()
{
    vec2  centered = inLocalUV * 2.0 - 1.0;
    float falloff  = 1.0 - smoothstep(0.6, 1.0, length(centered));
    float alpha    = inColor.a * falloff;
    if (alpha <= 0.001)
        discard;
    outColor = vec4(inColor.rgb, alpha);
}
