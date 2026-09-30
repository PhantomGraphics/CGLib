#version 450

// Opaque disc: a pixel is covered when its centre lies inside the circle inscribed in the point sprite.
layout(location = 0) in vec3 vColor;
layout(location = 0) out vec4 outColor;

void main()
{
    vec2 c = gl_PointCoord * 2.0 - 1.0;
    if (dot(c, c) > 1.0) discard;
    outColor = vec4(vColor, 1.0);
}
