#version 450
layout(std430,binding=0) readonly buffer State { vec2 cells[]; };
layout(std140,binding=1) uniform Uniforms {
    mat4 vp; mat4 inverseVP; mat4 reflectionVP;
    vec4 eye; vec4 domain; vec4 color; vec4 absorption; vec4 flags; uvec4 grid;
} u;
layout(location=0) out vec3 world;
layout(location=1) out vec3 normal;
void main() {
    uint quad=uint(gl_VertexIndex)/6, corner=uint(gl_VertexIndex)%6;
    const uvec2 offsets[6]=uvec2[](uvec2(0,0),uvec2(0,1),uvec2(1,0),uvec2(1,0),uvec2(0,1),uvec2(1,1));
    uvec2 xy=uvec2(quad%(u.grid.x-1),quad/(u.grid.x-1))+offsets[corner];
    uint i=xy.y*u.grid.x+xy.x;
    vec2 spacing=u.domain.xy/vec2(u.grid.xy-1);
    world=vec3(float(xy.x)*spacing.x,cells[i].x+u.domain.z,float(xy.y)*spacing.y);
    uint left=xy.y*u.grid.x+max(xy.x,1)-1, right=xy.y*u.grid.x+min(xy.x+1,u.grid.x-1);
    uint down=(max(xy.y,1)-1)*u.grid.x+xy.x, up=min(xy.y+1,u.grid.y-1)*u.grid.x+xy.x;
    normal=normalize(vec3(-(cells[right].x-cells[left].x)/(2*spacing.x),1,-(cells[up].x-cells[down].x)/(2*spacing.y)));
    gl_Position=u.vp*vec4(world,1);
}
