#version 450

layout(location = 0) in float inDensity;
layout(location = 0) out float outColor;

void main() {
    vec2 coord = gl_PointCoord * 2.0 - 1.0;
    if (dot(coord, coord) > 1.0) {
        discard;
    }
    outColor = inDensity;
}
