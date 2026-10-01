#version 450

layout(location = 0) in vec3 fragColor;
layout(location = 1) flat in float fragSrgb;
layout(location = 0) out vec4 outColor;

void main() {
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    float d2 = dot(c, c);
    if (d2 > 1.0) discard;
    // soft sphere look: brighter at the centre, never darker than 70 %
    float shade = mix(0.7, 1.0, sqrt(1.0 - d2));
    vec3 rgb = fragColor * shade;
    if (fragSrgb > 0.5) rgb = pow(max(rgb, vec3(0.0)), vec3(1.0 / 2.2));
    outColor = vec4(rgb, 1.0);
}
