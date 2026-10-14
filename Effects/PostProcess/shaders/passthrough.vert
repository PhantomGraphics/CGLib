#version 450

// Fullscreen triangle, no vertex buffer: for vertexIndex 0,1,2 this generates UVs
// (0,0), (2,0), (0,2) and clip-space positions (-1,-1), (3,-1), (-1,3), which cover the
// whole viewport with a single triangle.
layout(location = 0) out vec2 outUV;

void main()
{
    outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(outUV * 2.0 - 1.0, 0.0, 1.0);
}
